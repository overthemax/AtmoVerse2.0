/**
 * @file Updater.cpp
 * @brief Aggiornamento automatico da GitHub Releases (vedi Updater.h)
 *
 * Formato di manifest.json (generato da tools/make_manifest.py):
 * {
 *   "version": "2.1.0",
 *   "firmware": { "url": "...firmware.bin", "size": 1385156, "sha256": "..." },
 *   "files_base_url": "https://raw.githubusercontent.com/<repo>/<tag>/sd_files",
 *   "files": [ { "path": "/www/index.html", "size": 4210, "sha256": "...", "keep": false } ]
 * }
 * "keep": true indica un file dell'utente (es. quotes.json): viene scaricato solo se manca.
 */

#include "Updater.h"
#include "Version.h"
#include "Display.h"
#include <SD.h>
#include <Update.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include <esp_ota_ops.h>
#include <mbedtls/sha256.h>
#include <functional>
#include <vector>

volatile UpdateState updateState = UPDATE_IDLE;

static String lastStatus = "Nessun controllo eseguito";

// Area di preparazione sulla SD
static const char* STAGING_DIR = "/upd";
static const char* PENDING_LIST = "/upd/pending.txt";  // File da spostare al loro posto
static const char* READY_MARKER = "/upd/ready";        // Presente solo se tutto è stato verificato

// Stato persistente (NVS) per riconoscere un firmware che non si è avviato
static const char* PREFS_NS = "updater";
static const char* KEY_ATTEMPT = "attempt";  // Versione appena installata, non ancora confermata
static const char* KEY_BAD = "bad";          // Versione che ha causato un rollback

static const size_t MAX_MANIFEST_SIZE = 65536;  // ~230 file, icone BMP comprese

// Il core Arduino chiede se la conferma del firmware va rimandata: sì, la
// facciamo noi con markFirmwareHealthy() dopo 60 secondi di funzionamento
extern "C" bool verifyRollbackLater() {
  return true;
}

String getUpdateStatusText() {
  return lastStatus;
}

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------

// Confronta due versioni "x.y.z" numericamente: <0 se a<b, 0 se uguali, >0 se a>b
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

// Crea le cartelle mancanti del percorso di un file
static void ensureParentDirs(const String& filePath) {
  int slash = filePath.indexOf('/', 1);
  while (slash > 0) {
    String dir = filePath.substring(0, slash);
    if (!SD.exists(dir)) SD.mkdir(dir);
    slash = filePath.indexOf('/', slash + 1);
  }
}

// Cancella una cartella e tutto il suo contenuto
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
// Download HTTPS
// ---------------------------------------------------------------------------

typedef std::function<bool(const uint8_t*, int)> ChunkSink;

