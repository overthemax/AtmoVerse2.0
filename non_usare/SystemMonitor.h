#ifndef SYSTEM_MONITOR_H
#define SYSTEM_MONITOR_H

/**
 * @file SystemMonitor.h
 * @brief Utilità per il monitoraggio dello stato di salute del sistema
 * 
 * Questo file contiene classi e funzioni per monitorare lo stato di salute
 * dei vari componenti del sistema e del sistema nel complesso.
 */

#include <Arduino.h>
#include "Debug.h"
#include "DebugUtils.h"
#include "RetryUtils.h"

// Definizione di costanti per i componenti del sistema
enum SystemComponent {
  COMPONENT_WIFI,
  COMPONENT_WEATHER_API,
  COMPONENT_SD_CARD,
  COMPONENT_DISPLAY,
  COMPONENT_WEB_SERVER,
  COMPONENT_COUNT // Deve essere l'ultimo elemento
};

// Definizione di stati del sistema
enum SystemState {
  SYSTEM_NORMAL,      // Tutti i componenti funzionano normalmente
  SYSTEM_DEGRADED,    // Alcuni componenti hanno problemi ma il sistema funziona
  SYSTEM_CRITICAL,    // Problemi gravi richiedono attenzione
  SYSTEM_EMERGENCY    // Stato di emergenza, riavvio consigliato
};

/**
 * @brief Classe per il monitoraggio dello stato di salute del sistema
 * 
 * Permette di monitorare lo stato dei vari componenti critici
 * e fornisce una valutazione complessiva della salute del sistema.
 */
class SystemMonitor {
public:
  /**
   * @brief Inizializza il monitor di sistema
   */
  static void init() {
    // Inizializza i monitor di stabilità per ogni componente
    for (int i = 0; i < COMPONENT_COUNT; i++) {
      _componentMonitors[i] = new StabilityMonitor(3, 5); // 3 errori per instabilità, 5 successi per recupero
      _componentStatus[i] = true; // All'avvio tutti i componenti sono considerati funzionanti
    }
    // Inizializza il tempo dell'ultimo report
    _lastReportTime = millis();
    
    DEBUG_LOG(DEBUG_CATEGORY_SYSTEM, DEBUG_LEVEL_INFO, "SystemMonitor inizializzato");
  }
  
  /**
   * @brief Legge la tensione della batteria (VBAT) tramite ADC
   * @return Tensione in Volt, oppure < 0 in caso di errore/non disponibile
   */
  static float getBatteryVoltage();

  /**
   * @brief Converte una tensione LiPo in percentuale stimata (grezza)
   * @param voltage Tensione VBAT in Volt
   * @return Percentuale 0..100
   */
  static int getBatteryPercent(float voltage);
  
  /**
   * @brief Registra un evento di successo per un componente
   * @param component Componente che ha avuto successo
   */
  static void recordSuccess(SystemComponent component) {
    if (component >= COMPONENT_COUNT) return;
    
    bool wasStable = _componentMonitors[component]->isStable();
    _componentMonitors[component]->recordSuccess();
    _componentStatus[component] = true;
    
    // Se il componente era instabile ma ora è stabile, log del recupero
    if (!wasStable && _componentMonitors[component]->isStable()) {
      DEBUG_LOG(DEBUG_CATEGORY_SYSTEM, DEBUG_LEVEL_INFO, 
                "Componente %s: RIPRISTINATO dopo %d successi consecutivi", 
                getComponentName(component),
                _componentMonitors[component]->getConsecutiveSuccesses());
    }
  }
  
  /**
   * @brief Registra un evento di errore per un componente
   * @param component Componente che ha avuto un errore
   * @param errorMessage Messaggio di errore opzionale
   */
  static void recordError(SystemComponent component, const char* errorMessage = nullptr) {
    if (component >= COMPONENT_COUNT) return;
    
    bool wasStable = _componentMonitors[component]->isStable();
    _componentMonitors[component]->recordError();
    _componentStatus[component] = false;
    
    // Se il componente era stabile ma ora è instabile, log di avviso
    if (wasStable && !_componentMonitors[component]->isStable()) {
      DEBUG_LOG(DEBUG_CATEGORY_SYSTEM, DEBUG_LEVEL_WARNING, 
                "Componente %s: INSTABILE dopo %d errori consecutivi %s", 
                getComponentName(component),
                _componentMonitors[component]->getConsecutiveErrors(),
                errorMessage ? errorMessage : "");
    }
  }
  
