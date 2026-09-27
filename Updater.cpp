/**
 * @file Updater.cpp
 * @brief Automatic update from GitHub Releases (see Updater.h)
 *
 * manifest.json format (made by tools/make_manifest.py):
 * {
 *   "version": "2.1.0",
 *   "firmware": { "url": "...firmware.bin", "size": 1385156, "sha256": "..." },
 *   "files_base_url": "https://raw.githubusercontent.com/<repo>/<tag>/sd_files",
 *   "files": [ { "path": "/www/index.html", "size": 4210, "sha256": "...", "keep": false } ]
 * }
 * "keep": true marks a user file (e.g. quotes.json): it is downloaded only if missing.
 */

#include "Updater.h"
#include "Version.h"
#include "Display.h"
#include "BatteryManager.h"
#include "DisplayTask.h"
#include "Language.h"
#include <SD.h>
#include <Update.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <esp_http_client.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <esp_crt_bundle.h>
#include <esp_ota_ops.h>
#include <mbedtls/sha256.h>
#include <functional>
#include <vector>

volatile UpdateState updateState = UPDATE_IDLE;

static String lastStatus;  // Empty until the first check (see getUpdateStatusText)
static String updateNotice;       // See getUpdateNotice()
static bool sdWriteFailed = false;  // The last downloadToFile failed while writing to the SD card

// Staging area on the SD card
static const char* STAGING_DIR = "/upd";
static const char* PENDING_LIST = "/upd/pending.txt";  // Files to move into place
static const char* READY_MARKER = "/upd/ready";        // Present only when everything has been verified
// Release the SD files match: if it is the latest one, the periodic check
// does not compute the SHA-256 of every file again (~95 s)
static const char* SYNCED_RELEASE = "/sd_release.txt";
static const char* STAGED_RELEASE = "/upd/release.txt";  // Becomes SYNCED_RELEASE when the files are applied

// Persistent state (NVS) to recognize a firmware that did not start
static const char* PREFS_NS = "updater";
static const char* KEY_ATTEMPT = "attempt";  // Version just installed, not confirmed yet
static const char* KEY_BAD = "bad";          // Version that caused a rollback

static const size_t MAX_MANIFEST_SIZE = 65536;  // ~230 files, BMP icons included

// The Arduino core asks whether the firmware confirmation is postponed: yes,
// we do it with markFirmwareHealthy() after 60 seconds of running
extern "C" bool verifyRollbackLater() {
  return true;
}

String getUpdateNotice() {
  return updateNotice;
}

String getUpdateStatusText() {
  return lastStatus.length() ? lastStatus : String(TR("Nessun controllo eseguito", "No check yet"));
}

// ---------------------------------------------------------------------------
// Utilities
// ---------------------------------------------------------------------------

// Compares two "x.y.z" versions numerically: <0 if a<b, 0 if equal, >0 if a>b
static int compareVersions(const char* a, const char* b) {
  int va[3] = {0, 0, 0};
  int vb[3] = {0, 0, 0};
  sscanf(a, "%d.%d.%d", &va[0], &va[1], &va[2]);
  sscanf(b, "%d.%d.%d", &vb[0], &vb[1], &vb[2]);
  for (int i = 0; i < 3; i++) {
    if (va[i] != vb[i]) return va[i] < vb[i] ? -1 : 1;
  }
  return 0;
}

static String toHex(const uint8_t* data, size_t len) {
  static const char digits[] = "0123456789abcdef";
  String out;
  out.reserve(len * 2);
  for (size_t i = 0; i < len; i++) {
    out += digits[data[i] >> 4];
    out += digits[data[i] & 0x0f];
  }
  return out;
}

static String sha256OfFile(const String& path) {
  File f = SD.open(path, FILE_READ);
  if (!f) return "";
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);
  uint8_t buf[512];
  while (f.available()) {
    size_t n = f.read(buf, sizeof(buf));
    if (n == 0) break;
    mbedtls_sha256_update(&ctx, buf, n);
  }
  f.close();
  uint8_t digest[32];
  mbedtls_sha256_finish(&ctx, digest);
  mbedtls_sha256_free(&ctx);
  return toHex(digest, sizeof(digest));
}

// Creates the missing folders of a file path
static void ensureParentDirs(const String& filePath) {
  int slash = filePath.indexOf('/', 1);
  while (slash > 0) {
    String dir = filePath.substring(0, slash);
    if (!SD.exists(dir)) SD.mkdir(dir);
    slash = filePath.indexOf('/', slash + 1);
  }
}

