/**
 * @file Screens.h
 * @brief Schermate del display e-ink con un unico sistema tipografico
 *
 * Font U8g2 (FreeUniversal e Lucida Sans) con lettere accentate, margini e
 * griglia comuni, icone meteo 100 px ingrandite esattamente x2.
 * Le funzioni draw* sono chiamate solo dal task del display (DisplayTask.cpp),
 * dentro un ciclo firstPage()/nextPage(), e usano esclusivamente il modello.
 */
#ifndef SCREENS_H
#define SCREENS_H

#include <Arduino.h>
#include "DisplayTask.h"

void drawMainScreen(const ScreenModel& m);
void drawSetupScreen(const ScreenModel& m);
void drawUpdateScreen(const ScreenModel& m);
void drawMessageScreen(const ScreenModel& m);
void drawBatteryScreen(const ScreenModel& m);

// true se la citazione entra per intero nel riquadro, anche col carattere più
// piccolo. Chiamata dal loop: usa un oggetto di misura separato dal task.
bool quoteFitsDisplay(const String& text, const String& author);

#endif // SCREENS_H
