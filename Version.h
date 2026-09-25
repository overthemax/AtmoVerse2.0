#ifndef ATMOVERSE_VERSION_H
#define ATMOVERSE_VERSION_H

// Versione del firmware. La GitHub Action di rilascio la sostituisce con quella
// del tag (es. tag v2.1.0 -> "2.1.0") prima di compilare.
#define ATMOVERSE_VERSION "2.1.0"

// Manifest dell'ultima release pubblicata: descrive firmware e file della SD
#define ATMOVERSE_UPDATE_MANIFEST_URL \
  "https://github.com/overthemax/AtmoVerse2.0/releases/latest/download/manifest.json"

#endif // ATMOVERSE_VERSION_H
