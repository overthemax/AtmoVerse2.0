#ifndef RETRY_UTILS_H
#define RETRY_UTILS_H

/**
 * @file RetryUtils.h
 * @brief Utilità per la gestione dei tentativi e resilienza del sistema
 * 
 * Questo file contiene classi e funzioni per migliorare la resilienza
 * del sistema attraverso una gestione strutturata dei tentativi di operazioni.
 */

#include <Arduino.h>
#include "DebugUtils.h"
// Debug.h rimosso perché ridondante con DebugUtils.h

/**
 * @brief Classe per la gestione avanzata dei tentativi di operazioni
 * 
 * Permette di definire un numero massimo di tentativi, intervalli
 * tra i tentativi con backoff esponenziale, e gestione della persistenza degli errori.
 */
class RetryManager {
public:
  /**
   * @brief Costruttore per RetryManager
   * @param maxAttempts Numero massimo di tentativi
   * @param initialDelayMs Ritardo iniziale tra i tentativi (ms)
   * @param maxDelayMs Ritardo massimo tra tentativi (ms)
   * @param expBackoffFactor Fattore di backoff esponenziale tra tentativi
   */
  RetryManager(int maxAttempts = 5, 
               unsigned long initialDelayMs = 1000, 
               unsigned long maxDelayMs = 60000, 
               float expBackoffFactor = 2.0) 
    : _maxAttempts(maxAttempts),
      _initialDelayMs(initialDelayMs),
      _maxDelayMs(maxDelayMs),
      _expBackoffFactor(expBackoffFactor) {
    reset();
  }

  /**
   * @brief Resetta il gestore dei tentativi
   */
  void reset() {
    _currentAttempt = 0;
    _lastAttemptTime = 0;
    _currentDelayMs = _initialDelayMs;
    _shouldRetry = true;
    _lastError = "";
  }

  /**
   * @brief Registra un tentativo fallito e determina se riprovarci
   * @param errorMessage Messaggio di errore da registrare
   * @return true se è possibile ritentare, false altrimenti
   */
  bool recordFailure(String errorMessage = "") {
    _currentAttempt++;
    _lastAttemptTime = millis();
    _lastError = errorMessage;
    
    // Determina se dovremmo riprovare
    _shouldRetry = (_currentAttempt < _maxAttempts);
    
    // Log dell'errore
    DEBUG_LOG(DEBUG_CATEGORY_SYSTEM, DEBUG_LEVEL_INFO, 
              "Tentativo %d/%d fallito: %s", 
              _currentAttempt, _maxAttempts, 
              _lastError.length() > 0 ? _lastError.c_str() : "Nessun dettaglio");
    
    // Calcola il prossimo ritardo con backoff esponenziale
    if (_shouldRetry) {
      // Cast esplicito per evitare errori di tipo con la funzione min
      float nextDelay = _currentDelayMs * _expBackoffFactor;
      _currentDelayMs = (nextDelay < _maxDelayMs) ? nextDelay : _maxDelayMs;
    }
    
    return _shouldRetry;
  }

  /**
   * @brief Verifica se è il momento giusto per un nuovo tentativo
   * @return true se è tempo di riprovare, false altrimenti
   */
  bool isTimeToRetry() {
    if (!_shouldRetry) return false;
    return (millis() - _lastAttemptTime >= _currentDelayMs);
  }

  /**
   * @brief Registra un tentativo riuscito
   */
  void recordSuccess() {
    reset();
  }

  /**
   * @brief Ottiene il numero di tentativi effettuati
   * @return Numero di tentativi effettuati
   */
  int getAttemptCount() const {
    return _currentAttempt;
  }

  /**
   * @brief Ottiene il ritardo attuale prima del prossimo tentativo
   * @return Ritardo in millisecondi
   */
  unsigned long getCurrentDelayMs() const {
    return _currentDelayMs;
  }

  /**
   * @brief Ottiene il messaggio dell'ultimo errore
   * @return String contenente il messaggio di errore
   */
  String getLastError() const {
    return _lastError;
  }

  /**
   * @brief Verifica se sono stati superati i tentativi massimi
   * @return true se i tentativi massimi sono stati superati
   */
  bool isMaxAttemptsExceeded() const {
    return _currentAttempt >= _maxAttempts;
  }

private:
  int _maxAttempts;
  int _currentAttempt;
  unsigned long _initialDelayMs;
  unsigned long _maxDelayMs;
  unsigned long _lastAttemptTime;
  unsigned long _currentDelayMs;
  float _expBackoffFactor;
  bool _shouldRetry;
  String _lastError;
};

/**
 * @brief Classe per il monitoraggio della stabilità di un componente
 * 
 * Monitora errori consecutivi e successi per determinare lo stato
 * di salute di un componente del sistema.
 */
class StabilityMonitor {
public:
  /**
   * @brief Costruttore per StabilityMonitor
   * @param errorThreshold Numero di errori consecutivi che determina instabilità
   * @param recoveryThreshold Numero di successi consecutivi per considerare il recupero
   */
  StabilityMonitor(int errorThreshold = 3, int recoveryThreshold = 5)
    : _errorThreshold(errorThreshold),
      _recoveryThreshold(recoveryThreshold),
      _consecutiveErrors(0),
      _consecutiveSuccesses(0),
      _isStable(true) {
  }

  /**
   * @brief Registra un errore e aggiorna lo stato di stabilità
   * @return true se il componente è considerato stabile, false altrimenti
   */
  bool recordError() {
    _consecutiveErrors++;
    _consecutiveSuccesses = 0;
    
    // Se superata la soglia, il componente diventa instabile
    if (_isStable && _consecutiveErrors >= _errorThreshold) {
      _isStable = false;
      DEBUG_LOG(DEBUG_CATEGORY_SYSTEM, DEBUG_LEVEL_WARNING, 
                "Componente instabile rilevato: %d errori consecutivi", _consecutiveErrors);
    }
    
    return _isStable;
  }

  /**
   * @brief Registra un successo e aggiorna lo stato di stabilità
   * @return true se il componente è considerato stabile, false altrimenti
   */
  bool recordSuccess() {
    _consecutiveErrors = 0;
    _consecutiveSuccesses++;
    
    // Se raggiunta la soglia di recupero, il componente torna stabile
    if (!_isStable && _consecutiveSuccesses >= _recoveryThreshold) {
      _isStable = true;
      DEBUG_LOG(DEBUG_CATEGORY_SYSTEM, DEBUG_LEVEL_INFO, 
                "Componente recuperato: %d successi consecutivi", _consecutiveSuccesses);
    }
    
    return _isStable;
  }

  /**
   * @brief Verifica se il componente è considerato stabile
   * @return true se stabile, false se instabile
   */
  bool isStable() const {
    return _isStable;
  }

  /**
   * @brief Ottiene il numero di errori consecutivi
   * @return Numero di errori consecutivi
   */
  int getConsecutiveErrors() const {
    return _consecutiveErrors;
  }

  /**
   * @brief Ottiene il numero di successi consecutivi
   * @return Numero di successi consecutivi
   */
  int getConsecutiveSuccesses() const {
    return _consecutiveSuccesses;
  }

  /**
   * @brief Resetta il monitor di stabilità
   */
  void reset() {
    _consecutiveErrors = 0;
    _consecutiveSuccesses = 0;
    _isStable = true;
  }

private:
  int _errorThreshold;
  int _recoveryThreshold;
  int _consecutiveErrors;
  int _consecutiveSuccesses;
  bool _isStable;
};

#endif // RETRY_UTILS_H
