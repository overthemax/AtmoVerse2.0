#include "WeatherAlerts.h"
#include <HTTPClient.h>

// Istanza globale
WeatherAlerts weatherAlerts;

// Riferimento esterno ai dati meteo correnti
extern WeatherData currentWeather;

WeatherAlerts::WeatherAlerts() {
    // Configurazione predefinita
    thresholds.enabled = true;
    thresholds.tempHigh = 35.0;
    thresholds.tempLow = 0.0;
    thresholds.windHigh = 50.0;
    thresholds.humidityHigh = 90.0;
    thresholds.humidityLow = 20.0;
    thresholds.alertRain = true;
    thresholds.alertSnow = true;
    thresholds.alertStorm = true;
    thresholds.alertFog = false;
    thresholds.showOnDisplay = true;
    thresholds.sendWebhook = false;
    thresholds.webhookUrl = "";
    
    alertCount = 0;
    lastCheck = 0;
    
    // Inizializza array alert
    for (int i = 0; i < 5; i++) {
        alerts[i].active = false;
        alerts[i].acknowledged = false;
    }
}

void WeatherAlerts::begin(AlertThresholds cfg) {
    thresholds = cfg;
    Serial.println("[ALERTS] Sistema alert meteo inizializzato");
}

void WeatherAlerts::checkAlerts() {
    if (!thresholds.enabled) {
        return;
    }
    
    // Controlla solo ogni CHECK_INTERVAL
    if (millis() - lastCheck < CHECK_INTERVAL) {
        return;
    }
    lastCheck = millis();
    
    // Pulisci vecchi alert
    cleanOldAlerts();
    
    // Controlla varie condizioni
    checkTemperatureAlerts();
    checkWindAlerts();
    checkWeatherConditionAlerts();
}

void WeatherAlerts::checkTemperatureAlerts() {
    // Alert temperatura alta
    if (currentWeather.temp > thresholds.tempHigh) {
        String msg = "Temperatura elevata: " + String(currentWeather.temp, 1) + "°C";
        addAlert(ALERT_TEMP_HIGH, 
                currentWeather.temp > thresholds.tempHigh + 5 ? PRIORITY_CRITICAL : PRIORITY_WARNING,
                msg);
    }
    
    // Alert temperatura bassa
    if (currentWeather.temp < thresholds.tempLow) {
        String msg = "Temperatura bassa: " + String(currentWeather.temp, 1) + "°C";
        addAlert(ALERT_TEMP_LOW,
                currentWeather.temp < thresholds.tempLow - 5 ? PRIORITY_CRITICAL : PRIORITY_WARNING,
                msg);
    }
}

void WeatherAlerts::checkWindAlerts() {
    if (currentWeather.wind_speed > thresholds.windHigh) {
        String msg = "Vento forte: " + String(currentWeather.wind_speed, 1) + " km/h";
        addAlert(ALERT_WIND_HIGH,
                currentWeather.wind_speed > thresholds.windHigh + 20 ? PRIORITY_CRITICAL : PRIORITY_WARNING,
                msg);
    }
}

