#include "ButtonUtils.h"
#include "Config.h"
#include "Display.h"
#include <SD.h>

// Pin collegato al pulsante di reset
#define RESET_BUTTON_PIN 0  // Tipicamente è GPIO0 o un altro pin definito nell'hardware

// Durata minima per il reset (in millisecondi)
#define RESET_PRESS_DURATION 5000  // 5 secondi per il reset

// Variabili per la gestione del pulsante
static unsigned long buttonPressStartTime = 0;
static bool buttonWasPressed = false;
static bool resetInProgress = false;

/**
 * @brief Verifica se il pulsante di reset è premuto
 * 
 * Controlla lo stato del pulsante di reset e avvia la procedura
 * di reset della configurazione se il pulsante è premuto a lungo.
 */
void checkResetButton() {
  // Leggi lo stato del pulsante (LOW quando premuto, assumendo pull-up)
  bool buttonPressed = (digitalRead(RESET_BUTTON_PIN) == LOW);
  
  // Se il pulsante è appena stato premuto, salva il tempo
  if (buttonPressed && !buttonWasPressed) {
    buttonPressStartTime = millis();
    buttonWasPressed = true;
  }
  
  // Se il pulsante è rilasciato, resetta il flag
  if (!buttonPressed && buttonWasPressed) {
    buttonWasPressed = false;
    resetInProgress = false;
  }
  
  // Se il pulsante è tenuto premuto per il tempo richiesto
  if (buttonPressed && buttonWasPressed && !resetInProgress) {
    unsigned long pressDuration = millis() - buttonPressStartTime;
    
    if (pressDuration >= RESET_PRESS_DURATION) {
      // Reset della configurazione
      resetInProgress = true;
      Serial.println("Pulsante di reset premuto a lungo: avvio reset configurazione");
      
      // Rimuovere il file di configurazione
      if (SD.exists("/config.json")) {
        SD.remove("/config.json");
        Serial.println("File di configurazione rimosso");
      }
      
      // Mostra messaggi sul display
      showStatusOnDisplay("Configurazione resettata. Il dispositivo si riavvierà...");
      
      // Breve attesa per rendere visibile il messaggio
      delay(2000);
      
      // Riavvia ESP32
      ESP.restart();
    }
  }
}
