#ifndef WEATHER_ALERTS_H
#define WEATHER_ALERTS_H

#include <Arduino.h>
#include "WeatherUtils.h"

// Tipi di alert
enum AlertType {
    ALERT_NONE = 0,
    ALERT_TEMP_HIGH,        // Temperatura troppo alta
    ALERT_TEMP_LOW,         // Temperatura troppo bassa
    ALERT_WIND_HIGH,        // Vento forte
    ALERT_RAIN,             // Pioggia
    ALERT_SNOW,             // Neve
    ALERT_STORM,            // Temporale
    ALERT_TORNADO,          // Tornado
    ALERT_HURRICANE,        // Uragano/Tempesta tropicale
    ALERT_HEAT_WAVE,        // Ondata di calore
    ALERT_COLD_WAVE,        // Ondata di freddo
    ALERT_EXTREME_WEATHER   // Altre condizioni estreme
};

// Priorità alert
enum AlertPriority {
    PRIORITY_INFO = 0,      // Informativo
    PRIORITY_WARNING = 1,   // Avviso
    PRIORITY_CRITICAL = 2   // Critico
};

// Struttura singolo alert
struct WeatherAlert {
    AlertType type;
    AlertPriority priority;
    String message;
    unsigned long timestamp;
    bool acknowledged;      // Se utente ha visto
    bool active;           // Se condizione ancora presente
};

// Configurazione soglie alert
struct AlertThresholds {
    bool enabled;
    
    // Temperatura
    float tempHigh;         // Alert se temp > questo (°C)
    float tempLow;          // Alert se temp < questo (°C)
    
    // Vento
    float windHigh;         // Alert se vento > questo (km/h)
    
    // Umidità
    float humidityHigh;     // Alert se umidità > questo (%)
    float humidityLow;      // Alert se umidità < questo (%)
    
    // Weather ID specifici
    bool alertRain;         // Alert per pioggia
    bool alertSnow;         // Alert per neve
    bool alertStorm;        // Alert per temporale
    bool alertFog;          // Alert per nebbia
    
    // Notifiche
    bool showOnDisplay;     // Mostra icona su display
    bool sendWebhook;       // Invia webhook
    String webhookUrl;      // URL webhook
};

// Classe per gestione alert meteo
class WeatherAlerts {
private:
    AlertThresholds thresholds;
    WeatherAlert alerts[5];  // Max 5 alert contemporanei
    int alertCount;
    
    unsigned long lastCheck;
    const unsigned long CHECK_INTERVAL = 60000;  // Controlla ogni minuto
    
    // Metodi privati
    void checkTemperatureAlerts();
    void checkWindAlerts();
    void checkWeatherConditionAlerts();
    void addAlert(AlertType type, AlertPriority priority, String message);
    void cleanOldAlerts();
    void sendWebhookNotification(WeatherAlert& alert);
    
public:
    WeatherAlerts();
    
    // Inizializza con configurazione
    void begin(AlertThresholds cfg);
    
    // Controlla condizioni meteo e genera alert
    void checkAlerts();
    
    // Ottieni alert attivi
    int getActiveAlertsCount();
    WeatherAlert* getActiveAlerts();
    bool hasActiveAlerts() { return alertCount > 0; }
    bool hasCriticalAlerts();
    
    // Gestione alert
    void acknowledgeAlert(int index);
    void acknowledgeAll();
    void clearAlert(int index);
    void clearAll();
    
    // Configurazione
    void setThresholds(AlertThresholds cfg);
    AlertThresholds getThresholds() { return thresholds; }
    void setEnabled(bool enabled);
    
    // Ottieni messaggio per display
    String getDisplayMessage();
    
    // Ottieni icona alert per display
    AlertPriority getHighestPriority();
};

// Istanza globale
extern WeatherAlerts weatherAlerts;

#endif // WEATHER_ALERTS_H