void WeatherAlerts::checkWeatherConditionAlerts() {
    int weatherId = currentWeather.weather_id;
    
    // Eventi estremi (900-906) - Priorità massima
    if (weatherId >= 900 && weatherId < 910) {
        if (weatherId == 900 || weatherId == 781) {
            // Tornado
            addAlert(ALERT_TORNADO, PRIORITY_CRITICAL, "⚠️ ALLERTA TORNADO!");
        }
        else if (weatherId == 901 || weatherId == 902) {
            // Uragano/Tempesta tropicale
            addAlert(ALERT_HURRICANE, PRIORITY_CRITICAL, "⚠️ ALLERTA URAGANO!");
        }
        else if (weatherId == 903) {
            // Freddo estremo
            addAlert(ALERT_COLD_WAVE, PRIORITY_CRITICAL, "❄️ Freddo estremo!");
        }
        else if (weatherId == 904) {
            // Caldo estremo
            addAlert(ALERT_HEAT_WAVE, PRIORITY_CRITICAL, "🔥 Caldo estremo!");
        }
        else if (weatherId == 905 || weatherId == 906) {
            // Vento molto forte
            addAlert(ALERT_WIND_HIGH, PRIORITY_CRITICAL, "💨 Vento molto forte!");
        }
    }
    
    // Temporale (200-299)
    else if (weatherId >= 200 && weatherId < 300 && thresholds.alertStorm) {
        addAlert(ALERT_STORM, PRIORITY_CRITICAL, "⛈️ Allerta temporale in corso!");
    }
    
    // Neve (600-699)
    else if (weatherId >= 600 && weatherId < 700 && thresholds.alertSnow) {
        // Neve con temporale (620-622)
        if (weatherId >= 620 && weatherId <= 622) {
            addAlert(ALERT_SNOW, PRIORITY_CRITICAL, "❄️⛈️ Neve con temporale!");
        }
        // Neve intensa (602)
        else if (weatherId == 602) {
            addAlert(ALERT_SNOW, PRIORITY_CRITICAL, "❄️ Nevicate intense!");
        }
        // Nevischio con temporale (616)
        else if (weatherId == 616) {
            addAlert(ALERT_SNOW, PRIORITY_CRITICAL, "🌨️⛈️ Nevischio con temporale!");
        } 
        else {
            addAlert(ALERT_SNOW, PRIORITY_WARNING, "❄️ Nevicate previste");
        }
    }
    
    // Pioggia (500-599)
    else if (weatherId >= 500 && weatherId < 600 && thresholds.alertRain) {
        String intensity = weatherId < 510 ? "🌧️ Pioggia leggera" : "🌧️ Pioggia intensa";
        addAlert(ALERT_RAIN, 
                weatherId >= 502 ? PRIORITY_WARNING : PRIORITY_INFO,
                intensity);
    }
    
    // Atmosfera (700-799)
    else if (weatherId >= 700 && weatherId < 800) {
        // Tornado già gestito sopra (781)
        if (weatherId == 781) {
            // Già gestito
        }
        // Fumo (711)
        else if (weatherId == 711) {
            addAlert(ALERT_EXTREME_WEATHER, PRIORITY_WARNING, "💨 Allerta fumo!");
        }
        // Polvere/Sabbia (731, 751, 761)
        else if (weatherId == 731 || weatherId == 751 || weatherId == 761) {
            addAlert(ALERT_EXTREME_WEATHER, PRIORITY_WARNING, "🌪️ Tempesta di polvere/sabbia!");
        }
        // Cenere vulcanica (762)
        else if (weatherId == 762) {
            addAlert(ALERT_EXTREME_WEATHER, PRIORITY_CRITICAL, "🌋 Cenere vulcanica!");
        }
        // Nebbia (701, 721, 741)
        else if ((weatherId == 701 || weatherId == 721 || weatherId == 741) && thresholds.alertFog) {
            addAlert(ALERT_EXTREME_WEATHER, PRIORITY_INFO, "🌫️ Nebbia presente");
        }
    }
}

void WeatherAlerts::addAlert(AlertType type, AlertPriority priority, String message) {
    // Controlla se alert già presente
    for (int i = 0; i < alertCount; i++) {
        if (alerts[i].type == type && alerts[i].active) {
            // Aggiorna solo il timestamp
            alerts[i].timestamp = millis();
            return;
        }
    }
    
    // Aggiungi nuovo alert se c'è spazio
    if (alertCount < 5) {
        alerts[alertCount].type = type;
        alerts[alertCount].priority = priority;
        alerts[alertCount].message = message;
        alerts[alertCount].timestamp = millis();
        alerts[alertCount].acknowledged = false;
        alerts[alertCount].active = true;
        
        // Log
        Serial.print("[ALERTS] Nuovo alert: ");
        Serial.println(message);
        
        // Invia webhook se abilitato
        if (thresholds.sendWebhook && thresholds.webhookUrl.length() > 0) {
            sendWebhookNotification(alerts[alertCount]);
        }
        
        alertCount++;
    }
}