// Scarica un URL passando i dati a sink a blocchi. Segue i redirect (GitHub
// rimanda i file delle release a un CDN) e verifica il certificato del server
// con il bundle di certificati radice incluso in ESP-IDF.
static bool httpDownload(const String& url, const ChunkSink& sink, int* statusOut = nullptr) {
  // Nei log l'URL senza parametri: potrebbero contenere una API key
  int q = url.indexOf('?');
  String logUrl = (q >= 0) ? url.substring(0, q) : url;
  if (statusOut) *statusOut = -1;

  esp_http_client_config_t cfg = {};
  cfg.url = url.c_str();
  cfg.crt_bundle_attach = esp_crt_bundle_attach;
  cfg.timeout_ms = 20000;
  cfg.buffer_size = 4096;     // Le risposte di GitHub hanno intestazioni lunghe
  cfg.buffer_size_tx = 2048;  // Gli URL firmati del CDN sono lunghi
  cfg.user_agent = "AtmoVerse/" ATMOVERSE_VERSION;

  esp_http_client_handle_t client = esp_http_client_init(&cfg);
  if (!client) return false;

  bool ok = false;
  for (int redirects = 0; redirects <= 5; redirects++) {
    if (esp_http_client_open(client, 0) != ESP_OK) {
      Serial.println("[HTTPS] Connessione non riuscita: " + logUrl);
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
      Serial.printf("[HTTPS] HTTP %d per %s\n", status, logUrl.c_str());
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
    esp_http_client_close(client);
    break;
  }

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

// Scarica un file sulla SD verificandone dimensione e SHA-256; in caso di errore lo cancella
static bool downloadToFile(const String& url, const String& dest, size_t expectedSize, const String& expectedSha) {
  ensureParentDirs(dest);
  SD.remove(dest);
  File f = SD.open(dest, FILE_WRITE);
  if (!f) return false;

  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);
  size_t total = 0;

  bool ok = httpDownload(url, [&](const uint8_t* data, int len) {
    mbedtls_sha256_update(&ctx, data, len);
    total += len;
    return f.write(data, len) == (size_t)len;
  });
  f.close();

  uint8_t digest[32];
  mbedtls_sha256_finish(&ctx, digest);
  mbedtls_sha256_free(&ctx);

  if (!ok || total != expectedSize || toHex(digest, 32) != expectedSha) {
    Serial.println("[UPDATE] File non valido: " + url);
    SD.remove(dest);
    return false;
  }
  return true;
}

// Scrive il firmware nella seconda area del flash; la imposta come area di
// avvio solo se dimensione e SHA-256 corrispondono al manifest
static bool downloadFirmware(const String& url, size_t size, const String& expectedSha) {
  if (!Update.begin(size)) {
    Serial.println("[UPDATE] Spazio insufficiente per il firmware");
    return false;
  }

  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);
  size_t total = 0;

  bool ok = httpDownload(url, [&](const uint8_t* data, int len) {
    mbedtls_sha256_update(&ctx, data, len);
    total += len;
    return Update.write((uint8_t*)data, len) == (size_t)len;
  });

  uint8_t digest[32];
  mbedtls_sha256_finish(&ctx, digest);
  mbedtls_sha256_free(&ctx);

  if (!ok || total != size || toHex(digest, 32) != expectedSha) {
    Serial.println("[UPDATE] Firmware non valido, installazione annullata");
    Update.abort();
    return false;
  }
  return Update.end();
}

// ---------------------------------------------------------------------------
// Aggiornamento dei file della SD
// ---------------------------------------------------------------------------

