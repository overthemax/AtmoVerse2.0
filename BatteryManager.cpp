#include "BatteryManager.h"

// Istanza globale
BatteryManager battery;

BatteryManager::BatteryManager() {
    adcPin = BATTERY_ADC_PIN;
    adcAvailable = false;  // Assume non disponibile finché non testato
    ina219Available = false;
    voltageRatio = VOLTAGE_DIVIDER_RATIO;
    
    voltage = 0.0;
    percentage = 0;
    state = BATTERY_UNKNOWN;
    isCharging = false;
    
    // INA219 dati
    current_mA = 0.0;
    power_mW = 0.0;
    shuntVoltage_mV = 0.0;
    
    vref = 3.3;  // ESP32 usa 3.3V come riferimento
    adcMax = 4095;  // ESP32 ADC 12-bit
    
    lastRead = 0;
    historyIndex = 0;
    
    // Inizializza history
    for (int i = 0; i < 10; i++) {
        voltageHistory[i] = 0.0;
    }
}

void BatteryManager::begin(int pin, float ratio) {
    adcPin = pin;
    voltageRatio = ratio;
    
    // Serial.println("[BATTERY] Inizializzazione BatteryManager...");
    
    // Prova prima INA219 (più preciso)
    ina219Available = initINA219();
    
    if (ina219Available) {
        // Serial.println("[BATTERY] ✓ INA219 trovato e inizializzato!");
        // Serial.println("[BATTERY] Lettura tensione e corrente via I2C");
    } else {
        // Serial.println("[BATTERY] INA219 non trovato, provo fallback ADC...");
        
        // Configura pin ADC come fallback
        pinMode(adcPin, INPUT);
        adcAvailable = testADC();
        
        if (adcAvailable) {
            // Serial.println("[BATTERY] ✓ ADC disponibile su pin " + String(adcPin));
        } else {
            // Serial.println("[BATTERY] ⚠️ ATTENZIONE: Nessun sensore disponibile!");
            // Serial.println("[BATTERY] Collega INA219 o configura voltage divider su ADC.");
            voltage = 0.0;
            percentage = 0;
            state = BATTERY_UNKNOWN;
            isCharging = false;
            return;
        }
    }
    
    // Prima lettura
    update();
}

bool BatteryManager::initINA219() {
    // Inizializza I2C sui pin corretti per Lolin32 Lite
    Wire.begin(INA219_SDA_PIN, INA219_SCL_PIN);
    
    // Inizializza INA219 con indirizzo configurato (0x41 con A0 saldato)
    ina219 = Adafruit_INA219(INA219_I2C_ADDR);
    
    if (!ina219.begin(&Wire)) {
        // Serial.println("[BATTERY] INA219 begin() fallito");
        return false;
    }
    
    // Configura per batteria LiPo (range 16V, 400mA max tipico per ESP32)
    // Usa calibrazione 32V_1A per avere margine
    ina219.setCalibration_32V_1A();
    
    // Test lettura
    float testVoltage = ina219.getBusVoltage_V();
    if (testVoltage < 0.1 || testVoltage > 10.0) {
        // Serial.printf("[BATTERY] INA219 lettura anomala: %.2fV\n", testVoltage);
        return false;
    }
    
    // Serial.printf("[BATTERY] INA219 test OK, tensione: %.2fV\n", testVoltage);
    return true;
}

void BatteryManager::readINA219() {
    if (!ina219Available) return;
    
    // Leggi tutti i valori dall'INA219
    shuntVoltage_mV = ina219.getShuntVoltage_mV();
    voltage = ina219.getBusVoltage_V() + (shuntVoltage_mV / 1000.0); // Tensione totale batteria
    current_mA = ina219.getCurrent_mA();
    power_mW = ina219.getPower_mW();
    
    // Nota: con INA219 in serie sul positivo:
    // - Corrente POSITIVA = batteria si scarica (corrente verso ESP32)
    // - Corrente NEGATIVA = batteria in carica (corrente da USB verso batteria)
    // Questo dipende dal verso di collegamento VIN+/VIN-
}

void BatteryManager::setADCAvailable(bool available) {
    adcAvailable = available;
    // Serial.print("[BATTERY] ADC ");
    // Serial.println(available ? "ABILITATO" : "DISABILITATO");
}

