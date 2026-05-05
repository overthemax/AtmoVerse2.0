/**
 * @file TimerUtils.h
 * @brief Utility per gestire temporizzazioni non bloccanti
 * 
 * Questa libreria fornisce funzioni per gestire il timing in modo non bloccante,
 * sostituendo l'uso di delay() che blocca l'esecuzione del codice.
 */

#ifndef TIMER_UTILS_H
#define TIMER_UTILS_H

#include <Arduino.h>

/**
 * @class Timer
 * @brief Classe per gestire intervalli di tempo senza usare delay()
 */
class Timer {
  private:
    unsigned long _lastTime;    // Ultimo timestamp registrato
    unsigned long _interval;    // Intervallo in millisecondi
    bool _enabled;              // Se il timer è abilitato

  public:
    /**
     * @brief Costruttore del timer
     * @param interval Intervallo in millisecondi
     * @param startEnabled Se il timer deve essere abilitato all'avvio
     */
    Timer(unsigned long interval = 1000, bool startEnabled = true) {
      _interval = interval;
      _lastTime = millis();
      _enabled = startEnabled;
    }

    /**
     * @brief Verifica se è trascorso l'intervallo impostato
     * @param autoReset Se true, resetta automaticamente il timer quando scatta
     * @return true se l'intervallo è trascorso, false altrimenti
     */
    bool isReady(bool autoReset = true) {
      if (!_enabled) return false;
      
      unsigned long currentTime = millis();
      if (currentTime - _lastTime >= _interval) {
        if (autoReset) _lastTime = currentTime;
        return true;
      }
      return false;
    }

    /**
     * @brief Resetta il timer
     */
    void reset() {
      _lastTime = millis();
    }

    /**
     * @brief Modifica l'intervallo del timer
     * @param interval Nuovo intervallo in millisecondi
     */
    void setInterval(unsigned long interval) {
      _interval = interval;
    }

    /**
     * @brief Ottiene l'intervallo attuale
     * @return Intervallo in millisecondi
     */
    unsigned long getInterval() const {
      return _interval;
    }

    /**
     * @brief Abilita il timer
     */
    void enable() {
      _enabled = true;
    }

    /**
     * @brief Disabilita il timer
     */
    void disable() {
      _enabled = false;
    }

    /**
     * @brief Verifica se il timer è abilitato
     * @return true se abilitato, false altrimenti
     */
    bool isEnabled() const {
      return _enabled;
    }

    /**
     * @brief Restituisce il tempo rimanente prima del prossimo trigger
     * @return Millisecondi rimanenti o 0 se già scattato
     */
    unsigned long remainingTime() const {
      if (!_enabled) return _interval;
      
      unsigned long elapsed = millis() - _lastTime;
      return (elapsed >= _interval) ? 0 : (_interval - elapsed);
    }

    /**
     * @brief Restituisce la percentuale di completamento del timer
     * @return Percentuale da 0 a 100
     */
    uint8_t percentComplete() const {
      if (!_enabled) return 0;
      
      unsigned long elapsed = millis() - _lastTime;
      return (elapsed >= _interval) ? 100 : (elapsed * 100 / _interval);
    }
};

/**
 * @class DelayReplacement
 * @brief Classe per sostituire delay() con approccio non bloccante
 * 
 * Esempio di utilizzo:
 * ```
 * DelayReplacement nonBlockingDelay(1000);
 * if (nonBlockingDelay.start()) {
 *   // Esegui operazione che richiederebbe delay(1000)
 *   while (!nonBlockingDelay.isFinished()) {
 *     // Esegui altre operazioni mentre "attendi"
 *   }
 * }
 * ```
 */
class DelayReplacement {
  private:
    unsigned long _startTime;
    unsigned long _duration;
    bool _active;

  public:
    /**
     * @brief Costruttore
     * @param duration Durata in millisecondi
     */
    DelayReplacement(unsigned long duration = 1000) {
      _duration = duration;
      _active = false;
    }

    /**
     * @brief Avvia il conteggio del tempo
     * @return true se l'operazione è stata avviata, false se era già attiva
     */
    bool start() {
      if (_active) return false;
      
      _startTime = millis();
      _active = true;
      return true;
    }

    /**
     * @brief Verifica se il tempo impostato è trascorso
     * @param autoReset Se true, resetta automaticamente quando finisce
     * @return true se il tempo è trascorso, false altrimenti
     */
    bool isFinished(bool autoReset = true) {
      if (!_active) return false;
      
      if (millis() - _startTime >= _duration) {
        if (autoReset) _active = false;
        return true;
      }
      return false;
    }

    /**
     * @brief Annulla l'operazione in corso
     */
    void cancel() {
      _active = false;
    }

    /**
     * @brief Modifica la durata
     * @param duration Nuova durata in millisecondi
     */
    void setDuration(unsigned long duration) {
      _duration = duration;
    }
};

#endif // TIMER_UTILS_H
