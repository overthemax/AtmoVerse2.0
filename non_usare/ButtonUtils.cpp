#include "ButtonUtils.h"
#include "Config.h"
#include "Display.h"
#include "Hardware.h"
#include <SD.h>

// Usa il pin unificato definito in Hardware.h: RESET_BUTTON_PIN (GPIO35, input-only)

// Durata reset (ms)
#define RESET_PRESS_DURATION 5000

// Stringhe in PROGMEM per risparmiare spazio programma
const char RESET_MESSAGE[] PROGMEM = "Pulsante di reset premuto a lungo: avvio reset configurazione";
const char CONFIG_REMOVED[] PROGMEM = "File di configurazione rimosso";
const char RESTART_MESSAGE[] PROGMEM = "Configurazione resettata. Il dispositivo si riavvierà...";
const char CONFIG_PATH[] PROGMEM = "/config.json";
const char INIT_BUTTONS_MSG[] PROGMEM = "Inizializzazione pulsanti...";
const char BUTTON_INIT_ERR[] PROGMEM = "Errore inizializzazione pulsante di reset";

// Variabili a dimensione ottimizzata
static uint32_t buttonPressStartTime = 0;
static uint8_t buttonState = 0; // 0=non premuto, 1=premuto, 2=reset in corso

/**
 * @brief Inizializza i pin dei pulsanti
 */
void initButtons() {
  #ifdef DEBUG_MODE
  Serial.println(FPSTR(INIT_BUTTONS_MSG));
  #endif
  
  // GPIO35 non supporta PULLUP/PULLDOWN interni: richiede pull-up esterno
  pinMode(RESET_BUTTON_PIN, INPUT);
  
  // Verifica che il pin sia configurato correttamente
  if (digitalRead(RESET_BUTTON_PIN) == LOW) {
    #ifdef DEBUG_MODE
    Serial.println(FPSTR(BUTTON_INIT_ERR));
    #endif
  }
  
  // La SD verrà inizializzata altrove nel codice principale
}

/**
 * @brief Verifica se il pulsante di reset è premuto
 */
void checkResetButton() {
  // Leggi stato pulsante (LOW quando premuto, con pull-up)
  bool buttonPressed = (digitalRead(RESET_BUTTON_PIN) == LOW);
  
  // Gestione stato del pulsante
  switch(buttonState) {
    case 0: // Non premuto
      if (buttonPressed) {
        buttonPressStartTime = millis();
        buttonState = 1;
      }
      break;
    
    case 1: // Premuto, in attesa
      if (!buttonPressed) {
        buttonState = 0; // Rilasciato prima del tempo
      } else if (millis() - buttonPressStartTime >= RESET_PRESS_DURATION) {
        buttonState = 2; // Reset in corso
        
        // Messaggio debug
        #ifdef DEBUG_MODE
        Serial.println(FPSTR(RESET_MESSAGE));
        #endif
        
        // Rimuovere file configurazione
        char configPath[32];  // Buffer più grande per il percorso
        strcpy_P(configPath, CONFIG_PATH);
        
        // Rimuovi il file di configurazione dalla SD
        if (SD.exists(configPath)) {
          SD.remove(configPath);
          #ifdef DEBUG_MODE
          Serial.println(FPSTR(CONFIG_REMOVED));
          #endif
        }
        
        // Messaggio display
        char displayMsg[50];
        strcpy_P(displayMsg, RESTART_MESSAGE);
        showStatusOnDisplay(displayMsg);
        
        // Attesa breve e riavvio
        delay(1000); // Ridotto a 1 secondo
        ESP.restart();
      }
      break;
      
    case 2: // Reset in corso
      if (!buttonPressed) {
        buttonState = 0;
      }
      break;
  }
}
