#ifndef MEMORY_UTILS_H
#define MEMORY_UTILS_H

#include <Arduino.h>
#include <stdlib.h>

/**
 * @brief Classe di utilità per la gestione della memoria
 * 
 * Fornisce metodi per allocare memoria in modo ottimizzato,
 * usando PSRAM quando disponibile sull'ESP32
 */
class MemoryUtils {
public:
  /**
   * @brief Verifica se la PSRAM è disponibile e inizializzata
   * @return true se la PSRAM è disponibile
   */
  static bool isPsramAvailable() {
    #ifdef CONFIG_SPIRAM_SUPPORT
      return psramInit();
    #else
      return false;
    #endif
  }

  /**
   * @brief Alloca memoria preferibilmente in PSRAM se disponibile
   * @param size Dimensione in bytes da allocare
   * @return puntatore alla memoria allocata o nullptr in caso di errore
   */
  static void* allocateMemory(size_t size) {
    #ifdef CONFIG_SPIRAM_SUPPORT
      if (isPsramAvailable()) {
        // Prova ad allocare in PSRAM
        void* ptr = ps_malloc(size);
        if (ptr) {
          return ptr;
        }
      }
    #endif
    
    // Se PSRAM non disponibile o allocazione fallita, usa heap normale
    return malloc(size);
  }

  /**
   * @brief Libera la memoria allocata con allocateMemory
   * @param ptr Puntatore alla memoria da liberare
   */
  static void freeMemory(void* ptr) {
    if (ptr) {
      free(ptr);
    }
  }

  /**
   * @brief Stampa le informazioni sull'utilizzo della memoria
   */
  static void printMemoryInfo() {
    Serial.print(F("Heap libero: "));
    Serial.print(ESP.getFreeHeap() / 1024);
    Serial.println(F(" KB"));

    Serial.print(F("Heap minimo: "));
    Serial.print(ESP.getMinFreeHeap() / 1024);
    Serial.println(F(" KB"));

    #ifdef CONFIG_SPIRAM_SUPPORT
    if (isPsramAvailable()) {
      Serial.print(F("PSRAM libera: "));
      Serial.print(ESP.getFreePsram() / 1024);
      Serial.println(F(" KB"));

      Serial.print(F("PSRAM totale: "));
      Serial.print(ESP.getPsramSize() / 1024);
      Serial.println(F(" KB"));
    }
    #endif
  }
};

#endif // MEMORY_UTILS_H
