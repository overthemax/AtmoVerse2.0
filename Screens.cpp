/**
 * @file Screens.cpp
 * @brief E-ink display screens (see Screens.h)
 *
 * 648 x 480 grid, 32 px side margins.
 *   0 -  54  header: weekday and date on the left, city on the right
 *  54 - 228  time, temperature and condition on the left, icon on the right
 * 228 - 280  four values: feels like, humidity, wind, pressure
 * 290 - 452  quote, with a font that adapts to its length
 * 460 - 480  footer: last update, IP address, battery
 */

#include "Screens.h"
#include "Hardware.h"
#include "QRCodeHelper.h"
#include "Language.h"
#include "AtmoVerseConstants.h"
#include <U8g2_for_Adafruit_GFX.h>
#include <time.h>
#include <math.h>

// ---------------------------------------------------------------------------
// Typographic system
// ---------------------------------------------------------------------------

#define FONT_TIME     u8g2_font_fur49_tn   // Time
#define FONT_TEMP     u8g2_font_fur35_tf   // Temperature
#define FONT_TITLE    u8g2_font_fur20_tf   // Titles of the service screens
#define FONT_HEADER   u8g2_font_fur17_tf   // Date
#define FONT_BODY     u8g2_font_luRS14_tf  // City, weather condition
#define FONT_TEXT     u8g2_font_luRS12_tf  // Text of the service screens
#define FONT_VALUE    u8g2_font_luBS14_tf  // Data values
#define FONT_LABEL    u8g2_font_luRS10_tf  // Data labels
#define FONT_SMALL    u8g2_font_luRS08_tf  // Footer

static const int MARGIN = 32;

// Quote box
static const int QUOTE_TOP = 290;
static const int QUOTE_BOTTOM = 452;
static const int QUOTE_MAX_LINES = 8;
static const int AUTHOR_MAX_LINES = 3;  // The author wraps, it is never cut

// Quote styles from the largest to the smallest: the first one with
// which text and author fit the box in full is used
struct QuoteStyle {
  const uint8_t* text;
  const uint8_t* author;
};
static const QuoteStyle QUOTE_STYLES[] = {
  {u8g2_font_luIS19_tf, u8g2_font_luRS12_tf},
  {u8g2_font_luIS14_tf, u8g2_font_luRS10_tf},
  {u8g2_font_luIS12_tf, u8g2_font_luRS10_tf},
  {u8g2_font_luIS10_tf, u8g2_font_luRS08_tf},
};
static const int QUOTE_STYLE_COUNT = sizeof(QUOTE_STYLES) / sizeof(QUOTE_STYLES[0]);

static const char* DAYS_IT[] = {"Domenica", "Lunedì", "Martedì", "Mercoledì", "Giovedì", "Venerdì", "Sabato"};
static const char* MONTHS_IT[] = {"gennaio", "febbraio", "marzo", "aprile", "maggio", "giugno", "luglio",
                                  "agosto", "settembre", "ottobre", "novembre", "dicembre"};