// Deletes a folder and everything in it
static void removeTree(const String& dirPath) {
  File dir = SD.open(dirPath);
  if (!dir) return;
  if (!dir.isDirectory()) {
    dir.close();
    SD.remove(dirPath);
    return;
  }
  std::vector<String> files;
  std::vector<String> dirs;
  File entry = dir.openNextFile();
  while (entry) {
    String p = entry.path();
    if (entry.isDirectory()) dirs.push_back(p); else files.push_back(p);
    entry.close();
    entry = dir.openNextFile();
  }
  dir.close();
  for (const String& f : files) SD.remove(f);
  for (const String& d : dirs) removeTree(d);
  SD.rmdir(dirPath);
}

static String readTextFile(const char* path) {
  File f = SD.open(path, FILE_READ);
  if (!f) return "";
  String s = f.readString();
  f.close();
  return s;
}

static bool writeTextFile(const char* path, const String& text) {
  SD.remove(path);
  File f = SD.open(path, FILE_WRITE);
  if (!f) return false;
  size_t n = f.print(text);
  f.close();
  return n == text.length();
}

static bool sdAvailable() {
  return SD.cardType() != CARD_NONE;
}

// ---------------------------------------------------------------------------
// HTTPS download
// ---------------------------------------------------------------------------

typedef std::function<bool(const uint8_t*, int)> ChunkSink;

// Downloads a URL handing the data to sink in chunks. Follows redirects (GitHub
// sends release files to a CDN) and verifies the server certificate
// with the root certificate bundle included in ESP-IDF.
static esp_http_client_handle_t newHttpsClient(const String& url) {
  esp_http_client_config_t cfg = {};
  cfg.url = url.c_str();
  cfg.crt_bundle_attach = esp_crt_bundle_attach;
  cfg.timeout_ms = 20000;
  cfg.buffer_size = 4096;     // GitHub responses have long headers
  cfg.buffer_size_tx = 2048;  // The CDN's signed URLs are long
  cfg.user_agent = "AtmoVerse/" ATMOVERSE_VERSION;
  return esp_http_client_init(&cfg);
}

// Runs the request on the URL already set in the client, follows redirects and
// hands the data to sink. With keepOpen the connection stays open for the
// next request to the same server (no new TLS handshake).
static bool httpFetch(esp_http_client_handle_t client, const String& logUrl, const ChunkSink& sink,
                      int* statusOut, bool keepOpen) {
  if (statusOut) *statusOut = -1;
  bool ok = false;
  for (int redirects = 0; redirects <= 5; redirects++) {
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
      Serial.printf("[HTTPS] Connection failed (%s, errno %d, heap %u, max block %u): %s\n",
                    esp_err_to_name(err), esp_http_client_get_errno(client),
                    (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap(), logUrl.c_str());
      esp_http_client_close(client);
      break;
    }
    esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);
    if (statusOut) *statusOut = status;

    if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
      esp_http_client_flush_response(client, NULL);
      esp_http_client_set_redirection(client);
      esp_http_client_close(client);
      continue;
    }

    if (status != 200) {
      Serial.printf("[HTTPS] HTTP %d for %s\n", status, logUrl.c_str());
      esp_http_client_close(client);
      break;
    }

    uint8_t buf[1024];
    ok = true;
    while (true) {
      int n = esp_http_client_read(client, (char*)buf, sizeof(buf));
      if (n < 0) { ok = false; break; }
      if (n == 0) {
        ok = esp_http_client_is_complete_data_received(client);
        break;
      }
      if (!sink(buf, n)) { ok = false; break; }
    }
    if (!ok || !keepOpen) esp_http_client_close(client);
    break;
  }
  return ok;
}

// Downloads a URL with its own connection. Follows redirects (GitHub
// sends release files to a CDN) and verifies the server certificate
// with the root certificate bundle included in ESP-IDF.
static bool httpDownload(const String& url, const ChunkSink& sink, int* statusOut = nullptr) {
  // The URL without parameters in the logs: they may hold an API key
  int q = url.indexOf('?');
  String logUrl = (q >= 0) ? url.substring(0, q) : url;

  esp_http_client_handle_t client = newHttpsClient(url);
  if (!client) {
    if (statusOut) *statusOut = -1;
    return false;
  }
  bool ok = httpFetch(client, logUrl, sink, statusOut, false);
  esp_http_client_cleanup(client);
  return ok;
}

int httpsGet(const String& url, String& body, size_t maxLen) {
  body = "";
  int status = -1;
  httpDownload(url, [&](const uint8_t* data, int len) {
    if (body.length() + len > maxLen) return false;
    body.concat((const char*)data, len);
    return true;
  }, &status);
  return status;
}

