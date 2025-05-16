#ifndef CALENDAR_H
#define CALENDAR_H

#include <Arduino.h>
#include <time.h>

// Genera un mini-calendario per il display e-ink
void drawCalendar(int16_t x, int16_t y, uint16_t width, uint16_t height);

// Ottiene il numero di giorni in un mese specifico
int getDaysInMonth(int month, int year);

// Ottiene il giorno della settimana per il primo giorno del mese (0 = domenica, 1 = lunedì, ecc.)
int getFirstDayOfMonth(int month, int year);

// Controlla se l'anno è bisestile
bool isLeapYear(int year);

#endif // CALENDAR_H
