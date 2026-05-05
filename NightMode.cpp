#include "NightMode.h"
#include "Config.h"

// Istanza globale
NightMode nightMode;

NightMode::NightMode() {
    // Configurazione predefinita
    config.enabled = true;
    config.startHour = 22;  // 22:00
    config.endHour = 7;     // 07:00
    config.autoThemeSwitch = true;
    config.dayTheme = "enhanced";
    config.nightTheme = "icon_quote";
    config.reducedUpdates = true;
    
    isNightTime = false;
    lastCheck = 0;
}

void NightMode::begin(NightModeConfig cfg) {
    config = cfg;
    forceCheck();
}

bool NightMode::isNight() {
    return isNightTime && config.enabled;
}

void NightMode::update() {
    // Controlla solo ogni CHECK_INTERVAL
    if (millis() - lastCheck < CHECK_INTERVAL) {
        return;
    }
    
    forceCheck();
}

void NightMode::forceCheck() {
    lastCheck = millis();
    
    if (!config.enabled) {
        isNightTime = false;
        return;
    }
    
    // Ottieni ora corrente
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("[NIGHT MODE] Impossibile ottenere ora corrente");
        return;
    }
    
    int currentHour = timeinfo.tm_hour;
    
    // Determina se è notte
    // Caso 1: Start < End (es: 22-7 passa per mezzanotte)
    if (config.startHour > config.endHour) {
        isNightTime = (currentHour >= config.startHour || currentHour < config.endHour);
    }
    // Caso 2: Start < End (es: 13-15 pomeriggio)
    else {
        isNightTime = (currentHour >= config.startHour && currentHour < config.endHour);
    }
    
    // Log cambio stato
    static bool lastNightState = false;
    if (isNightTime != lastNightState) {
        Serial.print("[NIGHT MODE] Cambio stato: ");
        Serial.println(isNightTime ? "NOTTE" : "GIORNO");
        lastNightState = isNightTime;
    }
}

const char* NightMode::getCurrentTheme() {
    if (!config.enabled || !config.autoThemeSwitch) {
        return config.dayTheme;
    }
    
    return isNightTime ? config.nightTheme : config.dayTheme;
}

int NightMode::getUpdateInterval(int dayInterval, int nightInterval) {
    if (!config.enabled || !config.reducedUpdates) {
        return dayInterval;
    }
    
    return isNightTime ? nightInterval : dayInterval;
}

void NightMode::setEnabled(bool enabled) {
    config.enabled = enabled;
    Serial.print("[NIGHT MODE] ");
    Serial.println(enabled ? "ABILITATO" : "DISABILITATO");
    forceCheck();
}

void NightMode::setHours(int start, int end) {
    config.startHour = start;
    config.endHour = end;
    Serial.printf("[NIGHT MODE] Orari impostati: %02d:00 - %02d:00\n", start, end);
    forceCheck();
}
