/**
 * @file Screens.cpp
 * @brief Schermate del display e-ink (vedi Screens.h)
 *
 * Griglia 648 x 480, margini laterali di 32 px.
 *   0 -  60  intestazione: giorno e data a sinistra, città a destra
 *  66 - 266  ora, temperatura e condizione a sinistra, icona 200 px a destra
 * 276 - 334  quattro dati: percepita, umidità, vento, pressione
 * 346 - 452  citazione, con carattere che si adatta alla lunghezza
 * 460 - 480  piè di pagina: ultimo aggiornamento, IP, batteria
 */

#include "Screens.h"
#include "Hardware.h"
#include "QRCodeHelper.h"
#include "AtmoVerseConstants.h"
#include <U8g2_for_Adafruit_GFX.h>
#include <time.h>
#include <math.h>

// ---------------------------------------------------------------------------
// Sistema tipografico
// ---------------------------------------------------------------------------

#define FONT_TIME     u8g2_font_fur49_tn   // Ora
#define FONT_TEMP     u8g2_font_fur35_tf   // Temperatura
#define FONT_TITLE    u8g2_font_fur20_tf   // Titoli delle schermate di servizio
#define FONT_HEADER   u8g2_font_fur17_tf   // Data
#define FONT_BODY     u8g2_font_luRS14_tf  // Città, condizione meteo
#define FONT_TEXT     u8g2_font_luRS12_tf  // Testi delle schermate di servizio
#define FONT_VALUE    u8g2_font_luBS14_tf  // Valori dei dati
#define FONT_LABEL    u8g2_font_luRS10_tf  // Etichette dei dati
#define FONT_SMALL    u8g2_font_luRS08_tf  // Piè di pagina

static const int MARGIN = 32;

// Riquadro della citazione
static const int QUOTE_TOP = 346;
static const int QUOTE_BOTTOM = 452;
static const int QUOTE_MAX_LINES = 8;

// Stili della citazione dal più grande al più piccolo: si usa il primo con
// cui testo e autore entrano per intero nel riquadro
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

static const char* DAYS[] = {"Domenica", "Lunedì", "Martedì", "Mercoledì", "Giovedì", "Venerdì", "Sabato"};
static const char* MONTHS[] = {"gennaio", "febbraio", "marzo", "aprile", "maggio", "giugno", "luglio",
                               "agosto", "settembre", "ottobre", "novembre", "dicembre"};

static const char* DEGREE = "\xC2\xB0";   // °
static const char* MIDDOT = " \xC2\xB7 "; // ·

// Ogni oggetto U8g2 ha uno stato (font corrente): il task del display e il
// loop ne usano uno ciascuno, così non interferiscono tra loro
typedef U8G2_FOR_ADAFRUIT_GFX Text;
static Text u8g2;     // Solo task del display
static Text measure;  // Solo loop (quoteFitsDisplay)

static void initText(Text& t) {
  static bool ready[2] = {false, false};
  bool& r = ready[&t == &measure ? 1 : 0];
  if (!r) {
    t.begin(display);  // Memorizza solo il puntatore: "measure" non disegna mai
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

// I font coprono l'alfabeto latino (ISO 8859-1): la punteggiatura tipografica
// fuori da quell'insieme viene sostituita con l'equivalente semplice
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

// Divide il testo in righe larghe al massimo maxWidth (font corrente).
// Restituisce il numero di righe necessarie; ne salva al massimo maxLines.
static int wrapText(Text& t, const String& s, int maxWidth, String* lines, int maxLines) {
  int count = 0;
  String line;
  int start = 0;
  while (start < (int)s.length()) {
    int space = s.indexOf(' ', start);
    if (space < 0) space = s.length();
    String word = s.substring(start, space);
    start = space + 1;
    if (word.length() == 0) continue;

    String candidate = line.length() ? line + " " + word : word;
    if (line.length() && textWidth(t, candidate) > maxWidth) {
      if (count < maxLines) lines[count] = line;
      count++;
      line = word;
    } else {
      line = candidate;
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

static bool clockValid() {
  return time(nullptr) > 1700000000;  // Ora sincronizzata (NTP o RTC)
}

// ---------------------------------------------------------------------------
// Citazione
// ---------------------------------------------------------------------------

// Sceglie lo stile più grande con cui la citazione entra nel riquadro.
// Restituisce l'indice dello stile, oppure -1 se non entra nemmeno col più piccolo.
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
      total += 4 + authorLines * lineHeight(t);
    }
    if (total <= height) return i;
  }
  return -1;
}

bool quoteFitsDisplay(const String& text, const String& author) {
  initText(measure);
  const int width = display.width() - 2 * MARGIN;
  String t = "\xC2\xAB" + displayText(text) + "\xC2\xBB";
  return chooseQuoteStyle(measure, t, displayText(author), width, QUOTE_BOTTOM - QUOTE_TOP) >= 0;
}

static void drawQuoteBlock(const String& quoteText, const String& quoteAuthor) {
  const int width = display.width() - 2 * MARGIN;
  const int height = QUOTE_BOTTOM - QUOTE_TOP;
  const int cx = display.width() / 2;
  if (quoteText.length() == 0) return;

  String text = "\xC2\xAB" + displayText(quoteText) + "\xC2\xBB";  // «testo»
  String author = displayText(quoteAuthor);

  int style = chooseQuoteStyle(u8g2, text, author, width, height);
  bool truncated = style < 0;
  if (truncated) style = QUOTE_STYLE_COUNT - 1;

  // Righe del testo
  String lines[QUOTE_MAX_LINES];
  u8g2.setFont(QUOTE_STYLES[style].text);
  int textLh = lineHeight();
  int n = wrapText(u8g2, text, width, lines, QUOTE_MAX_LINES);

  // Righe dell'autore
  String authorLines[2];
  int an = 0;
  int authorLh = 0;
  if (author.length()) {
    u8g2.setFont(QUOTE_STYLES[style].author);
    authorLh = lineHeight();
    an = min(2, wrapText(u8g2, author, width, authorLines, 2));
  }

  // Testo troppo lungo anche col carattere più piccolo: si taglia con "..."
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

  // Blocco centrato verticalmente nel riquadro
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
      textCenter(cx, y - 4 + u8g2.getFontDescent(), authorLines[i]);
    }
  }
}