static const char* DAYS_EN[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
static const char* MONTHS_EN[] = {"January", "February", "March", "April", "May", "June", "July",
                                  "August", "September", "October", "November", "December"};

static const char* DEGREE = "\xC2\xB0";   // °
static const char* MIDDOT = " \xC2\xB7 "; // ·

// Every U8g2 object has a state (current font): the display task and the
// loop use one each, so they never interfere
typedef U8G2_FOR_ADAFRUIT_GFX Text;
static Text u8g2;     // Display task only
static Text measure;  // Loop only (quoteFitsDisplay)

static void initText(Text& t) {
  static bool ready[2] = {false, false};
  bool& r = ready[&t == &measure ? 1 : 0];
  if (!r) {
    t.begin(display);  // Stores only the pointer: "measure" never draws
    r = true;
  }
  t.setFontMode(1);
  t.setFontDirection(0);
  t.setForegroundColor(GxEPD_BLACK);
  t.setBackgroundColor(GxEPD_WHITE);
}

static int textWidth(Text& t, const String& s) { return t.getUTF8Width(s.c_str()); }
static int lineHeight(Text& t) { return t.getFontAscent() - t.getFontDescent() + 4; }
static int textWidth(const String& s) { return textWidth(u8g2, s); }
static int lineHeight() { return lineHeight(u8g2); }

static void textLeft(int x, int y, const String& s) { u8g2.drawUTF8(x, y, s.c_str()); }
static void textRight(int xRight, int y, const String& s) { u8g2.drawUTF8(xRight - textWidth(s), y, s.c_str()); }
static void textCenter(int cx, int y, const String& s) { u8g2.drawUTF8(cx - textWidth(s) / 2, y, s.c_str()); }

// The fonts cover the Latin alphabet (ISO 8859-1): typographic punctuation
// outside that set is replaced with its plain equivalent
static String displayText(String s) {
  s.replace("\xE2\x80\x98", "'");   // ‘
  s.replace("\xE2\x80\x99", "'");   // ’
  s.replace("\xE2\x80\x9C", "\"");  // “
  s.replace("\xE2\x80\x9D", "\"");  // ”
  s.replace("\xE2\x80\x93", "-");   // –
  s.replace("\xE2\x80\x94", "-");   // —
  s.replace("\xE2\x80\xA6", "..."); // …
  s.trim();
  return s;
}

// Splits the text into lines at most maxWidth wide (current font), also
// breaking at every '\n'. Returns the number of lines needed; stores at most
// maxLines.
static int wrapText(Text& t, const String& s, int maxWidth, String* lines, int maxLines) {
  int count = 0;
  String line;
  int start = 0;
  while (start < (int)s.length()) {
    int space = s.indexOf(' ', start);
    int newline = s.indexOf('\n', start);
    if (space < 0) space = s.length();
    bool breakAfter = newline >= 0 && newline < space;
    int end = breakAfter ? newline : space;
    String word = s.substring(start, end);
    start = end + 1;

    if (word.length()) {
      String candidate = line.length() ? line + " " + word : word;
      if (line.length() && textWidth(t, candidate) > maxWidth) {
        if (count < maxLines) lines[count] = line;
        count++;
        line = word;
      } else {
        line = candidate;
      }
    }
    if (breakAfter && line.length()) {
      if (count < maxLines) lines[count] = line;
      count++;
      line = "";
    }
  }
  if (line.length()) {
    if (count < maxLines) lines[count] = line;
    count++;
  }
  return count;
}

static String twoDigits(int v) {
  return v < 10 ? "0" + String(v) : String(v);
}

// Time as shown on the display: "14:05" or, with the 12-hour clock, "2:05"
// plus the "AM"/"PM" suffix. Only the display changes: every time-based
// choice (quotes of the literary clock, scheduled quotes) uses the 24-hour time.
static String clockText(int hour, int minute, bool use24h, String* suffix) {
  if (use24h) {
    if (suffix) *suffix = "";
    return twoDigits(hour) + ":" + twoDigits(minute);
  }
  int h12 = hour % 12 == 0 ? 12 : hour % 12;  // 0:xx -> 12:xx AM, 12:xx -> 12:xx PM
  String ampm = hour < 12 ? "AM" : "PM";
  if (suffix) {
    *suffix = ampm;
    return String(h12) + ":" + twoDigits(minute);
  }
  return String(h12) + ":" + twoDigits(minute) + " " + ampm;
}

static bool clockValid() {
  return time(nullptr) > 1700000000;  // Time synced (NTP or RTC)
}

// ---------------------------------------------------------------------------
// Quote
// ---------------------------------------------------------------------------

// Picks the largest style with which the quote fits the box.
// Returns the style index, or -1 if it does not fit even with the smallest one.
static int chooseQuoteStyle(Text& t, const String& text, const String& author, int width, int height) {
  String lines[QUOTE_MAX_LINES];
  for (int i = 0; i < QUOTE_STYLE_COUNT; i++) {
    t.setFont(QUOTE_STYLES[i].text);
    int n = wrapText(t, text, width, lines, QUOTE_MAX_LINES);
    if (n > QUOTE_MAX_LINES) continue;
    int total = n * lineHeight(t);
    if (author.length()) {
      t.setFont(QUOTE_STYLES[i].author);
      int authorLines = wrapText(t, author, width, lines, QUOTE_MAX_LINES);
      if (authorLines > AUTHOR_MAX_LINES) continue;
      total += 4 + authorLines * lineHeight(t);
    }
    if (total <= height) return i;
  }
  return -1;
}

// Poetry quoted on one line separates the verses with " / "; the quote
// editor lets the user type "\n". Both become line breaks when the quote
// still fits the box that way, otherwise the verses run on with " / ".
static String quoteLayout(Text& t, const String& quote, const String& author, int width, int height,
                          int* style) {
  String verses = quote;
  verses.replace(" / ", "\n");
  verses.replace("\\n", "\n");
  verses = "\xC2\xAB" + verses + "\xC2\xBB";  // «text»
  *style = chooseQuoteStyle(t, verses, author, width, height);
  if (*style >= 0) return verses;

  String flat = quote;
  flat.replace("\\n", " / ");
  flat = "\xC2\xAB" + flat + "\xC2\xBB";
  *style = chooseQuoteStyle(t, flat, author, width, height);
  return flat;
}

bool quoteFitsDisplay(const String& text, const String& author) {
  initText(measure);
  const int width = display.width() - 2 * MARGIN;
  int style;
  quoteLayout(measure, displayText(text), displayText(author), width, QUOTE_BOTTOM - QUOTE_TOP, &style);
  return style >= 0;
}

static void drawQuoteBlock(const String& quoteText, const String& quoteAuthor) {
  const int width = display.width() - 2 * MARGIN;
  const int height = QUOTE_BOTTOM - QUOTE_TOP;
  const int cx = display.width() / 2;
  if (quoteText.length() == 0) return;

  String author = displayText(quoteAuthor);
  int style;
  String text = quoteLayout(u8g2, displayText(quoteText), author, width, height, &style);
  bool truncated = style < 0;
  if (truncated) style = QUOTE_STYLE_COUNT - 1;

  // Text lines
  String lines[QUOTE_MAX_LINES];
  u8g2.setFont(QUOTE_STYLES[style].text);
  int textLh = lineHeight();
  int n = wrapText(u8g2, text, width, lines, QUOTE_MAX_LINES);

  // Author lines
  String authorLines[AUTHOR_MAX_LINES];
  int an = 0;
  int authorLh = 0;
  if (author.length()) {
    u8g2.setFont(QUOTE_STYLES[style].author);
    authorLh = lineHeight();
    an = min(AUTHOR_MAX_LINES, wrapText(u8g2, author, width, authorLines, AUTHOR_MAX_LINES));
  }

  // Text too long even with the smallest font: cut with "..."
  int maxTextLines = (height - (an ? 4 + an * authorLh : 0)) / textLh;
  if (n > maxTextLines) {
    n = maxTextLines;
    u8g2.setFont(QUOTE_STYLES[style].text);
    String& last = lines[n - 1];
    while (last.length() && textWidth(last + "...") > width) {
      int sp = last.lastIndexOf(' ');
      last = sp > 0 ? last.substring(0, sp) : last.substring(0, last.length() - 1);
    }
    last += "...";
  }

  // Block centered vertically in the box
  int total = n * textLh + (an ? 4 + an * authorLh : 0);
  int y = QUOTE_TOP + (height - total) / 2;

  u8g2.setFont(QUOTE_STYLES[style].text);
  for (int i = 0; i < n; i++) {
    y += textLh;
    textCenter(cx, y - 4 + u8g2.getFontDescent(), lines[i]);
  }
  if (an) {
    y += 4;
    u8g2.setFont(QUOTE_STYLES[style].author);
    for (int i = 0; i < an; i++) {
      y += authorLh;
      textRight(display.width() - MARGIN, y - 4 + u8g2.getFontDescent(), authorLines[i]);
    }
  }
}

// ---------------------------------------------------------------------------
// Shared elements
// ---------------------------------------------------------------------------

static void drawBatteryIndicator(const ScreenModel& m, int xRight, int baseline) {
  if (!m.showBattery) return;

  const int bw = 22, bh = 11;
  int bx = xRight - bw - 2;
  int by = baseline - bh + 1;
  display.drawRect(bx, by, bw, bh, GxEPD_BLACK);
  display.fillRect(bx + bw, by + 3, 2, bh - 6, GxEPD_BLACK);
  int fill = (bw - 4) * constrain(m.batteryPercent, 0, 100) / 100;
  if (fill > 0) display.fillRect(bx + 2, by + 2, fill, bh - 4, GxEPD_BLACK);

  u8g2.setFont(FONT_SMALL);
  String label = String(m.batteryPercent) + "%";
  if (m.batteryFull) label = String(TR("Carica completa", "Fully charged")) + MIDDOT + label;
  else if (m.batteryCharging) label = String(TR("In carica", "Charging")) + MIDDOT + label;
  textRight(bx - 6, baseline, label);
}

static void drawServiceFooter(const ScreenModel& m, const String& text) {
  u8g2.setFont(FONT_SMALL);
  textLeft(MARGIN, display.height() - 12, text);
  drawBatteryIndicator(m, display.width() - MARGIN, display.height() - 12);
}

// ---------------------------------------------------------------------------
// Main screen
// ---------------------------------------------------------------------------

// Icon from the model, scaled by num/den without interpolation: every source
// pixel becomes a solid block (sharp edges). With x2 every block is 2x2; a
// slightly smaller factor makes some blocks one pixel thinner.
static void drawIcon(const ScreenModel& m, int x, int y, int num, int den) {
  int bytesPerRow = (m.iconWidth + 7) / 8;
  for (int r = 0; r < m.iconHeight; r++) {
    int y0 = y + r * num / den, y1 = y + (r + 1) * num / den;
    if (y1 <= y0) continue;
    for (int c = 0; c < m.iconWidth; c++) {
      if (m.icon[r * bytesPerRow + c / 8] & (0x80 >> (c % 8))) {
        int x0 = x + c * num / den, x1 = x + (c + 1) * num / den;
        if (x1 > x0) display.fillRect(x0, y0, x1 - x0, y1 - y0, GxEPD_BLACK);
      }
    }
  }
}

// First and last non-empty row of the icon: used to center what is actually drawn
static void iconInkRows(const ScreenModel& m, int& first, int& last) {
  int bytesPerRow = (m.iconWidth + 7) / 8;
  first = -1;
  last = -1;
  for (int r = 0; r < m.iconHeight; r++) {
    for (int i = 0; i < bytesPerRow; i++) {
      if (m.icon[r * bytesPerRow + i]) {
        if (first < 0) first = r;
        last = r;
        break;
      }
    }
  }
}

static void thickArc(int cx, int cy, int r, int fromDeg, int toDeg, int thickness);

// WiFi icon: a dot and two fan-shaped arcs. (x, baseline) = bottom-left
// corner, about as tall as the header text. Returns the width.
static int drawWifiIcon(int x, int baseline) {
  const int size = 18;
  int cx = x + size / 2, cy = baseline - 1;
  display.fillCircle(cx, cy - 1, 2, GxEPD_BLACK);
  thickArc(cx, cy, 8, 225, 315, 2);
  thickArc(cx, cy, 14, 225, 315, 2);
  return size;
}

void drawMainScreen(const ScreenModel& m) {
  initText(u8g2);
  const int W = display.width();
  const int H = display.height();

  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);
  bool timeOk = clockValid();
  const WeatherData& w = m.weather;
  bool weatherOk = w.valid;

  // Header
  u8g2.setFont(FONT_HEADER);
  if (timeOk) {
    // "Venerdì 26 settembre" / "Friday 26 September"
    textLeft(MARGIN, 44, uiItalian()
        ? String(DAYS_IT[t.tm_wday]) + " " + String(t.tm_mday) + " " + MONTHS_IT[t.tm_mon]
        : String(DAYS_EN[t.tm_wday]) + " " + String(t.tm_mday) + " " + MONTHS_EN[t.tm_mon]);
  }
  u8g2.setFont(FONT_BODY);
  String city = displayText(m.city);
  textRight(W - MARGIN, 44, city);
  // WiFi on: icon to the left of the city
  if (m.wifiOn) drawWifiIcon(W - MARGIN - textWidth(city) - 12 - 18, 44);
  display.drawFastHLine(MARGIN, 54, W - 2 * MARGIN, GxEPD_BLACK);

  // Time
  u8g2.setFont(FONT_TIME);
  String ampm;
  String clock = timeOk ? clockText(t.tm_hour, t.tm_min, m.use24h, &ampm) : String("--:--");
  textLeft(MARGIN - 2, 118, clock);
  if (ampm.length()) {
    int clockWidth = textWidth(clock);
    u8g2.setFont(FONT_BODY);
    textLeft(MARGIN - 2 + clockWidth + 8, 118, ampm);
  }

  // Temperature and condition
  u8g2.setFont(FONT_TEMP);
  String temp = weatherOk ? String((int)lroundf(w.temp)) : String("--");
  textLeft(MARGIN, 180, temp + DEGREE);

  u8g2.setFont(FONT_BODY);
  String condition = weatherOk ? displayText(w.description) : String(TR("In attesa dei dati meteo", "Waiting for weather data"));
  if (condition.length() && condition[0] >= 'a' && condition[0] <= 'z') condition[0] = condition[0] - 32;
  textLeft(MARGIN, 210, condition);

  // Weather icon: 100 px BMP read by the loop, scaled x2. What is actually
  // drawn (not the bounding box) is centered between the header rule and the
  // data labels. The tallest icons (rain, lightning: 182 px at x2) are shrunk
  // just enough to fit the 173 px, so they never touch the rule or the labels.
  const int iconArea = 200;
  const int iconSpaceTop = 56, iconSpaceBottom = 229;
  if (weatherOk && m.iconWidth > 0) {
    int num = max(1, iconArea / max((int)m.iconWidth, (int)m.iconHeight));
    int den = 1;
    int first, last;
    iconInkRows(m, first, last);
    if (first < 0) { first = 0; last = m.iconHeight - 1; }
    int inkRows = last - first + 1;
    int space = iconSpaceBottom - iconSpaceTop;
    if (inkRows * num > space) { num = space; den = inkRows; }
    int inkH = inkRows * num / den;
    int y = iconSpaceTop + (space - inkH) / 2 - first * num / den;
    int drawnW = m.iconWidth * num / den;
    drawIcon(m, W - MARGIN - iconArea + (iconArea - drawnW) / 2, y, num, den);
  }

  // Four values
  const int colW = (W - 2 * MARGIN) / 4;
  String wind = "--";
  if (weatherOk) {
    wind = m.metric ? String((int)lroundf(w.wind_speed * 3.6f)) + " km/h"
                         : String((int)lroundf(w.wind_speed)) + " mph";
  }
  const String labels[4] = {TR("Percepita", "Feels like"), TR("Umidità", "Humidity"),
                            TR("Vento", "Wind"), TR("Pressione", "Pressure")};
  const String values[4] = {
    weatherOk ? String((int)lroundf(w.feels_like)) + DEGREE : String("--"),
    weatherOk ? String((int)lroundf(w.humidity)) + "%" : String("--"),
    wind,
    weatherOk ? String((int)lroundf(w.pressure)) + " hPa" : String("--"),
  };
  for (int i = 0; i < 4; i++) {
    int x = MARGIN + i * colW;
    u8g2.setFont(FONT_LABEL);
    textLeft(x, 242, labels[i]);
    u8g2.setFont(FONT_VALUE);
    textLeft(x, 266, values[i]);
  }
  display.drawFastHLine(MARGIN, 280, W - 2 * MARGIN, GxEPD_BLACK);

  // Quote
  drawQuoteBlock(m.quoteText, m.quoteAuthor);

  // Footer
  u8g2.setFont(FONT_SMALL);
  const int footerY = H - 12;
  if (m.notice.length()) {
    // Warning triangle + text
    display.fillTriangle(MARGIN, footerY, MARGIN + 12, footerY, MARGIN + 6, footerY - 11, GxEPD_BLACK);
    display.drawFastVLine(MARGIN + 6, footerY - 7, 4, GxEPD_WHITE);
    textLeft(MARGIN + 18, footerY, displayText(m.notice));
  } else if (w.last_update > 1700000000) {
    struct tm u;
    localtime_r(&w.last_update, &u);
    String when = clockText(u.tm_hour, u.tm_min, m.use24h, nullptr);
    textLeft(MARGIN, footerY, m.weatherUpdateOk
        ? String(TR("Aggiornato alle ", "Updated at ")) + when
        : String(TR("Meteo non aggiornato", "Weather not updated")) + MIDDOT + TR("ultimo alle ", "last at ") + when);
  } else if (!m.weatherUpdateOk) {
    textLeft(MARGIN, footerY, TR("Meteo non disponibile", "Weather unavailable"));
  }
  if (m.ip.length()) {
    textCenter(W / 2, footerY, m.ip);
  }
  drawBatteryIndicator(m, W - MARGIN, footerY);
}

