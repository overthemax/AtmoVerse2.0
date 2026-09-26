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
#include "BatteryManager.h"
#include "DisplayTask.h"
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

static String lastStatus = "Nessun controllo eseguito";
static String updateNotice;       // Vedi getUpdateNotice()
static bool sdWriteFailed = false;  // L'ultimo downloadToFile è fallito scrivendo sulla SD

// Area di preparazione sulla SD
static const char* STAGING_DIR = "/upd";
static const char* PENDING_LIST = "/upd/pending.txt";  // File da spostare al loro posto
static const char* READY_MARKER = "/upd/ready";        // Presente solo se tutto è stato verificato
// Release con cui i file della SD sono allineati: se coincide con l'ultima, il
// controllo periodico non ricalcola lo SHA-256 di tutti i file (~95 s)
static const char* SYNCED_RELEASE = "/sd_release.txt";
static const char* STAGED_RELEASE = "/upd/release.txt";  // Diventa SYNCED_RELEASE quando i file sono applicati

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

String getUpdateNotice() {
  return updateNotice;
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
static esp_http_client_handle_t newHttpsClient(const String& url) {
  esp_http_client_config_t cfg = {};
  cfg.url = url.c_str();
  cfg.crt_bundle_attach = esp_crt_bundle_attach;
  cfg.timeout_ms = 20000;
  cfg.buffer_size = 4096;     // Le risposte di GitHub hanno intestazioni lunghe
  cfg.buffer_size_tx = 2048;  // Gli URL firmati del CDN sono lunghi
  cfg.user_agent = "AtmoVerse/" ATMOVERSE_VERSION;
  return esp_http_client_init(&cfg);
}

// Esegue la richiesta sull'URL già impostato nel client, segue i redirect e
// passa i dati a sink. Con keepOpen la connessione resta aperta per la
// richiesta successiva allo stesso server (niente nuovo handshake TLS).
static bool httpFetch(esp_http_client_handle_t client, const String& logUrl, const ChunkSink& sink,
                      int* statusOut, bool keepOpen) {
  if (statusOut) *statusOut = -1;
  bool ok = false;
  for (int redirects = 0; redirects <= 5; redirects++) {
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
      Serial.printf("[HTTPS] Connessione non riuscita (%s, errno %d, heap %u, blocco max %u): %s\n",
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
    if (!ok || !keepOpen) esp_http_client_close(client);
    break;
  }
  return ok;
}

// Scarica un URL con una connessione dedicata. Segue i redirect (GitHub
// rimanda i file delle release a un CDN) e verifica il certificato del server
// con il bundle di certificati radice incluso in ESP-IDF.
static bool httpDownload(const String& url, const ChunkSink& sink, int* statusOut = nullptr) {
  // Nei log l'URL senza parametri: potrebbero contenere una API key
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

// Scarica un file sulla SD verificandone dimensione e SHA-256; in caso di errore lo cancella
// ---------------------------------------------------------------------------
// Avanzamento mostrato sul display
// ---------------------------------------------------------------------------
// Un refresh completo dell'e-ink dura ~4 s: la schermata si aggiorna a ogni 10%
// e non più di una volta ogni 20 s.
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

  // Tempo stimato dalla velocità media, dopo almeno 5 s di dati
  int eta = -1;
  unsigned long elapsed = now - progress.start;
  if (progress.bytesDone > 0 && elapsed > 5000) {
    eta = (int)((double)elapsed * (progress.bytesTotal - progress.bytesDone) / progress.bytesDone / 1000.0);
  }
  progress.shownPercent = pct;
  progress.shownAt = now;
  Serial.printf("[UPDATE] %s: %d%% (file %d/%d, stima %d s)\n", progress.phase, pct,
                progress.filesDone, progress.filesTotal, eta);
  showUpdateProgress(progress.phase, progress.filesDone, progress.filesTotal, pct, eta);
}

// Connessione per i file della SD.
// raw.githubusercontent.com usa la catena Let's Encrypt "Root YR" -> ISRG Root X1:
// sull'ESP32 la verifica della firma RSA-4096 della radice fallisce
// (esp-x509-crt-bundle, errore 0x4290), e la verifica non si può disattivare
// nel client HTTP di ESP-IDF precompilato. Per questi file la connessione è
// cifrata ma senza verifica del certificato: l'integrità è garantita dallo
// SHA-256 di ogni file, letto dal manifest scaricato da github.com con
// certificato verificato. Un file alterato viene scartato.
struct FileSession {
  WiFiClientSecure tls;
  HTTPClient http;
  FileSession() {
    tls.setInsecure();
    http.setReuse(true);  // Stessa connessione per tutti i file (un solo handshake)
    http.setTimeout(20000);
  }
};

static bool sessionDownload(FileSession& s, const String& url, const ChunkSink& sink) {
  if (!s.http.begin(s.tls, url)) return false;
  int code = s.http.GET();
  if (code != 200) {
    Serial.printf("[HTTPS] HTTP %d per %s (heap %u, blocco max %u)\n", code, url.c_str(),
                  (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
    s.http.end();
    return false;
  }
  int remaining = s.http.getSize();  // -1 se la dimensione non è indicata
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
      if (remaining > 0) ok = false;  // Chiuso prima della fine
      break;
    } else if (millis() - lastData > 20000) {
      ok = false;
      break;
    } else {
      delay(2);
    }
  }
  s.http.end();  // Con setReuse la connessione resta aperta per il file successivo
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
    progress.bytesDone = total;
    reportProgress();
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

  if (SD.exists(STAGED_RELEASE)) {
    SD.remove(SYNCED_RELEASE);
    SD.rename(STAGED_RELEASE, SYNCED_RELEASE);
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

// Errore che richiede un intervento: schermata dedicata (solo la prima volta,
// non a ogni nuovo tentativo) e avviso nel piè di pagina finché non si risolve
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

// Durante i download la CPU passa da 80 a 240 MHz: le connessioni HTTPS
// sono molto più rapide. Al termine si torna alla frequenza di risparmio.
static uint32_t savedCpuMhz = 0;

static void beginDownloadPhase() {
  updateState = UPDATE_DOWNLOADING;
  reportProgress(true);
  // Il cambio di frequenza vale per entrambi i core: si attende che il task
  // del display abbia finito di trasmettere al pannello via SPI
  waitDisplayIdle(15000);
  savedCpuMhz = getCpuFrequencyMhz();
  setCpuFrequencyMhz(240);
}

static void endDownloadPhase() {
  updateState = UPDATE_IDLE;
  // Qui il display è fermo (l'ultima richiesta è la schermata di aggiornamento)
  if (savedCpuMhz > 0) setCpuFrequencyMhz(savedCpuMhz);
  updateDisplay();  // Torna alla schermata normale
}

static void finishWithError(const String& message) {
  Serial.println("[UPDATE] " + message);
  lastStatus = message;
  if (sdAvailable() && SD.exists(STAGING_DIR)) removeTree(STAGING_DIR);
  endDownloadPhase();  // Torna alla schermata principale
  if (sdWriteFailed) {
    reportBlockingError("Aggiornamento non riuscito",
                        "Impossibile scrivere sulla scheda SD: spazio esaurito o scheda danneggiata. "
                        "Libera spazio o sostituisci la scheda: AtmoVerse riprova da solo.",
                        "SD piena o danneggiata: aggiornamento sospeso");
  }
}

// Manifest e liste di lavoro stanno sulla SD, non in RAM: il manifest descrive
// ~250 file (35 KB) e convertito tutto insieme in un documento JSON supera il
// blocco di memoria libera più grande disponibile con WiFi e TLS attivi.
static const char* MANIFEST_TMP = "/manifest.tmp";
static const char* NEEDED_LIST = "/upd/needed.txt";  // "percorso|dimensione|sha256" per riga

struct ManifestHeader {
  String version;
  String baseUrl;
  String fwUrl;
  size_t fwSize = 0;
  String fwSha;
};

// Legge solo versione, firmware e indirizzo dei file: l'elenco dei file viene
// saltato dal filtro, senza occupare memoria
template <typename TInput>
static bool parseManifestHeader(TInput& input, ManifestHeader& h) {
  JsonDocument filter;
  filter["version"] = true;
  filter["files_base_url"] = true;
  filter["firmware"] = true;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, input, DeserializationOption::Filter(filter));
  if (err) {
    Serial.printf("[UPDATE] Manifest non leggibile: %s (memoria libera max %u byte)\n",
                  err.c_str(), (unsigned)ESP.getMaxAllocHeap());
    return false;
  }
  h.version = doc["version"] | "";
  h.baseUrl = doc["files_base_url"] | "";
  h.fwUrl = doc["firmware"]["url"] | "";
  h.fwSize = doc["firmware"]["size"] | 0;
  h.fwSha = doc["firmware"]["sha256"] | "";
  return h.version.length() > 0;
}

// Scorre l'elenco "files" del manifest salvato sulla SD una voce alla volta e
// scrive in NEEDED_LIST quelle da scaricare. Restituisce quante sono, -1 se errore.
// Su FAT ogni file occupa almeno un cluster: fino a 32 KB anche se piccolo
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

      // Solo percorsi assoluti, senza risalire di cartella né toccare l'area di lavoro
      if (!path.startsWith("/") || path.indexOf("..") >= 0 || path.indexOf('|') >= 0 ||
          path.startsWith(STAGING_DIR) || sha.length() != 64) continue;

      if (SD.exists(path)) {
        if (keep) continue;                      // File dell'utente già presente
        if (sha256OfFile(path) == sha) continue; // Già aggiornato
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

// Scarica in /upd i file elencati in NEEDED_LIST, verificandoli; annota in
// PENDING_LIST quelli pronti da spostare al loro posto
static const int FILE_ATTEMPTS = 5;

static bool downloadNeededFiles(const String& baseUrl) {
  File list = SD.open(NEEDED_LIST, FILE_READ);
  if (!list) return false;
  File pending = SD.open(PENDING_LIST, FILE_WRITE);
  if (!pending) {
    list.close();
    return false;
  }

  // Una sola connessione HTTPS per tutti i file (stesso server): un handshake
  // TLS invece di uno per file, più veloce e con meno frammentazione della memoria
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
    // La prima connessione TLS fallisce spesso per memoria frammentata:
    // 5 tentativi con attesa crescente (2, 4, 6, 8 s)
    for (int attempt = 1; attempt <= FILE_ATTEMPTS && !done; attempt++) {
      done = downloadToFile(session, baseUrl + path, String(STAGING_DIR) + path, size, sha);
      if (!done) {
        session->http.end();
        session->tls.stop();  // Il tentativo successivo riapre la connessione
        if (sdWriteFailed) break;  // SD piena o guasta: riprovare non serve
        if (attempt < FILE_ATTEMPTS) delay(2000 * attempt);
      }
    }
    if (!done) {
      lastStatus = "Download non riuscito: " + path;
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

bool checkForUpdates(bool fullScan) {
  Serial.println("[UPDATE] Controllo aggiornamenti...");
  bool sd = sdAvailable();

  // 1. Manifest dell'ultima release: sulla SD se c'è, altrimenti in RAM
  //    (senza SD servono solo i dati del firmware)
  ManifestHeader h;
  bool downloaded;
  bool parsed = false;
  if (sd) {
    SD.remove(MANIFEST_TMP);
    File f = SD.open(MANIFEST_TMP, FILE_WRITE);
    downloaded = f && httpDownload(ATMOVERSE_UPDATE_MANIFEST_URL, [&](const uint8_t* data, int len) {
      return f.write(data, len) == (size_t)len;
    });
    if (f) f.close();
    if (downloaded) {
      File in = SD.open(MANIFEST_TMP, FILE_READ);
      parsed = in && parseManifestHeader(in, h);
      if (in) in.close();
    }
  } else {
    String text;
    downloaded = httpDownload(ATMOVERSE_UPDATE_MANIFEST_URL, [&](const uint8_t* data, int len) {
      if (text.length() + len > MAX_MANIFEST_SIZE) return false;
      text.concat((const char*)data, len);
      return true;
    });
    parsed = downloaded && parseManifestHeader(text, h);
  }

  if (!downloaded) {
    lastStatus = "Server degli aggiornamenti non raggiungibile";
    Serial.println("[UPDATE] " + lastStatus);
    return false;
  }
  if (!parsed) {
    lastStatus = "Manifest non valido";
    Serial.println("[UPDATE] " + lastStatus);
    if (sd) SD.remove(MANIFEST_TMP);
    return false;
  }

  // 2. Serve un nuovo firmware? (salta una versione che ha già fallito l'avvio)
  Preferences prefs;
  prefs.begin(PREFS_NS, true);
  String badVersion = prefs.getString(KEY_BAD, "");
  prefs.end();

  bool firmwareNewer = compareVersions(h.version.c_str(), ATMOVERSE_VERSION) > 0 && badVersion != h.version;

  // Con la batteria bassa e senza caricatore il firmware non si installa: uno
  // spegnimento durante la scrittura verrebbe recuperato dal rollback, ma è
  // meglio non rischiare. I file della SD si aggiornano comunque.
  if (firmwareNewer && battery.isAvailable() && !battery.charging() &&
      battery.getPercentage() < BATTERY_MIN_FIRMWARE_UPDATE_PERCENT) {
    Serial.printf("[UPDATE] Firmware %s rimandato: batteria al %d%%, non in carica\n",
                  h.version.c_str(), battery.getPercentage());
    firmwareNewer = false;
    lastStatus = "Firmware " + h.version + " disponibile: si installa con la batteria sopra il " +
                 String(BATTERY_MIN_FIRMWARE_UPDATE_PERCENT) + "% o in carica";
  }

  // 3. Quali file della SD sono cambiati? (elenco scritto sulla SD, non in RAM)
  int neededCount = 0;
  uint64_t neededBytes = 0;
  uint64_t neededDisk = 0;
  String synced = sd ? readTextFile(SYNCED_RELEASE) : String();
  synced.trim();
  bool filesInSync = !fullScan && synced.length() > 0 && synced == h.version;
  if (filesInSync) {
    Serial.println("[UPDATE] File della SD già allineati alla release " + h.version);
  } else if (sd && h.baseUrl.length() > 0) {
    if (SD.exists(STAGING_DIR)) removeTree(STAGING_DIR);
    SD.mkdir(STAGING_DIR);
    neededCount = buildNeededList(neededBytes, neededDisk);
    if (neededCount < 0) {
      SD.remove(MANIFEST_TMP);
      removeTree(STAGING_DIR);
      reportBlockingError("Aggiornamento non riuscito",
                          "Impossibile scrivere sulla scheda SD: spazio esaurito o scheda danneggiata.",
                          "SD piena o danneggiata: aggiornamento sospeso");
      return false;
    }
  }
  if (sd) SD.remove(MANIFEST_TMP);

  // Spazio: i file nuovi stanno in /upd accanto ai vecchi finché non sono tutti
  // verificati, quindi serve spazio per l'intera copia più un margine
  if (neededCount > 0) {
    uint64_t freeBytes = SD.totalBytes() - SD.usedBytes();
    uint64_t required = neededDisk + 512 * 1024;
    if (freeBytes < required) {
      removeTree(STAGING_DIR);
      reportBlockingError("Spazio insufficiente sulla SD",
                          "Per l'aggiornamento servono " + formatMB(required) + ", liberi " + formatMB(freeBytes) +
                          ". Libera spazio sulla scheda: AtmoVerse riprova da solo.",
                          "SD piena: servono " + formatMB(required) + ", liberi " + formatMB(freeBytes));
      return false;
    }
  }

  // Nessun file da scaricare: la SD è allineata a questa release
  if (sd && neededCount == 0 && !filesInSync && h.baseUrl.length() > 0) {
    writeTextFile(SYNCED_RELEASE, h.version);
  }

  if (!firmwareNewer && neededCount == 0) {
    if (sd && SD.exists(STAGING_DIR)) removeTree(STAGING_DIR);
    updateNotice = "";
    lastStatus = String("Aggiornato (versione ") + ATMOVERSE_VERSION + ")";
    Serial.println("[UPDATE] " + lastStatus);
    return true;
  }

  // 4. Download: il display mostra la schermata di aggiornamento
  Serial.printf("[UPDATE] Da scaricare: firmware %s, file %d\n", firmwareNewer ? h.version.c_str() : "no", neededCount);
  if (neededCount > 0) {
    startProgress("File della SD", neededCount, neededBytes);
  } else {
    startProgress("Nuovo firmware", 0, h.fwSize);
  }
  beginDownloadPhase();

  if (neededCount > 0 && !downloadNeededFiles(h.baseUrl)) {
    finishWithError(lastStatus);
    return false;
  }

  if (firmwareNewer) {
    if (neededCount > 0) {
      startProgress("Nuovo firmware", 0, h.fwSize);
      reportProgress(true);
    }
    if (h.fwUrl.length() == 0 || h.fwSize == 0 || !downloadFirmware(h.fwUrl, h.fwSize, h.fwSha)) {
      finishWithError("Installazione del firmware " + h.version + " non riuscita");
      return false;
    }

    // Da confermare dopo il riavvio; se non parte, initUpdater() lo segnerà come difettoso
    prefs.begin(PREFS_NS, false);
    prefs.putString(KEY_ATTEMPT, h.version);
    prefs.end();
  }

  // 5. Tutto verificato: i file vengono applicati ora (o al riavvio, se interrotti)
  if (neededCount > 0) {
    writeTextFile(STAGED_RELEASE, h.version);
    writeTextFile(READY_MARKER, "1");
  }

  if (firmwareNewer) {
    updateNotice = "";
    lastStatus = "Installata la versione " + h.version + ", riavvio";
    Serial.println("[UPDATE] " + lastStatus);
    delay(500);
    ESP.restart();  // I file della SD vengono applicati all'avvio da initUpdater()
  }

  applyPendingSdUpdate();
  updateNotice = "";
  lastStatus = "File della SD aggiornati";
  Serial.printf("[UPDATE] %d file della SD aggiornati\n", neededCount);
  endDownloadPhase();
  return true;
}
