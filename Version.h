#ifndef ATMOVERSE_VERSION_H
#define ATMOVERSE_VERSION_H

// Versione del firmware. La GitHub Action di rilascio la sostituisce con quella
// del tag (es. tag v2.1.0 -> "2.1.0") prima di compilare.
#define ATMOVERSE_VERSION "2.1.10"

// Manifest dell'ultima release pubblicata: descrive firmware e file della SD
#define ATMOVERSE_UPDATE_MANIFEST_URL \
  "https://github.com/overthemax/AtmoVerse2.0/releases/latest/download/manifest.json"

// Ultima release via API: fornisce l'URL del manifest e il suo SHA-256
// (github.com e il CDN dei file usano una catena di certificati che l'ESP32
// non riesce a verificare; api.github.com sì)
#define ATMOVERSE_RELEASE_API_URL \
  "https://api.github.com/repos/overthemax/AtmoVerse2.0/releases/latest"

#endif // ATMOVERSE_VERSION_H
