/**
 * @file DisplayTask.h
 * @brief Disegno del display e-ink in un task dedicato sul core 0
 *
 * Il loop (core 1) prepara uno ScreenModel con tutto ciò che serve alla
 * schermata — dati meteo, citazione, icona già letta dalla SD, batteria, IP —
 * e lo consegna con showScreen(), che torna subito. Il task del display lo
 * disegna mentre il loop continua a servire web server, DNS e rete: un
 * refresh completo dell'e-ink dura circa 4 secondi.
 *
 * Il task non accede mai alla SD, alla rete o alle variabili globali del
 * programma: usa solo la propria copia del modello. L'unico dato condiviso è
 * la richiesta in attesa, protetta da un mutex. Se arrivano più richieste
 * durante un refresh viene disegnata solo l'ultima.
 */
#ifndef DISPLAY_TASK_H
#define DISPLAY_TASK_H

#include <Arduino.h>
#include "WeatherUtils.h"

enum ScreenKind : uint8_t {
  SCREEN_MAIN,     // Meteo, ora, citazione
  SCREEN_SETUP,    // Modalità AP con codice QR
  SCREEN_UPDATE,   // Download di un aggiornamento
  SCREEN_MESSAGE,  // Titolo + testo
};

// Icona meteo in RAM: 1 bit per pixel, righe dall'alto, bit a 1 = nero
static const int ICON_MAX_SIZE = 128;
static const int ICON_MAX_BYTES = ICON_MAX_SIZE * ICON_MAX_SIZE / 8;

struct ScreenModel {
  ScreenKind kind = SCREEN_MESSAGE;

  // Schermata principale
  WeatherData weather = {};
  bool weatherUpdateOk = true;
  String quoteText;
  String quoteAuthor;
  String city;
  String ip;
  bool metric = true;
  uint16_t iconWidth = 0;
  uint16_t iconHeight = 0;
  uint8_t icon[ICON_MAX_BYTES] = {};

  // Batteria (mostrata in tutte le schermate)
  bool showBattery = false;
  int batteryPercent = 0;
  bool batteryCharging = false;

  // Schermata di configurazione
  String apName;
  String apIp;

  // Messaggio
  String title;
  String text;
};

// Avvia il task del display (una volta, dopo display.init)
void startDisplayTask();

// Consegna una schermata da disegnare. Non bloccante.
void showScreen(const ScreenModel& model);

// Attende la fine del disegno in corso e di quello in attesa (es. prima di un riavvio)
bool waitDisplayIdle(uint32_t timeoutMs);

// Legge un'icona BMP monocromatica dalla SD nel modello (dal loop, non dal task)
bool loadIconBitmap(const char* path, ScreenModel& model);

#endif // DISPLAY_TASK_H