// ---------------------------------------------------------------------------
// Service screens
// ---------------------------------------------------------------------------

void drawSetupScreen(const ScreenModel& m) {
  initText(u8g2);
  const String& apName = m.apName;
  const String& ipAddress = m.apIp;
  const int W = display.width();

  u8g2.setFont(FONT_TITLE);
  textLeft(MARGIN, 52, TR("Configurazione", "Setup"));
  u8g2.setFont(FONT_TEXT);
  textLeft(MARGIN, 80, TR("AtmoVerse non è collegato a una rete WiFi.", "AtmoVerse is not connected to a WiFi network."));
  display.drawFastHLine(MARGIN, 96, W - 2 * MARGIN, GxEPD_BLACK);

  // QR code on the right: joins the phone to the setup network
  static QRCodeHelper qr;
  const int qrArea = 216;
  int qrRight = W - MARGIN;
  int qrLeft = qrRight - qrArea;
  if (qr.generateWiFiQR(apName.c_str(), ATMOVERSE_AP_PASSWORD, "WPA")) {
    int modules = qr.getQRSize();
    int module = max(1, qrArea / modules);
    int size = modules * module;
    int x = qrLeft + (qrArea - size) / 2;
    qr.drawQRCode(display, x, 116, module);
    u8g2.setFont(FONT_LABEL);
    textCenter(qrLeft + qrArea / 2, 116 + size + 24, TR("Inquadra per collegarti", "Scan to connect"));
  }

  // Steps on the left
  const int textW = qrLeft - MARGIN - 32;
  const char* steps[] = {
    TR("Inquadra il codice QR con la fotocamera del telefono: il telefono si collega alla rete di AtmoVerse.",
       "Scan the QR code with your phone camera: the phone joins the AtmoVerse network."),
    TR("Si apre da sola la pagina di configurazione. Se non compare, apri il browser su http://",
       "The setup page opens by itself. If it does not, open your browser at http://"),
    TR("Scegli la rete WiFi di casa, inserisci città e API key di OpenWeatherMap e tocca Salva.",
       "Choose your home WiFi, enter your city and OpenWeatherMap API key, then tap Save."),
  };
  int y = 132;
  for (int i = 0; i < 3; i++) {
    String step = steps[i];
    if (i == 1) step += ipAddress;
    u8g2.setFont(FONT_VALUE);
    textLeft(MARGIN, y, String(i + 1));
    u8g2.setFont(FONT_TEXT);
    String lines[4];
    int n = min(4, wrapText(u8g2, step, textW, lines, 4));
    for (int j = 0; j < n; j++) {
      textLeft(MARGIN + 26, y, lines[j]);
      y += lineHeight();
    }
    y += 14;
  }

  // Manual connection
  y += 6;
  u8g2.setFont(FONT_LABEL);
  textLeft(MARGIN, y, TR("Collegamento manuale", "Manual connection"));
  y += 24;
  u8g2.setFont(FONT_TEXT);
  textLeft(MARGIN, y, String(TR("Rete  ", "Network  ")) + apName);
  y += lineHeight();
  textLeft(MARGIN, y, String("Password  ") + ATMOVERSE_AP_PASSWORD);

  drawServiceFooter(m, TR("Quando la rete di casa torna disponibile, AtmoVerse si ricollega da solo.",
                          "When your home network is back, AtmoVerse reconnects by itself."));
}