// Downloads a file to the SD card checking its size and SHA-256; deletes it on error
// ---------------------------------------------------------------------------
// Progress shown on the display
// ---------------------------------------------------------------------------
// A full e-ink refresh takes ~4 s: the screen is updated every 10%
// and at most once every 20 s.
struct Progress {
  const char* phase = "";
  int filesDone = 0;
  int filesTotal = 0;
  uint64_t bytesDone = 0;
  uint64_t bytesTotal = 0;
  unsigned long start = 0;
  int shownPercent = -100;
  unsigned long shownAt = 0;
};
static Progress progress;

static void startProgress(const char* phase, int files, uint64_t bytes) {
  progress = Progress();
  progress.phase = phase;
  progress.filesTotal = files;
  progress.bytesTotal = bytes;
  progress.start = millis();
}

static void reportProgress(bool force = false) {
  int pct = progress.bytesTotal ? (int)(progress.bytesDone * 100 / progress.bytesTotal) : 0;
  unsigned long now = millis();
  if (!force && (pct < progress.shownPercent + 10 || now - progress.shownAt < 20000)) return;

  // Time left from the average speed, after at least 5 s of data
  int eta = -1;
  unsigned long elapsed = now - progress.start;
  if (progress.bytesDone > 0 && elapsed > 5000) {
    eta = (int)((double)elapsed * (progress.bytesTotal - progress.bytesDone) / progress.bytesDone / 1000.0);
  }
  progress.shownPercent = pct;
  progress.shownAt = now;
  Serial.printf("[UPDATE] %s: %d%% (file %d/%d, about %d s left)\n", progress.phase, pct,
                progress.filesDone, progress.filesTotal, eta);
  showUpdateProgress(progress.phase, progress.filesDone, progress.filesTotal, pct, eta);
}

// Connection for the SD files.
// raw.githubusercontent.com uses the Let's Encrypt chain "Root YR" -> ISRG Root X1:
// on the ESP32 the verification of the root's RSA-4096 signature fails
// (esp-x509-crt-bundle, error 0x4290), and verification cannot be turned off
// in ESP-IDF's precompiled HTTP client. For these files the connection is
// encrypted but the certificate is not verified: integrity comes from the
// SHA-256 of every file, read from the manifest downloaded from github.com
// with a verified certificate. A tampered file is discarded.
// Since 2026 the release firmware also comes from a CDN with the same chain
// (release-assets.githubusercontent.com): same solution, the firmware
// boots only if size and SHA-256 match the manifest.
struct FileSession {
  WiFiClientSecure tls;
  HTTPClient http;
  FileSession() {
    tls.setInsecure();
    http.setReuse(true);  // One connection for all the files (a single handshake)
    http.setTimeout(20000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);  // github.com -> CDN
    http.setUserAgent("AtmoVerse/" ATMOVERSE_VERSION);
  }
};

