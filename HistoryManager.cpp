#include "HistoryManager.h"
#include "Hardware.h"
#include <ArduinoJson.h>
#include <SD.h> // Aggiunto

// Istanza globale
HistoryManager history;

HistoryManager::HistoryManager() {
    todayStatsLoaded = false;
    lastSave = 0;
    currentHourIndex = 0;
    
    // Inizializza buffer orario
    for (int i = 0; i < 24; i++) {
        hourlyBuffer[i].hour = i;
        hourlyBuffer[i].temp = 0;
        hourlyBuffer[i].humidity = 0;
        hourlyBuffer[i].weatherId = 0;
    }
}

void HistoryManager::begin() {
    // Usa initSD centralizzato per evitare conflitti
    if (!initSD()) {
        Serial.println("[HISTORY] SD card non disponibile");
        return;
    }
    
    // Carica o crea stats di oggi
    loadTodayStats();
    
    Serial.println("[HISTORY] Sistema storico inizializzato");
}

void HistoryManager::initTodayStats() {
    // Ottieni data corrente
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    
    strftime(todayStats.date, sizeof(todayStats.date), "%Y-%m-%d", &timeinfo);
    
    // Inizializza valori
    todayStats.tempMin = 127;   // Max int8
    todayStats.tempMax = -128;  // Min int8
    todayStats.humidityAvg = 0;
    todayStats.pressureAvg = 0;
    todayStats.weatherIdMain = 0;
    todayStats.samplesCount = 0;
    todayStats.checksum = 0;
    
    todayStatsLoaded = true;
}

void HistoryManager::loadTodayStats() {
    // Ottieni data corrente
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    
    char currentDate[11];
    strftime(currentDate, sizeof(currentDate), "%Y-%m-%d", &timeinfo);
    
    // Prova a caricare stats per oggi
    if (!SD.exists(DAILY_STATS_FILE)) {
        initTodayStats();
        return;
    }
    
    File file = SD.open(DAILY_STATS_FILE, FILE_READ);
    if (!file) {
        initTodayStats();
        return;
    }
    
    // Cerca record per oggi (leggi dall'ultimo verso l'inizio)
    bool found = false;
    file.seek(0, SeekEnd);
    long fileSize = file.position();
    int recordCount = fileSize / sizeof(DailyStats);
    
    for (int i = recordCount - 1; i >= 0 && i >= recordCount - 7; i--) {
        file.seek(i * sizeof(DailyStats));
        file.read((uint8_t*)&todayStats, sizeof(DailyStats));
        
        if (strcmp(todayStats.date, currentDate) == 0 && validateStats(todayStats)) {
            found = true;
            break;
        }
    }
    
    file.close();
    
    if (!found) {
        initTodayStats();
    } else {
        todayStatsLoaded = true;
        Serial.println("[HISTORY] Stats di oggi caricati");
    }
}

void HistoryManager::update(WeatherData& weather) {
    if (!todayStatsLoaded) {
        return;
    }
    
    // Aggiorna min/max temperatura
    int8_t tempCompressed;
    compressTempToInt8(weather.temp, tempCompressed);
    
    if (tempCompressed < todayStats.tempMin) {
        todayStats.tempMin = tempCompressed;
    }
    if (tempCompressed > todayStats.tempMax) {
        todayStats.tempMax = tempCompressed;
    }
    
    // Aggiorna medie (media incrementale)
    uint32_t totalHumidity = (uint32_t)todayStats.humidityAvg * todayStats.samplesCount;
    totalHumidity += (uint8_t)weather.humidity;
    
    uint32_t totalPressure = (uint32_t)todayStats.pressureAvg * todayStats.samplesCount;
    totalPressure += (uint16_t)weather.pressure;
    
    todayStats.samplesCount++;
    
    todayStats.humidityAvg = totalHumidity / todayStats.samplesCount;
    todayStats.pressureAvg = totalPressure / todayStats.samplesCount;
    todayStats.weatherIdMain = weather.weather_id; // Ultimo valore (o implementa moda)
    
    // Aggiorna dati orari
    struct tm timeinfo;
    time_t now = time(nullptr);
    localtime_r(&now, &timeinfo);
    
    int currentHour = timeinfo.tm_hour;
    hourlyBuffer[currentHour].hour = currentHour;
    hourlyBuffer[currentHour].temp = tempCompressed;
    hourlyBuffer[currentHour].humidity = (uint8_t)weather.humidity;
    hourlyBuffer[currentHour].weatherId = weather.weather_id % 256;
    
    // Salva periodicamente
    if (millis() - lastSave > SAVE_INTERVAL) {
        saveTodayStats();
    }
}