static String formatEta(int seconds) {
  if (seconds < 0) return TR("Stima del tempo in corso...", "Estimating time...");
  if (seconds < 60) return TR("Meno di un minuto", "Less than a minute");
  int minutes = (seconds + 30) / 60;
  if (minutes == 1) return TR("Circa 1 minuto", "About 1 minute");
  return String(TR("Circa ", "About ")) + String(minutes) + TR(" minuti", " minutes");
}

void drawUpdateScreen(const ScreenModel& m) {
  initText(u8g2);
  const int W = display.width();
  const int H = display.height();

  // Title
  u8g2.setFont(FONT_TITLE);
  textLeft(MARGIN, 52, TR("Aggiornamento in corso", "Updating"));
  u8g2.setFont(FONT_TEXT);
  textLeft(MARGIN, 80, TR("AtmoVerse sta installando una nuova versione.", "AtmoVerse is installing a new version."));
  display.drawFastHLine(MARGIN, 96, W - 2 * MARGIN, GxEPD_BLACK);

  // Step and percentage
  u8g2.setFont(FONT_BODY);
  textLeft(MARGIN, 160, m.updPhase.length() ? m.updPhase : String(TR("Preparazione", "Preparing")));
  u8g2.setFont(FONT_TEMP);
  textRight(W - MARGIN, 172, String(m.updPercent) + "%");

  // Progress bar: thin outline, solid fill
  const int barY = 196;
  const int barH = 22;
  const int barW = W - 2 * MARGIN;
  display.drawRect(MARGIN, barY, barW, barH, GxEPD_BLACK);
  int fill = (barW - 6) * m.updPercent / 100;
  if (fill > 0) display.fillRect(MARGIN + 3, barY + 3, fill, barH - 6, GxEPD_BLACK);

  // Details
  u8g2.setFont(FONT_TEXT);
  int y = barY + barH + 36;
  if (m.updFilesTotal > 0) {
    textLeft(MARGIN, y, String(m.updFilesDone) + TR(" di ", " of ") + String(m.updFilesTotal) +
                        TR(" file scaricati", " files downloaded"));
    y += lineHeight() + 4;
  }
  textLeft(MARGIN, y, formatEta(m.updEtaSec));

  // Warning
  u8g2.setFont(FONT_TEXT);
  textLeft(MARGIN, H - 64, TR("Non spegnere il dispositivo:", "Do not switch off the device:"));
  textLeft(MARGIN, H - 64 + lineHeight(), TR("al termine si riavvia o torna al meteo da solo.",
                                               "when done it restarts or returns to the weather by itself."));
  drawServiceFooter(m, TR("La schermata si aggiorna a ogni 10% di avanzamento.",
                          "This screen refreshes every 10% of progress."));
}