static bool sessionDownload(FileSession& s, const String& url, const ChunkSink& sink) {
  if (!s.http.begin(s.tls, url)) return false;
  int code = s.http.GET();
  if (code != 200) {
    Serial.printf("[HTTPS] HTTP %d for %s (heap %u, max block %u)\n", code, url.c_str(),
                  (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
    s.http.end();
    return false;
  }
  int remaining = s.http.getSize();  // -1 if the size is not given
  NetworkClient* stream = s.http.getStreamPtr();
  uint8_t buf[1024];
  unsigned long lastData = millis();
  bool ok = true;
  while (remaining != 0) {
    size_t avail = stream->available();
    if (avail) {
      int n = stream->readBytes(buf, min(avail, sizeof(buf)));
      if (n <= 0) continue;
      if (!sink(buf, n)) { ok = false; break; }
      if (remaining > 0) remaining -= n;
      lastData = millis();
    } else if (!s.http.connected()) {
      if (remaining > 0) ok = false;  // Closed before the end
      break;
    } else if (millis() - lastData > 20000) {
      ok = false;
      break;
    } else {
      delay(2);
    }
  }
  s.http.end();  // With setReuse the connection stays open for the next file
  return ok;
}

static bool downloadToFile(FileSession* session, const String& url, const String& dest,
                           size_t expectedSize, const String& expectedSha) {
  ensureParentDirs(dest);
  SD.remove(dest);
  sdWriteFailed = false;
  File f = SD.open(dest, FILE_WRITE);
  if (!f) {
    sdWriteFailed = true;
    return false;
  }

  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);
  size_t total = 0;

  ChunkSink sink = [&](const uint8_t* data, int len) {
    mbedtls_sha256_update(&ctx, data, len);
    total += len;
    if (f.write(data, len) != (size_t)len) {
      sdWriteFailed = true;
      return false;
    }
    return true;
  };
  bool ok = session ? sessionDownload(*session, url, sink) : httpDownload(url, sink);
  f.close();

  uint8_t digest[32];
  mbedtls_sha256_finish(&ctx, digest);
  mbedtls_sha256_free(&ctx);

  if (!ok || total != expectedSize || toHex(digest, 32) != expectedSha) {
    Serial.println("[UPDATE] Invalid file: " + url);
    SD.remove(dest);
    return false;
  }
  return true;
}

// Writes the firmware to the second flash slot; sets it as the boot slot
// only if size and SHA-256 match the manifest
static const int FIRMWARE_ATTEMPTS = 3;

static bool downloadFirmware(const String& url, size_t size, const String& expectedSha) {
  for (int attempt = 1; attempt <= FIRMWARE_ATTEMPTS; attempt++) {
    if (!Update.begin(size)) {
      Serial.println("[UPDATE] Not enough space for the firmware");
      return false;
    }

    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    size_t total = 0;
    progress.bytesDone = 0;

    // Connection without certificate verification (see FileSession): the SHA-256 is what counts
    FileSession* session = new FileSession();
    bool ok = sessionDownload(*session, url, [&](const uint8_t* data, int len) {
      mbedtls_sha256_update(&ctx, data, len);
      total += len;
      progress.bytesDone = total;
      reportProgress();
      return Update.write((uint8_t*)data, len) == (size_t)len;
    });
    session->tls.stop();
    delete session;

    uint8_t digest[32];
    mbedtls_sha256_finish(&ctx, digest);
    mbedtls_sha256_free(&ctx);

    if (ok && total == size && toHex(digest, 32) == expectedSha) {
      return Update.end();
    }
    Serial.printf("[UPDATE] Invalid firmware (attempt %d/%d, %u of %u bytes)\n", attempt,
                  FIRMWARE_ATTEMPTS, (unsigned)total, (unsigned)size);
    Update.abort();
    if (attempt < FIRMWARE_ATTEMPTS) delay(3000 * attempt);
  }
  Serial.println("[UPDATE] Invalid firmware, installation cancelled");
  return false;
}

// ---------------------------------------------------------------------------
// SD file update
// ---------------------------------------------------------------------------

// Moves the files prepared in /upd into place. Resumes where it stopped
// if an interruption halted it halfway (the files already moved are no longer in /upd).
static void applyPendingSdUpdate() {
  if (!SD.exists(READY_MARKER)) {
    // Download interrupted before the end: the partial files must not be used
    if (SD.exists(STAGING_DIR)) removeTree(STAGING_DIR);
    return;
  }

  String list = readTextFile(PENDING_LIST);
  int start = 0;
  int applied = 0;
  while (start < (int)list.length()) {
    int end = list.indexOf('\n', start);
    if (end < 0) end = list.length();
    String path = list.substring(start, end);
    path.trim();
    start = end + 1;
    if (path.length() == 0) continue;

    String staged = String(STAGING_DIR) + path;
    if (SD.exists(staged)) {
      ensureParentDirs(path);
      SD.remove(path);
      if (SD.rename(staged, path)) applied++;
    }
  }

  if (SD.exists(STAGED_RELEASE)) {
    SD.remove(SYNCED_RELEASE);
    SD.rename(STAGED_RELEASE, SYNCED_RELEASE);
  }
  removeTree(STAGING_DIR);
  Serial.printf("[UPDATE] SD files updated: %d\n", applied);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void initUpdater() {
  Preferences prefs;
  prefs.begin(PREFS_NS, false);
  String attempt = prefs.getString(KEY_ATTEMPT, "");
  if (attempt.length() > 0 && attempt != ATMOVERSE_VERSION) {
    // "attempt" had been installed, but another version is running:
    // the bootloader restored the previous one
    prefs.putString(KEY_BAD, attempt);
    prefs.remove(KEY_ATTEMPT);
    lastStatus = String(TR("La versione ", "Version ")) + attempt +
                 TR(" non si è avviata: ripristinata la " ATMOVERSE_VERSION,
                    " did not start: restored " ATMOVERSE_VERSION);
    Serial.println("[UPDATE] " + lastStatus);
  }
  prefs.end();

  if (sdAvailable()) {
    applyPendingSdUpdate();
  }
}

void markFirmwareHealthy() {
  esp_ota_mark_app_valid_cancel_rollback();

  Preferences prefs;
  prefs.begin(PREFS_NS, false);
  if (prefs.getString(KEY_ATTEMPT, "") == ATMOVERSE_VERSION) {
    prefs.remove(KEY_ATTEMPT);
    Serial.println("[UPDATE] Firmware " ATMOVERSE_VERSION " confirmed");
  }
  prefs.end();
}

// Error that needs action: dedicated screen (only the first time,
// not at every new attempt) and footer warning until it is solved
static void reportBlockingError(const String& title, const String& text, const String& notice) {
  bool firstTime = updateNotice != notice;
  updateNotice = notice;
  lastStatus = title + ": " + text;
  Serial.println("[UPDATE] " + lastStatus);
  if (firstTime) showUpdateError(title.c_str(), text.c_str());
}

static String formatMB(uint64_t bytes) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%.1f MB", bytes / 1048576.0);
  return String(buf);
}

// Downloads stay at 80 MHz. With the CPU at 240 MHz during downloads the
// HTTPS connections failed almost every time (HTTP -1, v2.1.8/2.1.9), while
// the same requests at 80 MHz worked.
static void beginDownloadPhase() {
  updateState = UPDATE_DOWNLOADING;
  reportProgress(true);
}

static void endDownloadPhase() {
  updateState = UPDATE_IDLE;
  updateDisplay();  // Back to the normal screen
}

static void finishWithError(const String& message) {
  Serial.println("[UPDATE] " + message);
  lastStatus = message;
  if (sdAvailable() && SD.exists(STAGING_DIR)) removeTree(STAGING_DIR);
  endDownloadPhase();  // Back to the main screen
  if (sdWriteFailed) {
    reportBlockingError(TR("Aggiornamento non riuscito", "Update failed"),
                        TR("Impossibile scrivere sulla scheda SD: spazio esaurito o scheda danneggiata. "
                           "Libera spazio o sostituisci la scheda: AtmoVerse riprova da solo.",
                           "Cannot write to the SD card: it is full or damaged. "
                           "Free some space or replace the card: AtmoVerse retries by itself."),
                        TR("SD piena o danneggiata: aggiornamento sospeso", "SD full or damaged: update paused"));
  }
}

// Manifest and work lists live on the SD card, not in RAM: the manifest describes
// ~250 files (35 KB), and converted all at once into a JSON document it exceeds
// the largest free memory block available with WiFi and TLS running.
static const char* MANIFEST_TMP = "/manifest.tmp";
static const char* NEEDED_LIST = "/upd/needed.txt";  // "path|size|sha256" per line

struct ManifestHeader {
  String version;
  String baseUrl;
  String fwUrl;
  size_t fwSize = 0;
  String fwSha;
  String filesDigest;  // Fingerprint of the SD file list (empty in old manifests)

  // Content of SYNCED_RELEASE after a sync: the files fingerprint if the
  // manifest has it, otherwise the version. With the fingerprint, a release
  // that changes only the firmware does not check the SD card again.
  String syncKey() const { return filesDigest.length() ? "files:" + filesDigest : version; }
};

// Reads only version, firmware and file address: the file list is skipped
// by the filter, without using memory
template <typename TInput>
static bool parseManifestHeader(TInput& input, ManifestHeader& h) {
  JsonDocument filter;
  filter["version"] = true;
  filter["files_base_url"] = true;
  filter["firmware"] = true;
  filter["files_digest"] = true;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, input, DeserializationOption::Filter(filter));
  if (err) {
    Serial.printf("[UPDATE] Cannot read the manifest: %s (largest free block %u bytes)\n",
                  err.c_str(), (unsigned)ESP.getMaxAllocHeap());
    return false;
  }
  h.version = doc["version"] | "";
  h.baseUrl = doc["files_base_url"] | "";
  h.fwUrl = doc["firmware"]["url"] | "";
  h.fwSize = doc["firmware"]["size"] | 0;
  h.fwSha = doc["firmware"]["sha256"] | "";
  h.filesDigest = doc["files_digest"] | "";
  return h.version.length() > 0;
}

