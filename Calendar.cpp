#include "Calendar.h"
#include "Display.h"
#include <Fonts/FreeSerif9pt7b.h>
#include <Fonts/FreeSerif12pt7b.h>

// Nomi dei mesi abbreviati in italiano
const char* monthNames[] = {"Gen", "Feb", "Mar", "Apr", "Mag", "Giu", "Lug", "Ago", "Set", "Ott", "Nov", "Dic"};

// Giorni della settimana abbreviati in italiano (iniziando da lunedì)
const char* dayNames[] = {"L", "M", "M", "G", "V", "S", "D"};

// Controlla se l'anno è bisestile
bool isLeapYear(int year) {
  return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

// Ottiene il numero di giorni in un mese specifico
int getDaysInMonth(int month, int year) {
  const int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  
  if (month == 1 && isLeapYear(year)) { // Febbraio in anno bisestile
    return 29;
  }
  
  return daysInMonth[month];
}

// Implementazione dell'algoritmo di Zeller per trovare il giorno della settimana
// Restituisce 0 per domenica, 1 per lunedì, ecc.
int getFirstDayOfMonth(int month, int year) {
  if (month < 3) {
    month += 12;
    year--;
  }
  
  int k = year % 100;
  int j = year / 100;
  
  int dayOfWeek = (1 + ((13 * (month + 1)) / 5) + k + (k / 4) + (j / 4) - (2 * j)) % 7;
  
  // Convertiamo l'output di Zeller (0=dom, 1=lun... 6=sab) per avere lunedì come primo giorno (0)
  // In pratica, dobbiamo spostare la domenica alla fine: (dom->6, lun->0, mar->1...)
  if (dayOfWeek == 0)
    return 6;  // La domenica diventa l'ultimo giorno (indice 6)
  else
    return dayOfWeek - 1; // Tutti gli altri giorni si spostano indietro di una posizione
}

// Disegna un mini-calendario sul display e-ink
void drawCalendar(int16_t x, int16_t y, uint16_t width, uint16_t height) {
  // Ottieni la data corrente
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);
  
  int currentDay = timeinfo.tm_mday;
  int currentMonth = timeinfo.tm_mon;    // 0-11
  int currentYear = timeinfo.tm_year + 1900;
  
  // Calcola i parametri del calendario
  int daysInMonth = getDaysInMonth(currentMonth, currentYear);
  int firstDay = getFirstDayOfMonth(currentMonth, currentYear);
  
  // Calcola se abbiamo bisogno di 5 o 6 righe per i giorni
  int numWeeks = ceil((firstDay + daysInMonth) / 7.0);
  numWeeks = min(numWeeks, 5); // Limitiamo a 5 righe massimo
  
  // Dimensioni celle - adattate dinamicamente in base al numero di righe
  int cellWidth = width / 7;
  // Detraggo l'altezza dell'intestazione (headerHeight) e divido lo spazio rimanente per il numero di righe necessarie
  int cellHeight = (height / (numWeeks + 2)); // +2 per intestazione mese e giorni settimana
  
  // Bordo del calendario rimosso per un design ancora più minimalista
  
  // Rendi più alta la prima riga per dare più spazio al titolo
  int headerHeight = cellHeight + 6;
  
  // Disegna l'intestazione con il nome del mese e anno
  display.setFont(&FreeSerif9pt7b); // Ridotto da 12pt a 9pt
  display.setTextColor(GxEPD_BLACK);
  
  // Titolo del mese con font ridotto
  String title = String(monthNames[currentMonth]) + " " + String(currentYear);
  int16_t tbx, tby;
  uint16_t tbw, tbh;
  display.getTextBounds(title, 0, 0, &tbx, &tby, &tbw, &tbh);
  // Posiziono il testo più in alto per allontanarlo dalla linea
  display.setCursor(x + (width - tbw) / 2, y + tbh + 4);
  display.print(title);
  
  // Linea sotto il titolo - spostata più in basso
  display.drawFastHLine(x, y + headerHeight, width, GxEPD_BLACK);
  
  // Disegna i nomi dei giorni della settimana
  display.setFont(&FreeSerif9pt7b);
  // Posizione della seconda riga (giorni della settimana)
  int daysRowY = y + headerHeight;
  
  for (int i = 0; i < 7; i++) {
    int16_t dbx, dby;
    uint16_t dbw, dbh;
    display.getTextBounds(dayNames[i], 0, 0, &dbx, &dby, &dbw, &dbh);
    display.setCursor(x + i * cellWidth + (cellWidth - dbw) / 2, daysRowY + dbh + 5); // Allineato con la nuova altezza dell'header
    display.print(dayNames[i]);
    
    // Rimosse le linee verticali tra le colonne per un aspetto più pulito
  }
  
  // Linea sotto i nomi dei giorni - allineata con il nuovo headerHeight
  int daysBottomY = daysRowY + cellHeight;
  display.drawFastHLine(x, daysBottomY, width, GxEPD_BLACK);
  
  // Disegna i numeri dei giorni
  int day = 1;
  
  // Usa font diversi per il giorno corrente e gli altri giorni
  display.setFont(&FreeSerif9pt7b); // Font più grande e leggibile
  
  // Rimuoviamo la griglia interna per un aspetto più pulito
  // Le linee orizzontali della griglia sono state rimosse

  for (int row = 0; row < 6 && day <= daysInMonth; row++) {
    for (int col = 0; col < 7 && day <= daysInMonth; col++) {
      // Salta le celle prima del primo giorno del mese
      if (row == 0 && col < firstDay) {
        continue;
      }
      
      // Rimosse le linee verticali per separare le celle per un aspetto più pulito
      
      // Calcola il centro della cella (senza offset arbitrari)
      int cellCenterX = x + col * cellWidth + cellWidth / 2;
      int cellCenterY = daysBottomY + row * cellHeight + cellHeight / 2;
      
      // Stampa il numero del giorno
      String dayStr = String(day);
      
      // Evidenzia il giorno corrente con un quadrato invece di un cerchio
      if (day == currentDay) {
        // Dimensione del quadrato leggermente inferiore alla cella per evitare sovrapposizioni
        int squareSize = min(cellWidth, cellHeight) / 1.5;
        // Disegna un quadrato bianco sotto per cancellare eventuali linee della griglia
        display.fillRect(cellCenterX - squareSize/2, cellCenterY - squareSize/2, squareSize, squareSize, GxEPD_WHITE);
        // Disegna il quadrato nero sopra
        display.fillRect(cellCenterX - squareSize/2, cellCenterY - squareSize/2, squareSize, squareSize, GxEPD_BLACK);
        display.setTextColor(GxEPD_WHITE);
        
        // Usa lo stesso font degli altri giorni
        display.setFont(NULL);
      } else {
        display.setTextColor(GxEPD_BLACK);
        // Mantieni il font predefinito per tutti i giorni
        display.setFont(NULL);
      }
      
      // Calcola la bounding box del testo e centra correttamente
      int16_t tbx, tby;
      uint16_t tbw, tbh;
      display.getTextBounds(dayStr, 0, 0, &tbx, &tby, &tbw, &tbh);
      // Correggi l'origine usando tbx/tby per un centraggio preciso
      int textX = cellCenterX - (tbw / 2) - tbx;
      int textY = cellCenterY - (tbh / 2) - tby;
      display.setCursor(textX, textY);
      display.print(dayStr);
      
      // Ripristina il colore e il font predefinito
      display.setTextColor(GxEPD_BLACK);
      display.setFont(NULL);
      
      day++;
    }
  }
}
