#ifndef HISTORY_MANAGER_H
#define HISTORY_MANAGER_H

#include <Arduino.h>
#include <SD.h>
#include "WeatherUtils.h"

// Statistiche giornaliere compresse
struct DailyStats {
    char date[11];          // "YYYY-MM-DD"
    int8_t tempMin;         // Temperatura * 2 (range -60 a +60°C)
    int8_t tempMax;         // Temperatura * 2
    uint8_t humidityAvg;    // 0-255 (media umidità)
    uint16_t pressureAvg;   // Pressione (hPa)
    uint8_t weatherIdMain;  // ID meteo prevalente
    uint8_t samplesCount;   // Numero di campioni
    uint16_t checksum;      // CRC per validazione
};

// Statistiche orarie ultra-compresse (per storia dettagliata)
struct HourlyData {
    uint8_t hour;           // 0-23
    int8_t temp;            // Temperatura * 2
    uint8_t humidity;       // 0-100%
    uint8_t weatherId;      // ID meteo principale
};

// Summary settimanale/mensile
struct PeriodSummary {
    float tempMinPeriod;
    float tempMaxPeriod;
    float tempAvgPeriod;
    float humidityAvgPeriod;
    int mostCommonWeatherId;
    int rainyDays;
    int sunnyDays;
};

class HistoryManager {
private:
    const char* DAILY_STATS_FILE = "/daily_stats.dat";
    const char* HOURLY_DATA_FILE = "/hourly_data.dat";
    
    DailyStats todayStats;
    bool todayStatsLoaded;
    
    unsigned long lastSave;
    const unsigned long SAVE_INTERVAL = 300000; // Salva ogni 5 minuti
    
    // Buffer per dati orari (24 ore)
    HourlyData hourlyBuffer[24];
    int currentHourIndex;
    
    // Metodi privati
    void initTodayStats();
    void loadTodayStats();
    void saveTodayStats();
    void compressTempToInt8(float temp, int8_t& compressed);
    float decompressTempFromInt8(int8_t compressed);
    uint16_t calculateChecksum(DailyStats& stats);
    bool validateStats(DailyStats& stats);
    void rotateDailyStats();
    
public:
    HistoryManager();
    
    // Inizializza sistema
    void begin();
    
    // Aggiorna con dati attuali
    void update(WeatherData& weather);
    
    // Ottieni statistiche
    DailyStats getTodayStats();
    DailyStats getStatsForDate(const char* date);
    bool getStatsForDaysAgo(int daysAgo, DailyStats& stats);
    
    // Ottieni dati orari
    HourlyData* getTodayHourlyData();
    
    // Calcola summary periodi
    PeriodSummary getWeeklySummary();
    PeriodSummary getMonthlySummary();
    
    // Esporta dati
    String exportTodayJSON();
    String exportLastDaysJSON(int days);
    
    // Manutenzione
    void cleanOldData(int keepDays = 30);
    int getStoredDaysCount();
    size_t getDataFileSize();
    
    // Reset
    void clearAllHistory();
    void resetTodayStats();
};

// Istanza globale
extern HistoryManager history;

#endif // HISTORY_MANAGER_H