// Walks the "files" list of the manifest saved on the SD card one entry at a time
// and writes the ones to download to NEEDED_LIST. Returns how many, -1 on error.
// On FAT every file takes at least one cluster: up to 32 KB even if small
static const uint64_t FAT_CLUSTER_MAX = 32768;

static int buildNeededList(uint64_t& totalBytes, uint64_t& diskBytes) {
  totalBytes = 0;
  diskBytes = 0;
  File manifest = SD.open(MANIFEST_TMP, FILE_READ);
  if (!manifest) return -1;
  File out = SD.open(NEEDED_LIST, FILE_WRITE);
  if (!out) {
    manifest.close();
    return -1;
  }

  int count = 0;
  if (manifest.find("\"files\":[")) {
    do {
      JsonDocument f;
      if (deserializeJson(f, manifest)) break;
      String path = f["path"] | "";
      String sha = f["sha256"] | "";
      size_t size = f["size"] | 0;
      bool keep = f["keep"] | false;

      // Absolute paths only, never going up a folder or touching the work area
      if (!path.startsWith("/") || path.indexOf("..") >= 0 || path.indexOf('|') >= 0 ||
          path.startsWith(STAGING_DIR) || sha.length() != 64) continue;

      File probe = SD.open(path, FILE_READ);
      if (probe) {
        size_t have = probe.size();
        probe.close();
        if (keep) continue;  // User file already present
        // Different size: surely to download, without reading the file again
        if (have == size && sha256OfFile(path) == sha) continue;  // Already up to date
      }
      out.printf("%s|%u|%s\n", path.c_str(), (unsigned)size, sha.c_str());
      count++;
      totalBytes += size;
      diskBytes += ((size + FAT_CLUSTER_MAX - 1) / FAT_CLUSTER_MAX + 1) * FAT_CLUSTER_MAX;
    } while (manifest.findUntil(",", "]"));
  }
  out.close();
  manifest.close();
  return count;
}