bool BatteryManager::testADC() {
    // Leggi ADC più volte per verificare stabilità e consistenza
    const int numReadings = 10;
    int readings[numReadings];
    
    for (int i = 0; i < numReadings; i++) {
        readings[i] = analogRead(adcPin);
        delay(5);  // Delay ridotto per test più veloce ma efficace
    }
    
    // Calcola media
    long sum = 0;
    int minVal = adcMax, maxVal = 0;
    for (int i = 0; i < numReadings; i++) {
        sum += readings[i];
        if (readings[i] < minVal) minVal = readings[i];
        if (readings[i] > maxVal) maxVal = readings[i];
    }
    int avg = sum / numReadings;
    
    // Se tutte le letture sono identiche a 0 o adcMax per molte volte, 
    // potrebbe indicare pin non connesso o cortocircuito
    int identicalCount = 0;
    for (int i = 0; i < numReadings; i++) {
        if (readings[i] == readings[0]) identicalCount++;
    }
    
    // Se tutte le letture sono identiche E sono 0 o max, probabilmente non connesso
    if (identicalCount == numReadings && (readings[0] == 0 || readings[0] == adcMax)) {
        return false;
    }
    
    // Controlla range di variazione (max - min)
    // Se il pin è floating, varierà molto; se è stabile, varierà poco
    int range = maxVal - minVal;
    
    // Se variazione troppo alta (>10% del range ADC), probabilmente floating
    if (range > (adcMax / 10)) {
        return false;
    }
    
    // Se variazione è 0 per molte letture ma non ai limiti, potrebbe essere un segnale DC stabile (valido)
    // quindi consideriamo l'ADC disponibile
    
    return true;
}

void BatteryManager::update() {
    // Controlla intervallo
    if (millis() - lastRead < READ_INTERVAL) {
        return;
    }
    lastRead = millis();
    
    // Nessun sensore disponibile
    if (!ina219Available && !adcAvailable) {
        voltage = 0.0;
        percentage = 0;
        state = BATTERY_UNKNOWN;
        isCharging = false;
        return;
    }
    
    // Leggi tensione (e corrente se INA219)
    if (ina219Available) {
        readINA219();
    } else {
        voltage = readVoltageRaw();
    }
    
    // Aggiorna history
    updateHistory(voltage);
    
    // Calcola percentuale
    percentage = voltageToPercentage(voltage);
    
    // Rileva stato carica
    detectChargingState();
    
    // Determina stato batteria
    if (isCharging) {
        if (percentage >= 95) {
            state = BATTERY_FULL;
        } else {
            state = BATTERY_CHARGING;
        }
    } else {
        if (percentage < 10) {
            state = BATTERY_CRITICAL;
        } else if (percentage < 20) {
            state = BATTERY_LOW;
        } else {
            state = BATTERY_DISCHARGING;
        }
    }
    
    // Log
    // if (ina219Available) {
    //     Serial.printf("[BATTERY] V=%.2fV, I=%.1fmA, P=%.1fmW, %%=%d, Charging=%s\n", 
    //                   voltage, current_mA, power_mW, percentage, isCharging ? "YES" : "NO");
    // } else {
    //     Serial.printf("[BATTERY] V=%.2fV, %%=%d, State=%d, Charging=%s\n", 
    //                   voltage, percentage, state, isCharging ? "YES" : "NO");
    // }
}

float BatteryManager::readVoltageRaw() {
    // Fai media di più letture per stabilità
    const int samples = 10;
    int sum = 0;
    
    for (int i = 0; i < samples; i++) {
        sum += analogRead(adcPin);
        delay(1);
    }
    
    int avgReading = sum / samples;
    
    // Converti in tensione
    // Tensione misurata = (ADC / ADC_MAX) * VREF
    float measuredVoltage = (avgReading / (float)adcMax) * vref;
    
    // Applica voltage divider ratio
    float actualVoltage = measuredVoltage * voltageRatio;
    
    return actualVoltage;
}

void BatteryManager::updateHistory(float v) {
    voltageHistory[historyIndex] = v;
    historyIndex = (historyIndex + 1) % 10;
}