void drawMessageScreen(const ScreenModel& m) {
  initText(u8g2);
  const String& title = m.title;
  const String& text = m.text;
  const int W = display.width();
  const int H = display.height();

  String lines[8];
  u8g2.setFont(FONT_TEXT);
  int n = min(8, wrapText(u8g2, displayText(text), W - 4 * MARGIN, lines, 8));
  int lh = lineHeight();

  int y = H / 2 - (n * lh) / 2;
  if (title.length()) {
    u8g2.setFont(FONT_TITLE);
    textCenter(W / 2, y - 12, displayText(title));
    y += 24;
  }
  u8g2.setFont(FONT_TEXT);
  for (int i = 0; i < n; i++) {
    textCenter(W / 2, y, lines[i]);
    y += lh;
  }
}

// ---------------------------------------------------------------------------
// Battery empty: tired face
// ---------------------------------------------------------------------------

// Thick arc: filled dots along the circumference (angles in degrees, 0 = right,
// clockwise because the screen y grows downwards)
static void thickArc(int cx, int cy, int r, int fromDeg, int toDeg, int thickness) {
  for (int a = fromDeg; a <= toDeg; a += 2) {
    float rad = a * PI / 180.0f;
    display.fillCircle(cx + (int)lroundf(r * cosf(rad)), cy + (int)lroundf(r * sinf(rad)),
                       thickness / 2, GxEPD_BLACK);
  }
}