// Downloads to /upd the files listed in NEEDED_LIST, verifying them; records in
// PENDING_LIST the ones ready to move into place
static const int FILE_ATTEMPTS = 5;

static bool downloadNeededFiles(const String& baseUrl) {
  File list = SD.open(NEEDED_LIST, FILE_READ);
  if (!list) return false;
  File pending = SD.open(PENDING_LIST, FILE_WRITE);
  if (!pending) {
    list.close();
    return false;
  }

  // A single HTTPS connection for all the files (same server): one TLS
  // handshake instead of one per file, faster and with less memory fragmentation
  FileSession* session = new FileSession();

  bool ok = true;
  int downloaded = 0;
  while (list.available()) {
    String line = list.readStringUntil('\n');
    int a = line.indexOf('|');
    int b = line.indexOf('|', a + 1);
    if (a < 0 || b < 0) continue;
    String path = line.substring(0, a);
    size_t size = line.substring(a + 1, b).toInt();
    String sha = line.substring(b + 1);
    sha.trim();
    bool done = false;
    // The first TLS connection often fails because of fragmented memory:
    // 5 attempts with a growing wait (2, 4, 6, 8 s)
    for (int attempt = 1; attempt <= FILE_ATTEMPTS && !done; attempt++) {
      done = downloadToFile(session, baseUrl + path, String(STAGING_DIR) + path, size, sha);
      if (!done) {
        session->http.end();
        session->tls.stop();  // The next attempt opens the connection again
        if (sdWriteFailed) break;  // SD card full or faulty: retrying does not help
        if (attempt < FILE_ATTEMPTS) delay(2000 * attempt);
      }
    }
    if (!done) {
      lastStatus = String(TR("Download non riuscito: ", "Download failed: ")) + path;
      ok = false;
      break;
    }
    pending.println(path);
    downloaded++;
    progress.filesDone = downloaded;
    progress.bytesDone += size;
    reportProgress();
  }
  session->tls.stop();
  delete session;
  pending.close();
  list.close();
  return ok;
}

// Latest release from the GitHub API (certificate verifiable with the ESP-IDF
// bundle): manifest URL and its SHA-256 (the asset's "digest")
static bool fetchReleaseInfo(String& manifestUrl, String& manifestSha) {
  String body;
  int status = httpsGet(ATMOVERSE_RELEASE_API_URL, body, 32768);
  if (status != 200) {
    Serial.printf("[UPDATE] GitHub API: HTTP %d\n", status);
    return false;
  }
  JsonDocument filter;
  filter["assets"][0]["name"] = true;
  filter["assets"][0]["digest"] = true;
  filter["assets"][0]["browser_download_url"] = true;
  JsonDocument doc;
  if (deserializeJson(doc, body, DeserializationOption::Filter(filter))) return false;
  for (JsonObject a : doc["assets"].as<JsonArray>()) {
    if (strcmp(a["name"] | "", "manifest.json") != 0) continue;
    String digest = a["digest"] | "";
    manifestUrl = a["browser_download_url"] | "";
    if (!digest.startsWith("sha256:") || manifestUrl.length() == 0) return false;
    manifestSha = digest.substring(7);
    return manifestSha.length() == 64;
  }
  Serial.println("[UPDATE] GitHub API: manifest.json not found in the release");
  return false;
}