// ---------------------------------------------------------------------------
// Elementi comuni
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
  if (m.batteryCharging) label = "In carica" + String(MIDDOT) + label;
  textRight(bx - 6, baseline, label);
}

static void drawServiceFooter(const ScreenModel& m, const String& text) {
  u8g2.setFont(FONT_SMALL);
  textLeft(MARGIN, display.height() - 12, text);
  drawBatteryIndicator(m, display.width() - MARGIN, display.height() - 12);
}

// ---------------------------------------------------------------------------
// Schermata principale
// ---------------------------------------------------------------------------

// Icona dal modello, ingrandita di un fattore intero (bordi netti, nessuna interpolazione)
static void drawIcon(const ScreenModel& m, int x, int y, int scale) {
  int bytesPerRow = (m.iconWidth + 7) / 8;
  for (int r = 0; r < m.iconHeight; r++) {
    for (int c = 0; c < m.iconWidth; c++) {
      if (m.icon[r * bytesPerRow + c / 8] & (0x80 >> (c % 8))) {
        display.fillRect(x + c * scale, y + r * scale, scale, scale, GxEPD_BLACK);
      }
    }
  }
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

  // Intestazione
  u8g2.setFont(FONT_HEADER);
  if (timeOk) {
    textLeft(MARGIN, 44, String(DAYS[t.tm_wday]) + " " + String(t.tm_mday) + " " + MONTHS[t.tm_mon]);
  }
  u8g2.setFont(FONT_BODY);
  textRight(W - MARGIN, 44, displayText(m.city));
  display.drawFastHLine(MARGIN, 60, W - 2 * MARGIN, GxEPD_BLACK);

  // Ora
  u8g2.setFont(FONT_TIME);
  textLeft(MARGIN - 2, 136, timeOk ? twoDigits(t.tm_hour) + ":" + twoDigits(t.tm_min) : String("--:--"));

  // Temperatura e condizione
  u8g2.setFont(FONT_TEMP);
  String temp = weatherOk ? String((int)lroundf(w.temp)) : String("--");
  textLeft(MARGIN, 204, temp + DEGREE);

  u8g2.setFont(FONT_BODY);
  String condition = weatherOk ? displayText(w.description) : String("In attesa dei dati meteo");
  if (condition.length() && condition[0] >= 'a' && condition[0] <= 'z') condition[0] = condition[0] - 32;
  textLeft(MARGIN, 238, condition);

  // Icona meteo: BMP 100 px letta dal loop, ingrandita esattamente x2
  const int iconArea = 200;
  if (weatherOk && m.iconWidth > 0) {
    int scale = max(1, iconArea / max((int)m.iconWidth, (int)m.iconHeight));
    drawIcon(m, W - MARGIN - iconArea + (iconArea - m.iconWidth * scale) / 2,
             66 + (iconArea - m.iconHeight * scale) / 2, scale);
  }

  // Quattro dati
  const int colW = (W - 2 * MARGIN) / 4;
  String wind = "--";
  if (weatherOk) {
    wind = m.metric ? String((int)lroundf(w.wind_speed * 3.6f)) + " km/h"
                         : String((int)lroundf(w.wind_speed)) + " mph";
  }
  const String labels[4] = {"Percepita", "Umidità", "Vento", "Pressione"};
  const String values[4] = {
    weatherOk ? String((int)lroundf(w.feels_like)) + DEGREE : String("--"),
    weatherOk ? String((int)lroundf(w.humidity)) + "%" : String("--"),
    wind,
    weatherOk ? String((int)lroundf(w.pressure)) + " hPa" : String("--"),
  };
  for (int i = 0; i < 4; i++) {
    int x = MARGIN + i * colW;
    u8g2.setFont(FONT_LABEL);
    textLeft(x, 292, labels[i]);
    u8g2.setFont(FONT_VALUE);
    textLeft(x, 318, values[i]);
  }
  display.drawFastHLine(MARGIN, 334, W - 2 * MARGIN, GxEPD_BLACK);

  // Citazione
  drawQuoteBlock(m.quoteText, m.quoteAuthor);

  // Piè di pagina
  u8g2.setFont(FONT_SMALL);
  const int footerY = H - 12;
  if (w.last_update > 1700000000) {
    struct tm u;
    localtime_r(&w.last_update, &u);
    String when = twoDigits(u.tm_hour) + ":" + twoDigits(u.tm_min);
    textLeft(MARGIN, footerY, m.weatherUpdateOk ? "Aggiornato alle " + when
                                                       : "Meteo non aggiornato" + String(MIDDOT) + "ultimo alle " + when);
  } else if (!m.weatherUpdateOk) {
    textLeft(MARGIN, footerY, "Meteo non disponibile");
  }
  if (m.ip.length()) {
    textCenter(W / 2, footerY, m.ip);
  }
  drawBatteryIndicator(m, W - MARGIN, footerY);
}