static void thickLine(int x0, int y0, int x1, int y1, int thickness) {
  int steps = max(abs(x1 - x0), abs(y1 - y0));
  for (int i = 0; i <= steps; i++) {
    display.fillCircle(x0 + (x1 - x0) * i / max(1, steps), y0 + (y1 - y0) * i / max(1, steps),
                       thickness / 2, GxEPD_BLACK);
  }
}

static void drawTiredFace(int cx, int cy, int r) {
  // Outline
  for (int i = 0; i < 4; i++) display.drawCircle(cx, cy, r - i, GxEPD_BLACK);

  const int eyeDx = r * 36 / 100;
  const int eyeY = cy - r * 12 / 100;
  const int eyeW = r * 22 / 100;
  for (int side = -1; side <= 1; side += 2) {
    int ex = cx + side * eyeDx;
    // Drooping eyelid: a line with the lower half of the pupil below it
    thickLine(ex - eyeW, eyeY, ex + eyeW, eyeY, 5);
    display.fillCircle(ex, eyeY + 2, eyeW * 55 / 100, GxEPD_BLACK);
    display.fillRect(ex - eyeW, eyeY - eyeW, eyeW * 2 + 1, eyeW, GxEPD_WHITE);
    thickLine(ex - eyeW, eyeY, ex + eyeW, eyeY, 5);
    // Eye bags
    thickArc(ex, eyeY + eyeW * 40 / 100, eyeW * 85 / 100, 35, 145, 3);
    // Eyebrows slanting outwards (tired look)
    int by = eyeY - r * 26 / 100;
    thickLine(ex - side * eyeW, by - 6, ex + side * eyeW, by + 4, 5);
  }

  // Wavy mouth
  const int mouthY = cy + r * 40 / 100;
  const int mouthW = r * 34 / 100;
  int px = cx - mouthW;
  int py = mouthY;
  for (int x = cx - mouthW; x <= cx + mouthW; x += 2) {
    int y = mouthY + (int)lroundf(3.0f * sinf((x - cx) * PI / mouthW));
    thickLine(px, py, x, y, 5);
    px = x;
    py = y;
  }

  // Drop of sweat
  int sx = cx + r * 78 / 100;
  int sy = cy - r * 48 / 100;
  display.fillCircle(sx, sy + 8, 8, GxEPD_BLACK);
  display.fillTriangle(sx - 7, sy + 5, sx + 7, sy + 5, sx, sy - 12, GxEPD_BLACK);
}