bool checkForUpdates(bool fullScan) {
  Serial.println("[UPDATE] Checking for updates...");
  bool sd = sdAvailable();

  // 1. Manifest of the latest release: on the SD card if present, otherwise in RAM
  //    (without an SD card only the firmware data is needed)
  ManifestHeader h;
  bool downloaded;
  bool parsed = false;
  // The manifest URL and SHA-256 come from the GitHub API, with a verified
  // certificate; the manifest is then downloaded from the CDN (see
  // FileSession) and is accepted only if its SHA-256 matches
  String manifestUrl, manifestSha;
  bool infoOk = false;
  for (int attempt = 1; attempt <= 3 && !infoOk; attempt++) {
    if (attempt > 1) delay(3000);
    infoOk = fetchReleaseInfo(manifestUrl, manifestSha);
  }
  if (!infoOk) {
    lastStatus = TR("Server degli aggiornamenti non raggiungibile", "Update server not reachable");
    Serial.println("[UPDATE] " + lastStatus);
    return false;
  }

  String text;
  File f;
  downloaded = false;
  for (int attempt = 1; attempt <= 3 && !downloaded; attempt++) {
    if (attempt > 1) delay(3000);
    if (sd) {
      SD.remove(MANIFEST_TMP);
      f = SD.open(MANIFEST_TMP, FILE_WRITE);
      if (!f) break;
    } else {
      text = "";
    }
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    FileSession* session = new FileSession();
    bool ok = sessionDownload(*session, manifestUrl, [&](const uint8_t* data, int len) {
      mbedtls_sha256_update(&ctx, data, len);
      if (sd) return f.write(data, len) == (size_t)len;
      if (text.length() + len > MAX_MANIFEST_SIZE) return false;
      text.concat((const char*)data, len);
      return true;
    });
    session->tls.stop();
    delete session;
    if (sd) f.close();
    uint8_t digest[32];
    mbedtls_sha256_finish(&ctx, digest);
    mbedtls_sha256_free(&ctx);
    if (ok && toHex(digest, 32) != manifestSha) {
      Serial.println("[UPDATE] Manifest SHA-256 differs from the one declared by GitHub: discarded");
      ok = false;
    }
    downloaded = ok;
  }

  if (!downloaded) {
    lastStatus = TR("Server degli aggiornamenti non raggiungibile", "Update server not reachable");
    Serial.println("[UPDATE] " + lastStatus);
    if (sd) SD.remove(MANIFEST_TMP);
    return false;
  }
  if (sd) {
    File in = SD.open(MANIFEST_TMP, FILE_READ);
    parsed = in && parseManifestHeader(in, h);
    if (in) in.close();
  } else {
    parsed = parseManifestHeader(text, h);
  }
  if (!parsed) {
    lastStatus = TR("Manifest non valido", "Invalid manifest");
    Serial.println("[UPDATE] " + lastStatus);
    if (sd) SD.remove(MANIFEST_TMP);
    return false;
  }

  // 2. Is a new firmware needed? (skips a version that already failed to boot)
  Preferences prefs;
  prefs.begin(PREFS_NS, true);
  String badVersion = prefs.getString(KEY_BAD, "");
  prefs.end();

  bool firmwareNewer = compareVersions(h.version.c_str(), ATMOVERSE_VERSION) > 0 && badVersion != h.version;
  bool firmwarePostponed = false;

  // With a low battery and no charger the firmware is not installed: a power
  // loss while writing would be recovered by the rollback, but it is better
  // not to take the risk. The SD files are updated anyway.
  if (firmwareNewer && battery.isAvailable() && !battery.charging() &&
      battery.getPercentage() < BATTERY_MIN_FIRMWARE_UPDATE_PERCENT) {
    Serial.printf("[UPDATE] Firmware %s postponed: battery at %d%%, not charging\n",
                  h.version.c_str(), battery.getPercentage());
    firmwareNewer = false;
    firmwarePostponed = true;
    lastStatus = "Firmware " + h.version +
                 TR(" disponibile: si installa con la batteria sopra il ", " available: it installs with the battery above ") +
                 String(BATTERY_MIN_FIRMWARE_UPDATE_PERCENT) + TR("% o in carica", "% or while charging");
  }

  // 3. Which SD files changed? (list written to the SD card, not to RAM)
  int neededCount = 0;
  uint64_t neededBytes = 0;
  uint64_t neededDisk = 0;
  String synced = sd ? readTextFile(SYNCED_RELEASE) : String();
  synced.trim();
  bool filesInSync = !fullScan && synced.length() > 0 && synced == h.syncKey();
  if (filesInSync) {
    Serial.println("[UPDATE] SD files already match (" + h.version + "): check skipped");
  } else if (sd && h.baseUrl.length() > 0) {
    if (SD.exists(STAGING_DIR)) removeTree(STAGING_DIR);
    SD.mkdir(STAGING_DIR);
    neededCount = buildNeededList(neededBytes, neededDisk);
    if (neededCount < 0) {
      SD.remove(MANIFEST_TMP);
      removeTree(STAGING_DIR);
      reportBlockingError(TR("Aggiornamento non riuscito", "Update failed"),
                          TR("Impossibile scrivere sulla scheda SD: spazio esaurito o scheda danneggiata.",
                             "Cannot write to the SD card: it is full or damaged."),
                          TR("SD piena o danneggiata: aggiornamento sospeso", "SD full or damaged: update paused"));
      return false;
    }
  }
  if (sd) SD.remove(MANIFEST_TMP);

  // Space: the new files stay in /upd next to the old ones until they are all
  // verified, so there must be room for the whole copy plus a margin
  if (neededCount > 0) {
    uint64_t freeBytes = SD.totalBytes() - SD.usedBytes();
    uint64_t required = neededDisk + 512 * 1024;
    if (freeBytes < required) {
      removeTree(STAGING_DIR);
      reportBlockingError(TR("Spazio insufficiente sulla SD", "Not enough space on the SD card"),
                          String(TR("Per l'aggiornamento servono ", "The update needs ")) + formatMB(required) +
                          TR(", liberi ", ", free ") + formatMB(freeBytes) +
                          TR(". Libera spazio sulla scheda: AtmoVerse riprova da solo.",
                             ". Free some space on the card: AtmoVerse retries by itself."),
                          String(TR("SD piena: servono ", "SD full: needs ")) + formatMB(required) +
                          TR(", liberi ", ", free ") + formatMB(freeBytes));
      return false;
    }
  }

  // No file to download: the SD card matches this release
  if (sd && neededCount == 0 && !filesInSync && h.baseUrl.length() > 0) {
    writeTextFile(SYNCED_RELEASE, h.syncKey());
  }

  if (!firmwareNewer && neededCount == 0) {
    if (sd && SD.exists(STAGING_DIR)) removeTree(STAGING_DIR);
    updateNotice = "";
    // With a postponed firmware the status keeps saying why it was not installed
    if (!firmwarePostponed) {
      lastStatus = String(TR("Aggiornato (versione ", "Up to date (version ")) + ATMOVERSE_VERSION + ")";
    }
    Serial.println("[UPDATE] " + lastStatus);
    return true;
  }

  // 4. Download: the display shows the update screen
  Serial.printf("[UPDATE] To download: firmware %s, %d files\n", firmwareNewer ? h.version.c_str() : "no", neededCount);
  if (neededCount > 0) {
    startProgress(TR("File della SD", "SD card files"), neededCount, neededBytes);
  } else {
    startProgress(TR("Nuovo firmware", "New firmware"), 0, h.fwSize);
  }
  beginDownloadPhase();

  if (neededCount > 0 && !downloadNeededFiles(h.baseUrl)) {
    finishWithError(lastStatus);
    return false;
  }

  if (firmwareNewer) {
    if (neededCount > 0) {
      startProgress(TR("Nuovo firmware", "New firmware"), 0, h.fwSize);
      reportProgress(true);
    }
    if (h.fwUrl.length() == 0 || h.fwSize == 0 || !downloadFirmware(h.fwUrl, h.fwSize, h.fwSha)) {
      finishWithError(String(TR("Installazione del firmware non riuscita: ", "Firmware installation failed: ")) + h.version);
      return false;
    }

    // To be confirmed after the restart; if it does not boot, initUpdater() marks it as faulty
    prefs.begin(PREFS_NS, false);
    prefs.putString(KEY_ATTEMPT, h.version);
    prefs.end();
  }

  // 5. Everything verified: the files are applied now (or at boot, if interrupted)
  if (neededCount > 0) {
    writeTextFile(STAGED_RELEASE, h.syncKey());
    writeTextFile(READY_MARKER, "1");
  }

  if (firmwareNewer) {
    updateNotice = "";
    lastStatus = String(TR("Installata la versione ", "Installed version ")) + h.version + TR(", riavvio", ", restarting");
    Serial.println("[UPDATE] " + lastStatus);
    delay(500);
    ESP.restart();  // The SD files are applied at boot by initUpdater()
  }

  applyPendingSdUpdate();
  updateNotice = "";
  lastStatus = TR("File della SD aggiornati", "SD card files updated");
  Serial.printf("[UPDATE] %d SD files updated\n", neededCount);
  endDownloadPhase();
  return true;
}