  /**
   * @brief Ottiene lo stato complessivo del sistema
   * @return Stato del sistema (NORMAL, DEGRADED, CRITICAL, EMERGENCY)
   */
  static SystemState getSystemState() {
    int unstableCount = 0;
    bool criticalComponentDown = false;
    
    // Controlla quanti componenti sono instabili
    for (int i = 0; i < COMPONENT_COUNT; i++) {
      if (!_componentMonitors[i]->isStable()) {
        unstableCount++;
        
        // Alcuni componenti sono considerati critici
        if (i == COMPONENT_WIFI || i == COMPONENT_DISPLAY) {
          criticalComponentDown = true;
        }
      }
    }
    
    if (unstableCount == 0) {
      return SYSTEM_NORMAL;
    } else if (unstableCount <= 1 && !criticalComponentDown) {
      return SYSTEM_DEGRADED;
    } else if (criticalComponentDown || unstableCount >= 3) {
      return SYSTEM_CRITICAL;
    } else {
      return SYSTEM_DEGRADED;
    }
  }
  
  /**
   * @brief Verifica se un componente è considerato stabile
   * @param component Componente da verificare
   * @return true se il componente è stabile, false altrimenti
   */
  static bool isComponentStable(SystemComponent component) {
    if (component >= COMPONENT_COUNT) return false;
    return _componentMonitors[component]->isStable();
  }
  
  /**
   * @brief Ottiene il nome di un componente come stringa
   * @param component Componente di cui ottenere il nome
   * @return Puntatore a stringa con il nome del componente
   */
  static const char* getComponentName(SystemComponent component) {
    switch(component) {
      case COMPONENT_WIFI: return "WiFi";
      case COMPONENT_WEATHER_API: return "Weather API";
      case COMPONENT_SD_CARD: return "SD Card";
      case COMPONENT_DISPLAY: return "Display";
      case COMPONENT_WEB_SERVER: return "Web Server";
      default: return "Sconosciuto";
    }
  }
  
  /**
   * @brief Ottiene il nome di uno stato come stringa
   * @param state Stato di cui ottenere il nome
   * @return Puntatore a stringa con il nome dello stato
   */
  static const char* getStateName(SystemState state) {
    switch(state) {
      case SYSTEM_NORMAL: return "NORMALE";
      case SYSTEM_DEGRADED: return "DEGRADATO";
      case SYSTEM_CRITICAL: return "CRITICO";
      case SYSTEM_EMERGENCY: return "EMERGENZA";
      default: return "SCONOSCIUTO";
    }
  }
  
  /**
   * @brief Stampa un report dello stato del sistema
   * @param forceOutput Se true, forza l'output anche se non è passato abbastanza tempo dall'ultimo report
   */
  static void printStatusReport(bool forceOutput = false) {
    // Limita la frequenza dei report a uno ogni minuto, a meno che non sia forzato
    unsigned long currentTime = millis();
    if (!forceOutput && (currentTime - _lastReportTime < 60000)) {
      return;
    }
    _lastReportTime = currentTime;
    
    SystemState state = getSystemState();
    DEBUG_LOG(DEBUG_CATEGORY_SYSTEM, DEBUG_LEVEL_INFO, 
              "========= REPORT STATO SISTEMA =========");
    DEBUG_LOG(DEBUG_CATEGORY_SYSTEM, DEBUG_LEVEL_INFO, 
              "Stato generale: %s", getStateName(state));
    
    // Report per ogni componente
    for (int i = 0; i < COMPONENT_COUNT; i++) {
      DEBUG_LOG(DEBUG_CATEGORY_SYSTEM, DEBUG_LEVEL_INFO, 
                "%-12s: %s (Errori: %d, Successi: %d)", 
                getComponentName((SystemComponent)i),
                _componentMonitors[i]->isStable() ? "OK" : "PROBLEMI",
                _componentMonitors[i]->getConsecutiveErrors(),
                _componentMonitors[i]->getConsecutiveSuccesses());
    }
    DEBUG_LOG(DEBUG_CATEGORY_SYSTEM, DEBUG_LEVEL_INFO, 
              "=======================================");
  }

private:
  // Array di monitor di stabilità per ogni componente
  static StabilityMonitor* _componentMonitors[COMPONENT_COUNT];
  
  // Stato attuale di ogni componente (true = funzionante, false = non funzionante)
  static bool _componentStatus[COMPONENT_COUNT];
  
  // Timestamp dell'ultimo report di stato
  static unsigned long _lastReportTime;
};

// Le variabili statiche sono definite in SystemMonitor.cpp

#endif // SYSTEM_MONITOR_H
