#ifndef ATMOVERSE_VERSION_H
#define ATMOVERSE_VERSION_H

// Firmware version. The release GitHub Action replaces it with the one
// from the tag (e.g. tag v2.1.0 -> "2.1.0") before compiling.
#define ATMOVERSE_VERSION "2.1.16"

// Manifest of the latest published release: describes firmware and SD files
#define ATMOVERSE_UPDATE_MANIFEST_URL \
  "https://github.com/overthemax/AtmoVerse2.0/releases/latest/download/manifest.json"

// Latest release through the API: gives the manifest URL and its SHA-256
// (github.com and the file CDN use a certificate chain the ESP32 cannot
// verify; api.github.com works)
#define ATMOVERSE_RELEASE_API_URL \
  "https://api.github.com/repos/overthemax/AtmoVerse2.0/releases/latest"

#endif // ATMOVERSE_VERSION_H