float BatteryManager::getVoltageTrend() {
    // Calcola se tensione sta salendo o scendendo
    // Confronta prima metà con seconda metà dell'array
    float firstHalf = 0.0;
    float secondHalf = 0.0;
    
    for (int i = 0; i < 5; i++) {
        firstHalf += voltageHistory[i];
        secondHalf += voltageHistory[i + 5];
    }
    
    firstHalf /= 5.0;
    secondHalf /= 5.0;
    
    // Ritorna differenza (positivo = salendo, negativo = scendendo)
    return secondHalf - firstHalf;
}

void BatteryManager::detectChargingState() {
    // Se abbiamo INA219, usiamo la corrente (molto più affidabile!)
    if (ina219Available) {
        // Corrente negativa = carica (corrente fluisce verso la batteria)
        // Soglia di 10mA per evitare rumore
        if (current_mA < -10.0) {
            isCharging = true;
        } else if (current_mA > 10.0) {
            isCharging = false;
        }
        // Se corrente tra -10 e +10 mA, mantieni stato precedente
        return;
    }
    
    // Fallback: usa trend tensione (meno affidabile)
    float trend = getVoltageTrend();
    
    // Se tensione sta salendo significativamente, probabilmente in carica
    if (trend > 0.05) {  // 50mV di aumento
        isCharging = true;
    }
    // Se tensione sta scendendo o stabile, in scarica
    else if (trend < -0.02) {  // 20mV di calo
        isCharging = false;
    }
    // Mantieni stato precedente se cambio minimo
    
    // Ulteriore controllo: se tensione molto alta, sicuramente in carica
    if (voltage > BATTERY_MAX_VOLTAGE - 0.1) {
        isCharging = true;
    }
}

int BatteryManager::voltageToPercentage(float v) {
    // Mappa tensione a percentuale usando curva LiPo
    if (v >= BATTERY_MAX_VOLTAGE) return 100;
    if (v <= BATTERY_MIN_VOLTAGE) return 0;
    
    // Conversione lineare (semplificata)
    // Per curva più accurata, usa lookup table
    float range = BATTERY_MAX_VOLTAGE - BATTERY_MIN_VOLTAGE;
    float normalized = (v - BATTERY_MIN_VOLTAGE) / range;
    
    return (int)(normalized * 100.0);
}

int BatteryManager::getEstimatedTimeRemaining() {
    if (isCharging) {
        return -1;  // N/A durante carica
    }
    
    if (!ina219Available && !adcAvailable) {
        return -1;  // Nessun sensore
    }
    
    const float BATTERY_CAPACITY_MAH = 2000.0;
    float currentDraw;
    
    // Se abbiamo INA219, usa corrente reale!
    if (ina219Available && current_mA > 5.0) {
        currentDraw = current_mA;
    } else {
        // Fallback: stima 50mA
        currentDraw = 50.0;
    }
    
    float remainingCapacity = (percentage / 100.0) * BATTERY_CAPACITY_MAH;
    float hoursRemaining = remainingCapacity / currentDraw;
    
    return (int)(hoursRemaining * 60);  // Ritorna minuti
}

String BatteryManager::getStatusString() {
    if (!ina219Available && !adcAvailable) {
        return "⚠️ SENSORE NON DISPONIBILE";
    }
    
    String status = String(percentage) + "%";
    
    // Aggiungi info corrente se INA219 presente
    if (ina219Available) {
        status += " (" + String(abs(current_mA), 0) + "mA)";
    }
    
    if (isCharging) {
        status += " ⚡";
    } else {
        int mins = getEstimatedTimeRemaining();
        if (mins > 0) {
            int hours = mins / 60;
            mins = mins % 60;
            status += " ~" + String(hours) + "h" + String(mins) + "m";
        }
    }
    
    return status;
}

void BatteryManager::calibrate(float actualVoltage) {
    // Calibra il voltage divider ratio
    float measured = readVoltageRaw();
    if (measured > 0) {
        voltageRatio = actualVoltage / (measured / voltageRatio);
        // Serial.printf("[BATTERY] Calibrato: nuovo ratio = %.2f\n", voltageRatio);
    }
}