void HistoryManager::saveTodayStats() {
    if (!todayStatsLoaded) {
        return;
    }
    
    // Calcola checksum
    todayStats.checksum = calculateChecksum(todayStats);
    
    // Apri file in append
    File file = SD.open(DAILY_STATS_FILE, FILE_APPEND);
    if (!file) {
        Serial.println("[HISTORY] Errore apertura file per salvataggio");
        return;
    }
    
    // Cerca se esiste già record per oggi
    file.seek(0);
    long fileSize = file.size();
    int recordCount = fileSize / sizeof(DailyStats);
    
    long writePos = -1;
    DailyStats tempStats;
    
    for (int i = 0; i < recordCount; i++) {
        file.seek(i * sizeof(DailyStats));
        file.read((uint8_t*)&tempStats, sizeof(DailyStats));
        
        if (strcmp(tempStats.date, todayStats.date) == 0) {
            writePos = i * sizeof(DailyStats);
            break;
        }
    }
    
    // Scrivi
    if (writePos >= 0) {
        // Sovrascrivi record esistente
        file.seek(writePos);
    } else {
        // Aggiungi nuovo record
        file.seek(0, SeekEnd);
    }
    
    file.write((uint8_t*)&todayStats, sizeof(DailyStats));
    file.close();
    
    lastSave = millis();
    Serial.println("[HISTORY] Stats salvati");
}

void HistoryManager::compressTempToInt8(float temp, int8_t& compressed) {
    // Moltiplica per 2 per avere risoluzione 0.5°C
    // Range: -64°C a +63.5°C
    int tempInt = (int)(temp * 2.0);
    if (tempInt > 127) tempInt = 127;
    if (tempInt < -128) tempInt = -128;
    compressed = (int8_t)tempInt;
}

float HistoryManager::decompressTempFromInt8(int8_t compressed) {
    return (float)compressed / 2.0;
}