// Sposta al loro posto i file preparati in /upd. Riprende da dove era rimasto
// se un'interruzione lo ha fermato a metà (i file già spostati non sono più in /upd).
static void applyPendingSdUpdate() {
  if (!SD.exists(READY_MARKER)) {
    // Download interrotto prima della fine: i file parziali non vanno usati
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

  removeTree(STAGING_DIR);
  Serial.printf("[UPDATE] File della SD aggiornati: %d\n", applied);
}

// ---------------------------------------------------------------------------
// API pubblica
// ---------------------------------------------------------------------------

void initUpdater() {
  Preferences prefs;
  prefs.begin(PREFS_NS, false);
  String attempt = prefs.getString(KEY_ATTEMPT, "");
  if (attempt.length() > 0 && attempt != ATMOVERSE_VERSION) {
    // Era stata installata "attempt", ma è in esecuzione un'altra versione:
    // il bootloader ha ripristinato quella precedente
    prefs.putString(KEY_BAD, attempt);
    prefs.remove(KEY_ATTEMPT);
    lastStatus = "La versione " + attempt + " non si è avviata: ripristinata la " ATMOVERSE_VERSION;
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
    Serial.println("[UPDATE] Firmware " ATMOVERSE_VERSION " confermato");
  }
  prefs.end();
}

// Durante i download la CPU passa da 80 a 240 MHz: le connessioni HTTPS
// sono molto più rapide. Al termine si torna alla frequenza di risparmio.
static uint32_t savedCpuMhz = 0;

static void beginDownloadPhase() {
  updateState = UPDATE_DOWNLOADING;
  savedCpuMhz = getCpuFrequencyMhz();
  setCpuFrequencyMhz(240);
  showUpdateScreen();
}

static void endDownloadPhase() {
  updateState = UPDATE_IDLE;
  if (savedCpuMhz > 0) setCpuFrequencyMhz(savedCpuMhz);
  updateDisplay();  // Torna alla schermata normale
}

static void finishWithError(const String& message) {
  Serial.println("[UPDATE] " + message);
  lastStatus = message;
  if (sdAvailable() && SD.exists(STAGING_DIR)) removeTree(STAGING_DIR);
  endDownloadPhase();
}

bool checkForUpdates() {
  Serial.println("[UPDATE] Controllo aggiornamenti...");

  // 1. Manifest dell'ultima release
  String manifestText;
  bool ok = httpDownload(ATMOVERSE_UPDATE_MANIFEST_URL, [&](const uint8_t* data, int len) {
    if (manifestText.length() + len > MAX_MANIFEST_SIZE) return false;
    manifestText.concat((const char*)data, len);
    return true;
  });
  if (!ok) {
    lastStatus = "Server degli aggiornamenti non raggiungibile";
    Serial.println("[UPDATE] " + lastStatus);
    return false;
  }

  JsonDocument manifest;
  if (deserializeJson(manifest, manifestText)) {
    lastStatus = "Manifest non valido";
    Serial.println("[UPDATE] " + lastStatus);
    return false;
  }
  manifestText = String();  // Libera memoria

  const char* version = manifest["version"] | "";

  // 2. Serve un nuovo firmware? (salta una versione che ha già fallito l'avvio)
  Preferences prefs;
  prefs.begin(PREFS_NS, true);
  String badVersion = prefs.getString(KEY_BAD, "");
  prefs.end();

  bool firmwareNewer = strlen(version) > 0 &&
                       compareVersions(version, ATMOVERSE_VERSION) > 0 &&
                       badVersion != version;

  // 3. Quali file della SD sono cambiati?
  struct NeededFile { String path; size_t size; String sha; };
  std::vector<NeededFile> needed;
  String baseUrl = manifest["files_base_url"] | "";

  if (sdAvailable() && baseUrl.length() > 0) {
    for (JsonObject f : manifest["files"].as<JsonArray>()) {
      String path = f["path"] | "";
      String sha = f["sha256"] | "";
      size_t size = f["size"] | 0;
      bool keep = f["keep"] | false;

      // Solo percorsi assoluti, senza risalire di cartella
      if (!path.startsWith("/") || path.indexOf("..") >= 0 || path.startsWith(STAGING_DIR)) continue;

      if (SD.exists(path)) {
        if (keep) continue;                      // File dell'utente già presente
        if (sha256OfFile(path) == sha) continue; // Già aggiornato
      }
      needed.push_back({path, size, sha});
    }
  }

  if (!firmwareNewer && needed.empty()) {
    lastStatus = String("Aggiornato (versione ") + ATMOVERSE_VERSION + ")";
    Serial.println("[UPDATE] " + lastStatus);
    return true;
  }

  // 4. Download: il display mostra la schermata di aggiornamento
  Serial.printf("[UPDATE] Da scaricare: firmware %s, file %d\n", firmwareNewer ? version : "no", (int)needed.size());
  beginDownloadPhase();

  if (!needed.empty()) {
    if (SD.exists(STAGING_DIR)) removeTree(STAGING_DIR);
    SD.mkdir(STAGING_DIR);

    String pending;
    for (const NeededFile& f : needed) {
      if (!downloadToFile(baseUrl + f.path, String(STAGING_DIR) + f.path, f.size, f.sha)) {
        finishWithError("Download non riuscito: " + f.path);
        return false;
      }
      pending += f.path + "\n";
    }
    if (!writeTextFile(PENDING_LIST, pending)) {
      finishWithError("Impossibile scrivere sulla SD");
      return false;
    }
  }

  if (firmwareNewer) {
    String fwUrl = manifest["firmware"]["url"] | "";
    size_t fwSize = manifest["firmware"]["size"] | 0;
    String fwSha = manifest["firmware"]["sha256"] | "";
    if (fwUrl.length() == 0 || fwSize == 0 || !downloadFirmware(fwUrl, fwSize, fwSha)) {
      finishWithError(String("Installazione del firmware ") + version + " non riuscita");
      return false;
    }

    // Da confermare dopo il riavvio; se non parte, initUpdater() lo segnerà come difettoso
    prefs.begin(PREFS_NS, false);
    prefs.putString(KEY_ATTEMPT, version);
    prefs.end();
  }

  // 5. Tutto verificato: i file vengono applicati ora (o al riavvio, se interrotti)
  if (!needed.empty()) {
    writeTextFile(READY_MARKER, "1");
  }

  if (firmwareNewer) {
    lastStatus = String("Installata la versione ") + version + ", riavvio";
    Serial.println("[UPDATE] " + lastStatus);
    delay(500);
    ESP.restart();  // I file della SD vengono applicati all'avvio da initUpdater()
  }

  applyPendingSdUpdate();
  lastStatus = "File della SD aggiornati";
  endDownloadPhase();
  return true;
}
