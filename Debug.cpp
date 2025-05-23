/**
 * @file Debug.cpp
 * @brief Implementazione del sistema di debug asincrono multi-core
 * 
 * Questo file contiene l'implementazione del sistema di debug asincrono
 * che sfrutta il secondo core dell'ESP32 per gestire i messaggi di debug
 * senza interferire con le operazioni principali dell'applicazione.
 */

#include "Debug.h"

// ==========================================
// Variabili globali per il sistema di debug
// ==========================================

/** Coda dei messaggi di debug in attesa di essere processati */
QueueHandle_t debugQueue = NULL;

/** Mutex per l'accesso esclusivo a Serial */
SemaphoreHandle_t serialMutex = NULL;

/** Handle del task di debug che gira sul Core 1 */
TaskHandle_t DebugTask = NULL;

/** Flag di inizializzazione del sistema di debug */
volatile bool debugSystemInitialized = false;

/**
 * @brief Funzione principale del task di debug
 * 
 * Questa funzione viene eseguita nel contesto del task di debug sul Core 1.
 * Si occupa di leggere i messaggi dalla coda di debug e stamparli su Serial.
 * Lavora in modo completamente asincrono rispetto alle altre operazioni.
 * 
 * @param parameter Parametri del task (non utilizzati)
 */
void debugTaskFunction(void * parameter) {
  // Notifica di avvio del task con informazione sul core corrente
  if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
    Serial.print("[SYSTEM] ");
    Serial.print("Task Debug avviato sul Core ");
    Serial.println(xPortGetCoreID());
    xSemaphoreGive(serialMutex);
  }
  
  // Buffer per il messaggio ricevuto dalla coda
  char message[DEBUG_MESSAGE_SIZE];
  
  // Loop principale del task debug - continua per tutta la vita del sistema
  while(true) {
    // Aspetta messaggi dalla coda di debug con timeout di 1 secondo
    if (xQueueReceive(debugQueue, message, pdMS_TO_TICKS(1000)) == pdPASS) {
      // Abbiamo ricevuto un messaggio, lo stampiamo proteggendo l'accesso a Serial
      if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
        Serial.println(message);
        xSemaphoreGive(serialMutex);
      }
    }
    
    // Piccolo delay per non monopolizzare la CPU e dare spazio ad altri task
    vTaskDelay(5 / portTICK_PERIOD_MS);
  }
}

/**
 * @brief Inizializza il sistema di debug asincrono
 * 
 * Configura la porta seriale, crea il mutex per proteggere l'accesso
 * a Serial e inizializza la coda dei messaggi di debug.
 * Questa funzione deve essere chiamata durante il setup iniziale,
 * prima di qualsiasi altra operazione di debug.
 */
void initDebugSystem() {
  // Inizializzazione Serial (solo se non è già stato fatto)
  if (!Serial) {
    Serial.begin(115200);
    while(!Serial && millis() < 3000); // Aspetta al massimo 3 secondi per evitare blocchi indefiniti
  }
  
  // Crea il mutex per l'accesso esclusivo a Serial
  // Questo è fondamentale per evitare conflitti tra i diversi task
  serialMutex = xSemaphoreCreateMutex();
  if (serialMutex == NULL) {
    Serial.println("ERRORE: Impossibile creare serialMutex");
    return;
  }
  
  // Crea la coda di messaggi di debug con dimensione definita in Debug.h
  // Questa coda permette la comunicazione asincrona tra i task e il task di debug
  debugQueue = xQueueCreate(DEBUG_QUEUE_SIZE, DEBUG_MESSAGE_SIZE);
  if (debugQueue == NULL) {
    Serial.println("ERRORE: Impossibile creare debugQueue");
    return;
  }
  
  Serial.println("Sistema di debug inizializzato correttamente");
}

/**
 * @brief Avvia il task di debug sul Core 1
 * 
 * Crea un task FreeRTOS dedicato al debug e lo assegna al Core 1,
 * separandolo dalle operazioni principali che avvengono sul Core 0.
 * Il task ha una priorità elevata per garantire che i messaggi di debug
 * vengano processati in modo tempestivo.
 */
void startDebugTask() {
  // Crea il task di debug e lo assegna al Core 1
  xTaskCreatePinnedToCore(
    debugTaskFunction,      // Funzione che implementa il task
    "DebugTask",           // Nome del task (per identificazione)
    4096,                  // Dimensione dello stack in bytes
    NULL,                  // Parametri passati al task (nessuno)
    2,                     // Priorità (0-24, con 24 massima priorità)
    &DebugTask,            // Puntatore alla variabile che conterrà l'handle del task
    1                      // Core dove eseguire (1 = second core dell'ESP32)
  );
  
  // Breve delay per permettere al task debug di inizializzare
  // Questo evita race condition durante l'avvio del sistema
  delay(100);
  
  // Segna il sistema di debug come completamente inizializzato
  // Da questo momento in poi i messaggi verranno inviati alla coda
  debugSystemInitialized = true;
  
  Serial.println("Task debug avviato sul Core 1");
}

/**
 * @brief Invia un messaggio di debug al sistema asincrono
 * 
 * Questa funzione invia un messaggio di debug alla coda, dove verrà
 * processato dal task di debug in background. Se il sistema non è ancora
 * inizializzato, stampa direttamente su Serial per evitare perdita di
 * messaggi durante la fase di avvio.
 * 
 * @param module Nome del modulo che genera il messaggio (es. "SYSTEM")
 * @param message Messaggio di debug da visualizzare
 */
void debugPrint(const char* module, const char* message) {
  if (!debugSystemInitialized) {
    // Durante l'inizializzazione o se il sistema non è ancora attivo, stampa direttamente
    // Questo garantisce che i messaggi di debug critici durante l'avvio non vengano persi
    if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
      Serial.print("[");
      Serial.print(module);
      Serial.print("] ");
      Serial.println(message);
      xSemaphoreGive(serialMutex);
    }
    return;
  }
  
  // Crea il messaggio completo con prefisso del modulo
  char fullMessage[DEBUG_MESSAGE_SIZE];
  snprintf(fullMessage, DEBUG_MESSAGE_SIZE, "[%s] %s", module, message);
  
  // Invia alla coda con timeout di 10ms (non bloccare se la coda è piena)
  // Questo è fondamentale per non rallentare il sistema principale
  if (xQueueSend(debugQueue, fullMessage, pdMS_TO_TICKS(10)) != pdPASS) {
    // Se la coda è piena, il messaggio viene scartato senza bloccare il chiamante
    // Questo è un compromesso accettabile per garantire le prestazioni del sistema
  }
}

/**
 * @brief Versione formattata della funzione di debug
 * 
 * Funziona come printf, accettando una stringa di formato e parametri
 * variabili. Formatta il messaggio e poi lo invia al sistema di debug.
 * 
 * @param module Nome del modulo che genera il messaggio
 * @param format Stringa di formato (stile printf)
 * @param ... Parametri variabili corrispondenti ai segnaposto nel formato
 */
void debugPrintf(const char* module, const char* format, ...) {
  // Riserva spazio per il prefisso del modulo (20 caratteri)
  char buffer[DEBUG_MESSAGE_SIZE - 20]; 
  
  // Gestione dei parametri variabili (va_list)
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  
  // Invia il messaggio formattato al sistema di debug
  debugPrint(module, buffer);
}