uint16_t HistoryManager::calculateChecksum(DailyStats& stats) {
    // CRC16 semplificato
    uint16_t crc = 0xFFFF;
    uint8_t* data = (uint8_t*)&stats;
    int len = sizeof(DailyStats) - sizeof(uint16_t); // Escludi checksum stesso
    
    for (int i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    
    return crc;
}

bool HistoryManager::validateStats(DailyStats& stats) {
    uint16_t calcChecksum = calculateChecksum(stats);
    return calcChecksum == stats.checksum;
}

DailyStats HistoryManager::getTodayStats() {
    return todayStats;
}

DailyStats HistoryManager::getStatsForDate(const char* date) {
    DailyStats stats;
    stats.samplesCount = 0; // Indica non trovato
    
    if (!SD.exists(DAILY_STATS_FILE)) {
        return stats;
    }
    
    File file = SD.open(DAILY_STATS_FILE, FILE_READ);
    if (!file) {
        return stats;
    }
    
    // Cerca data
    while (file.available()) {
        file.read((uint8_t*)&stats, sizeof(DailyStats));
        if (strcmp(stats.date, date) == 0 && validateStats(stats)) {
            break;
        }
        stats.samplesCount = 0;
    }
    
    file.close();
    return stats;
}

bool HistoryManager::getStatsForDaysAgo(int daysAgo, DailyStats& stats) {
    // Calcola data X giorni fa
    time_t now = time(nullptr);
    time_t target = now - (daysAgo * 86400);
    struct tm timeinfo;
    localtime_r(&target, &timeinfo);
    
    char targetDate[11];
    strftime(targetDate, sizeof(targetDate), "%Y-%m-%d", &timeinfo);
    
    stats = getStatsForDate(targetDate);
    return stats.samplesCount > 0;
}

HourlyData* HistoryManager::getTodayHourlyData() {
    return hourlyBuffer;
}

PeriodSummary HistoryManager::getWeeklySummary() {
    PeriodSummary summary;
    summary.tempMinPeriod = 999.0;
    summary.tempMaxPeriod = -999.0;
    summary.tempAvgPeriod = 0.0;
    summary.humidityAvgPeriod = 0.0;
    summary.mostCommonWeatherId = 0;
    summary.rainyDays = 0;
    summary.sunnyDays = 0;
    
    int validDays = 0;
    float tempSum = 0.0;
    float humiditySum = 0.0;
    
    for (int i = 0; i < 7; i++) {
        DailyStats stats;
        if (getStatsForDaysAgo(i, stats)) {
            float tMin = decompressTempFromInt8(stats.tempMin);
            float tMax = decompressTempFromInt8(stats.tempMax);
            
            if (tMin < summary.tempMinPeriod) summary.tempMinPeriod = tMin;
            if (tMax > summary.tempMaxPeriod) summary.tempMaxPeriod = tMax;
            
            tempSum += (tMin + tMax) / 2.0;
            humiditySum += stats.humidityAvg;
            
            // Conta giorni di pioggia/sole
            if (stats.weatherIdMain >= 500 && stats.weatherIdMain < 600) {
                summary.rainyDays++;
            } else if (stats.weatherIdMain == 800) {
                summary.sunnyDays++;
            }
            
            validDays++;
        }
    }
    
    if (validDays > 0) {
        summary.tempAvgPeriod = tempSum / validDays;
        summary.humidityAvgPeriod = humiditySum / validDays;
    }
    
    return summary;
}

PeriodSummary HistoryManager::getMonthlySummary() {
    // Simile a weekly ma per 30 giorni
    PeriodSummary summary;
    summary.tempMinPeriod = 999.0;
    summary.tempMaxPeriod = -999.0;
    summary.tempAvgPeriod = 0.0;
    summary.humidityAvgPeriod = 0.0;
    summary.rainyDays = 0;
    summary.sunnyDays = 0;
    
    int validDays = 0;
    float tempSum = 0.0;
    float humiditySum = 0.0;
    
    for (int i = 0; i < 30; i++) {
        DailyStats stats;
        if (getStatsForDaysAgo(i, stats)) {
            float tMin = decompressTempFromInt8(stats.tempMin);
            float tMax = decompressTempFromInt8(stats.tempMax);
            
            if (tMin < summary.tempMinPeriod) summary.tempMinPeriod = tMin;
            if (tMax > summary.tempMaxPeriod) summary.tempMaxPeriod = tMax;
            
            tempSum += (tMin + tMax) / 2.0;
            humiditySum += stats.humidityAvg;
            
            if (stats.weatherIdMain >= 500 && stats.weatherIdMain < 600) {
                summary.rainyDays++;
            } else if (stats.weatherIdMain == 800) {
                summary.sunnyDays++;
            }
            
            validDays++;
        }
    }
    
    if (validDays > 0) {
        summary.tempAvgPeriod = tempSum / validDays;
        summary.humidityAvgPeriod = humiditySum / validDays;
    }
    
    return summary;
}

String HistoryManager::exportTodayJSON() {
    DynamicJsonDocument doc(1024);
    
    doc["date"] = todayStats.date;
    doc["temp_min"] = decompressTempFromInt8(todayStats.tempMin);
    doc["temp_max"] = decompressTempFromInt8(todayStats.tempMax);
    doc["humidity_avg"] = todayStats.humidityAvg;
    doc["pressure_avg"] = todayStats.pressureAvg;
    doc["weather_id"] = todayStats.weatherIdMain;
    doc["samples"] = todayStats.samplesCount;
    
    String output;
    serializeJson(doc, output);
    return output;
}

String HistoryManager::exportLastDaysJSON(int days) {
    DynamicJsonDocument doc(4096);
    JsonArray array = doc.createNestedArray("history");
    
    for (int i = 0; i < days; i++) {
        DailyStats stats;
        if (getStatsForDaysAgo(i, stats)) {
            JsonObject obj = array.createNestedObject();
            obj["date"] = stats.date;
            obj["temp_min"] = decompressTempFromInt8(stats.tempMin);
            obj["temp_max"] = decompressTempFromInt8(stats.tempMax);
            obj["humidity_avg"] = stats.humidityAvg;
        }
    }
    
    String output;
    serializeJson(doc, output);
    return output;
}

void HistoryManager::cleanOldData(int keepDays) {
    // TODO: Implementare rimozione dati più vecchi di keepDays
    Serial.println("[HISTORY] Pulizia dati vecchi non ancora implementata");
}

int HistoryManager::getStoredDaysCount() {
    if (!SD.exists(DAILY_STATS_FILE)) {
        return 0;
    }
    
    File file = SD.open(DAILY_STATS_FILE, FILE_READ);
    if (!file) {
        return 0;
    }
    
    long fileSize = file.size();
    file.close();
    
    return fileSize / sizeof(DailyStats);
}

size_t HistoryManager::getDataFileSize() {
    if (!SD.exists(DAILY_STATS_FILE)) {
        return 0;
    }
    
    File file = SD.open(DAILY_STATS_FILE, FILE_READ);
    if (!file) {
        return 0;
    }
    
    size_t size = file.size();
    file.close();
    
    return size;
}

void HistoryManager::clearAllHistory() {
    SD.remove(DAILY_STATS_FILE);
    SD.remove(HOURLY_DATA_FILE);
    initTodayStats();
    Serial.println("[HISTORY] Tutta la storia cancellata");
}

void HistoryManager::resetTodayStats() {
    initTodayStats();
    Serial.println("[HISTORY] Stats di oggi resettati");
}
