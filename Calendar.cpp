#include "Calendar.h"
#include "Display.h"
#include <Fonts/FreeSerif9pt7b.h>
#include <Fonts/FreeSerif12pt7b.h>

// Nomi dei mesi abbreviati in italiano
const char* monthNames[] = {"Gen", "Feb", "Mar", "Apr", "Mag", "Giu", "Lug", "Ago", "Set", "Ott", "Nov", "Dic"};

// Nomi dei giorni della settimana abbreviati in italiano
const char* dayNames[] = {"Do", "Lu", "Ma", "Me", "Gi", "Ve", "Sa"};

// Numero di giorni in ogni mese (non bisestile)
const int daysInMonths[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

// Controlla se l'anno è bisestile
bool isLeapYear(int year) {
  return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

// Ottiene il numero di giorni in un mese specifico
int getDaysInMonth(int month, int year) {
  if (month == 1 && isLeapYear(year)) {  // Febbraio bisestile
    return 29;
  }
  return daysInMonths[month];
}

// Ottiene il giorno della settimana per il primo giorno del mese (0 = domenica, 1 = lunedì, ecc.)
int getFirstDayOfMonth(int month, int year) {
  // Algoritmo di Zeller per calcolare il giorno della settimana
  if (month < 3) {
    month += 12;
    year--;
  }
  int k = year % 100;
  int j = year / 100;
  int h = (1 + (13 * (month + 1)) / 5 + k + k / 4 + j / 4 + 5 * j) % 7;

  // Converte dal formato di Zeller (0 = Sabato) a quello standard (0 = Domenica)
  return (h + 6) % 7;
}

// Genera un mini-calendario per il display e-ink
void drawCalendar(int16_t x, int16_t y, uint16_t width, uint16_t height) {
  // Ottieni data e ora correnti
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);

  int currentDay = timeinfo.tm_mday;
  int currentMonth = timeinfo.tm_mon;
  int currentYear = timeinfo.tm_year + 1900;

  // Calcola i parametri del calendario
  int daysInMonth = getDaysInMonth(currentMonth, currentYear);
  int firstDay = getFirstDayOfMonth(currentMonth, currentYear);

  // Dimensioni della griglia del calendario
  int headerHeight = height / 4;
  int gridHeight = height - headerHeight;
  int cellWidth = width / 7;
  int cellHeight = gridHeight / 6;

  // Disegna l'intestazione del mese
  display.setFont(&FreeSerif12pt7b);
  display.setTextColor(GxEPD_BLACK);
  
  // Crea la stringa del mese e anno
  String monthYearStr = String(monthNames[currentMonth]) + " " + String(currentYear);
  
  // Calcola la larghezza del testo per centrarlo
  int16_t tbx, tby;
  uint16_t tbw, tbh;
  display.getTextBounds(monthYearStr, 0, 0, &tbx, &tby, &tbw, &tbh);
  
  // Posiziona il testo centrato
  display.setCursor(x + (width - tbw) / 2, y + tbh + 5);
  display.print(monthYearStr);

  // Disegna i giorni della settimana
  display.setFont(&FreeSerif9pt7b);
  int daysRowY = y + headerHeight - 10;
  
  for (int i = 0; i < 7; i++) {
    int dayX = x + i * cellWidth + cellWidth / 2;
    
    // Calcola la larghezza del testo per centrarlo
    display.getTextBounds(dayNames[i], 0, 0, &tbx, &tby, &tbw, &tbh);
    
    display.setCursor(dayX - tbw / 2, daysRowY);
    display.print(dayNames[i]);
  }

  // Disegna la griglia dei giorni
  int day = 1;
  display.setFont(NULL); // Torna al font predefinito
  
  for (int row = 0; row < 6 && day <= daysInMonth; row++) {
    for (int col = 0; col < 7; col++) {
      // Salta le posizioni prima del primo giorno del mese
      if (row == 0 && col < firstDay) {
        continue;
      }
      
      if (day <= daysInMonth) {
        int cellCenterX = x + col * cellWidth + cellWidth / 2;
        int cellCenterY = y + headerHeight + row * cellHeight + cellHeight / 2;
        
        // Evidenzia il giorno corrente
        if (day == currentDay) {
          int squareSize = min(cellWidth, cellHeight) - 4;
          display.fillRect(cellCenterX - squareSize/2, cellCenterY - squareSize/2, squareSize, squareSize, GxEPD_BLACK);
          display.setTextColor(GxEPD_WHITE);
        } else {
          display.setTextColor(GxEPD_BLACK);
        }
        
        // Converte il giorno in stringa
        String dayStr = String(day);
        
        // Calcola la larghezza del testo per centrarlo
        display.getTextBounds(dayStr, 0, 0, &tbx, &tby, &tbw, &tbh);
        
        // Posiziona il testo centrato nella cella
        display.setCursor(cellCenterX - tbw/2, cellCenterY + 3);
        display.print(dayStr);
        
        // Ripristina il colore del testo
        display.setTextColor(GxEPD_BLACK);
        
        day++;
      }
    }
  }
}
