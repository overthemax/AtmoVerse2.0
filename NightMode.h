#ifndef NIGHTMODE_H
#define NIGHTMODE_H

#include <Arduino.h>
#include <time.h>

// Configurazione Night Mode
struct NightModeConfig {
    bool enabled;           // Night mode attivo
    int startHour;         // Ora inizio (es: 22)
    int endHour;           // Ora fine (es: 7)
    bool autoThemeSwitch;  // Cambia tema automaticamente
    const char* dayTheme;  // Tema diurno (es: "enhanced")
    const char* nightTheme; // Tema notturno (es: "icon_quote")
    bool reducedUpdates;   // Riduci frequenza aggiornamenti
};

// Classe per gestione Night Mode
class NightMode {
private:
    NightModeConfig config;
    bool isNightTime;
    unsigned long lastCheck;
    const unsigned long CHECK_INTERVAL = 60000; // Controlla ogni minuto
    
public:
    NightMode();
    
    // Inizializza con configurazione
    void begin(NightModeConfig cfg);
    
    // Controlla se è ora notturna
    bool isNight();
    
    // Aggiorna stato (chiamare nel loop)
    void update();
    
    // Ottieni tema appropriato per ora corrente
    const char* getCurrentTheme();
    
    // Ottieni intervallo aggiornamento appropriato
    int getUpdateInterval(int dayInterval, int nightInterval);
    
    // Forza controllo immediato
    void forceCheck();
    
    // Abilita/disabilita night mode
    void setEnabled(bool enabled);
    
    // Configura orari
    void setHours(int start, int end);
    
    // Ottieni configurazione corrente
    NightModeConfig getConfig() { return config; }
};

// Istanza globale
extern NightMode nightMode;

#endif // NIGHTMODE_H