void WeatherAlerts::cleanOldAlerts() {
    // Rimuovi alert più vecchi di 1 ora
    const unsigned long MAX_AGE = 3600000; // 1 ora
    
    for (int i = 0; i < alertCount; i++) {
        if (millis() - alerts[i].timestamp > MAX_AGE) {
            alerts[i].active = false;
        }
    }
    
    // Compatta array rimuovendo alert inattivi
    int writeIndex = 0;
    for (int readIndex = 0; readIndex < alertCount; readIndex++) {
        if (alerts[readIndex].active) {
            if (writeIndex != readIndex) {
                alerts[writeIndex] = alerts[readIndex];
            }
            writeIndex++;
        }
    }
    alertCount = writeIndex;
}

void WeatherAlerts::sendWebhookNotification(WeatherAlert& alert) {
    HTTPClient http;
    http.begin(thresholds.webhookUrl);
    http.addHeader("Content-Type", "application/json");
    
    // Crea payload JSON
    String payload = "{";
    payload += "\"type\":\"weather_alert\",";
    payload += "\"priority\":" + String(alert.priority) + ",";
    payload += "\"message\":\"" + alert.message + "\",";
    payload += "\"timestamp\":" + String(alert.timestamp);
    payload += "}";
    
    int httpCode = http.POST(payload);
    
    if (httpCode > 0) {
        Serial.printf("[ALERTS] Webhook inviato, code: %d\n", httpCode);
    } else {
        Serial.printf("[ALERTS] Webhook fallito: %s\n", http.errorToString(httpCode).c_str());
    }
    
    http.end();
}

int WeatherAlerts::getActiveAlertsCount() {
    return alertCount;
}

WeatherAlert* WeatherAlerts::getActiveAlerts() {
    return alerts;
}

bool WeatherAlerts::hasCriticalAlerts() {
    for (int i = 0; i < alertCount; i++) {
        if (alerts[i].active && alerts[i].priority == PRIORITY_CRITICAL) {
            return true;
        }
    }
    return false;
}

void WeatherAlerts::acknowledgeAlert(int index) {
    if (index >= 0 && index < alertCount) {
        alerts[index].acknowledged = true;
        Serial.println("[ALERTS] Alert " + String(index) + " riconosciuto");
    }
}

void WeatherAlerts::acknowledgeAll() {
    for (int i = 0; i < alertCount; i++) {
        alerts[i].acknowledged = true;
    }
    Serial.println("[ALERTS] Tutti gli alert riconosciuti");
}

void WeatherAlerts::clearAlert(int index) {
    if (index >= 0 && index < alertCount) {
        alerts[index].active = false;
        cleanOldAlerts(); // Ricompatta
    }
}

void WeatherAlerts::clearAll() {
    for (int i = 0; i < alertCount; i++) {
        alerts[i].active = false;
    }
    alertCount = 0;
    Serial.println("[ALERTS] Tutti gli alert cancellati");
}

void WeatherAlerts::setThresholds(AlertThresholds cfg) {
    thresholds = cfg;
}

void WeatherAlerts::setEnabled(bool enabled) {
    thresholds.enabled = enabled;
    Serial.print("[ALERTS] Sistema alert ");
    Serial.println(enabled ? "ABILITATO" : "DISABILITATO");
}

String WeatherAlerts::getDisplayMessage() {
    if (alertCount == 0) {
        return "";
    }
    
    // Trova alert con priorità più alta
    int highestPriorityIndex = 0;
    for (int i = 1; i < alertCount; i++) {
        if (alerts[i].priority > alerts[highestPriorityIndex].priority) {
            highestPriorityIndex = i;
        }
    }
    
    return alerts[highestPriorityIndex].message;
}

AlertPriority WeatherAlerts::getHighestPriority() {
    AlertPriority highest = PRIORITY_INFO;
    
    for (int i = 0; i < alertCount; i++) {
        if (alerts[i].active && alerts[i].priority > highest) {
            highest = alerts[i].priority;
        }
    }
    
    return highest;
}