// ---------------------------------------------------------------------------
// Schermate di servizio
// ---------------------------------------------------------------------------

void drawSetupScreen(const ScreenModel& m) {
  initText(u8g2);
  const String& apName = m.apName;
  const String& ipAddress = m.apIp;
  const int W = display.width();

  u8g2.setFont(FONT_TITLE);
  textLeft(MARGIN, 52, "Configurazione");
  u8g2.setFont(FONT_TEXT);
  textLeft(MARGIN, 80, "AtmoVerse non è collegato a una rete WiFi.");
  display.drawFastHLine(MARGIN, 96, W - 2 * MARGIN, GxEPD_BLACK);

  // QR code a destra: collega il telefono alla rete di configurazione
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
    textCenter(qrLeft + qrArea / 2, 116 + size + 24, "Inquadra per collegarti");
  }

  // Passi a sinistra
  const int textW = qrLeft - MARGIN - 32;
  const char* steps[] = {
    "Inquadra il codice QR con la fotocamera del telefono: il telefono si collega alla rete di AtmoVerse.",
    "Si apre da sola la pagina di configurazione. Se non compare, apri il browser su http://",
    "Scegli la rete WiFi di casa, inserisci città e API key di OpenWeatherMap e tocca Salva.",
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

  // Collegamento manuale
  y += 6;
  u8g2.setFont(FONT_LABEL);
  textLeft(MARGIN, y, "Collegamento manuale");
  y += 24;
  u8g2.setFont(FONT_TEXT);
  textLeft(MARGIN, y, "Rete  " + apName);
  y += lineHeight();
  textLeft(MARGIN, y, String("Password  ") + ATMOVERSE_AP_PASSWORD);

  drawServiceFooter(m, "Quando la rete di casa torna disponibile, AtmoVerse si ricollega da solo.");
}

void drawUpdateScreen(const ScreenModel& m) {
  initText(u8g2);
  const int W = display.width();
  const int cy = display.height() / 2;

  // Freccia di download sottile
  display.drawFastVLine(W / 2, cy - 110, 50, GxEPD_BLACK);
  display.drawFastVLine(W / 2 + 1, cy - 110, 50, GxEPD_BLACK);
  display.drawLine(W / 2 - 16, cy - 76, W / 2, cy - 60, GxEPD_BLACK);
  display.drawLine(W / 2 + 17, cy - 76, W / 2 + 1, cy - 60, GxEPD_BLACK);
  display.drawFastHLine(W / 2 - 24, cy - 48, 50, GxEPD_BLACK);

  u8g2.setFont(FONT_TITLE);
  textCenter(W / 2, cy, "Aggiornamento in corso");
  u8g2.setFont(FONT_TEXT);
  textCenter(W / 2, cy + 36, "AtmoVerse sta installando una nuova versione.");
  textCenter(W / 2, cy + 36 + lineHeight(), "Non spegnere il dispositivo: si riavvierà da solo.");
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
