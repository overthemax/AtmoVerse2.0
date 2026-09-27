#include "NetworkUtils.h"
#include "Config.h"
#include "WebServer.h"
#include "Debug.h"
#include "AtmoVerseConstants.h"
#include <WiFi.h>
#include <SD.h>
#include <time.h>
#include <DNSServer.h>

DNSServer dnsServer;
bool apMode = false;

const byte DNS_PORT = 53;

// POSIX TZ string from the configured time zone. Daylight saving follows the
// European rules (last Sunday of March / October) instead of always adding
// daylightOffset_sec. POSIX inverts the sign: UTC+1 is written "-1".
static void buildTimezone(char* tz, size_t len) {
  long offset = config.gmtOffset_sec;
  int hours = -(int)(offset / 3600);
  int minutes = abs((int)(offset % 3600)) / 60;
  if (config.daylightOffset_sec > 0) {
    snprintf(tz, len, "STD%+d:%02dDST,M3.5.0,M10.5.0/3", hours, minutes);
  } else {
    snprintf(tz, len, "STD%+d:%02d", hours, minutes);
  }
}

// Sets only the time zone (also without network), so that the RTC time,
// which is UTC, is shown as local time
void applyTimezone() {
  char tz[48];
  buildTimezone(tz, sizeof(tz));
  setenv("TZ", tz, 1);
  tzset();
}

// Time zone and NTP server from the settings
void setupTimeServer() {
  DEBUG_TRACE();
  char tz[48];
  buildTimezone(tz, sizeof(tz));
  const char* ntp = strlen(config.ntpServer) > 0 ? config.ntpServer : "pool.ntp.org";
  configTzTime(tz, ntp, "time.nist.gov");
  Serial.printf("[TIME] Time zone %s, NTP server %s\n", tz, ntp);
}

// Starts the setup access point, with a captive DNS and the web server
void startAccessPoint(bool forceStart) {
  DEBUG_TRACE();
  if (apMode && !forceStart) {
    return;
  }

  // Fresh settings from the SD card for the setup page
  loadConfig();

  // SSID with the last 4 characters of the MAC address
  String macAddress = WiFi.macAddress();
  String lastFourMac = macAddress.substring(macAddress.length() - 5);
  lastFourMac.replace(":", "");
  String apSSID = String(ATMOVERSE_AP_SSID) + "_" + lastFourMac;

  // Access point network (IP 192.168.4.1)
  IPAddress localIP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);

  // Explicit AP mode, to avoid mixed states
  WiFi.mode(WIFI_AP);
  delay(100);
  WiFi.softAPConfig(localIP, gateway, subnet);

  bool success = WiFi.softAP(apSSID.c_str(), ATMOVERSE_AP_PASSWORD);
  delay(500);  // Let the AP settle

  if (!success) {
    Serial.println("[WIFI] Cannot start the access point, retrying");
    WiFi.mode(WIFI_OFF);
    delay(500);
    WiFi.mode(WIFI_AP);
    delay(500);
    success = WiFi.softAP(apSSID.c_str(), ATMOVERSE_AP_PASSWORD);
    if (!success) {
      Serial.println("[WIFI] Access point failed");
      return;
    }
  }

  // Captive portal DNS: every name points to the device
  IPAddress apIP = WiFi.softAPIP();
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", apIP);

  apMode = true;
  setupServer();

  Serial.printf("[WIFI] Access point %s active at %s\n", apSSID.c_str(), apIP.toString().c_str());
}

bool isWiFiConnected() {
  DEBUG_TRACE();
  return WiFi.status() == WL_CONNECTED;
}

// Connects to a WiFi network, leaving AP mode first if needed. Waits up to 30 s.
bool connectToWiFi(const char* ssid, const char* password) {
  DEBUG_TRACE();
  if (strlen(ssid) == 0) {
    return false;
  }

  // From AP to station mode, with long pauses to avoid conflicts
  if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
    if (apMode) {
      dnsServer.stop();
      apMode = false;
    }
    WiFi.softAPdisconnect(true);
    delay(200);
    WiFi.mode(WIFI_OFF);
    delay(1000);
    WiFi.mode(WIFI_STA);
    delay(1000);
  }

  if (WiFi.getMode() != WIFI_STA) {
    WiFi.mode(WIFI_STA);
    delay(1000);
  }

  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);      // No modem sleep while connecting: more stable
  WiFi.persistent(false);    // No flash writes for the WiFi credentials

  WiFi.begin(ssid, password);

  int attemptCount = 0;
  const int maxAttempts = 60;  // 30 seconds (60 * 500 ms)
  while (WiFi.status() != WL_CONNECTED && attemptCount < maxAttempts) {
    delay(500);
    attemptCount++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    setupTimeServer();
    return true;
  }
  return false;
}

// First WiFi connection (the settings are already loaded by setup())
bool setupWiFi() {
  DEBUG_TRACE();
  // No network configured: setup access point
  if (strlen(config.ssid) == 0) {
    startAccessPoint();
    return false;
  }
  return connectToWiFi(config.ssid, config.password);
}
