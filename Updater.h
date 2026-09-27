/**
 * @file Updater.h
 * @brief Automatic update of firmware and SD files from GitHub Releases
 *
 * The latest public release contains manifest.json with version, URL and
 * SHA-256 of the firmware and of the SD files. The device downloads only what
 * changed, verifies every file and applies everything together:
 * - SD files are prepared in /upd and moved into place only when everything
 *   has been downloaded and verified (even after an interruption);
 * - the firmware goes to the second flash slot; if the new version does not
 *   survive its first 60 seconds, the bootloader goes back to the previous
 *   one and the faulty version is never downloaded again.
 */
#ifndef UPDATER_H
#define UPDATER_H

#include <Arduino.h>

enum UpdateState : uint8_t {
  UPDATE_IDLE,
  UPDATE_DOWNLOADING,   // The display shows the update screen
};

extern volatile UpdateState updateState;

// Call in setup() after mounting the SD card: detects a firmware rollback
// and completes any pending SD update
void initUpdater();

// Checks GitHub and installs the updates. Only from the network task, with WiFi connected.
// If a new firmware is installed the device restarts.
// Returns false if the check failed (to be retried soon).
// If the SD files already match the latest release their SHA-256 is not
// computed again, unless fullScan (check started from the web page).
bool checkForUpdates(bool fullScan = false);

// Confirms that the running firmware works (cancels the automatic rollback)
void markFirmwareHealthy();

// Result of the last check, shown on the web pages
String getUpdateStatusText();

// Problem that needs action (e.g. SD card full), shown in the display
// footer until a later check succeeds.
// Empty if there is no problem.
String getUpdateNotice();

// HTTPS GET with the server certificate verified (ESP-IDF root
// certificate bundle), following redirects. Also used for the weather.
// Returns the HTTP code (200 = ok) or -1 if the connection fails.
int httpsGet(const String& url, String& body, size_t maxLen);

#endif // UPDATER_H