void drawBatteryScreen(const ScreenModel& m) {
  initText(u8g2);
  const int W = display.width();
  const int H = display.height();

  drawTiredFace(W / 2, 150, 92);

  u8g2.setFont(FONT_TITLE);
  textCenter(W / 2, 300, TR("Batteria scarica", "Battery empty"));
  u8g2.setFont(FONT_TEXT);
  textCenter(W / 2, 336, TR("Collega il caricatore:", "Plug in the charger:"));
  textCenter(W / 2, 336 + lineHeight(), TR("AtmoVerse ripartirà da solo.", "AtmoVerse will restart by itself."));

  // Empty battery with the percentage
  const int bw = 64, bh = 28;
  int bx = W / 2 - bw / 2 - 20;
  int by = 392;
  display.drawRect(bx, by, bw, bh, GxEPD_BLACK);
  display.drawRect(bx + 1, by + 1, bw - 2, bh - 2, GxEPD_BLACK);
  display.fillRect(bx + bw, by + 8, 5, bh - 16, GxEPD_BLACK);
  int fill = (bw - 8) * constrain(m.batteryPercent, 0, 100) / 100;
  if (fill > 0) display.fillRect(bx + 4, by + 4, max(fill, 2), bh - 8, GxEPD_BLACK);
  u8g2.setFont(FONT_VALUE);
  textLeft(bx + bw + 14, by + 21, String(m.batteryPercent) + "%");
}
