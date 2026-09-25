#include "BatteryManager.h"

// Istanza globale
BatteryManager battery;

BatteryManager::BatteryManager() {
    adcPin = BATTERY_ADC_PIN;
    adcAvailable = false;  // Assume non disponibile finché non testato
    ina219Available = false;
    voltageRatio = VOLTAGE_DIVIDER_RATIO;

    voltage = 0.0;
    restVoltage = 0.0;
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
    historyCount = 0;
    lastVoltage = 0.0;
    stuckCounter = 0;
    
    // Inizializza history
    for (int i = 0; i < 10; i++) {
        voltageHistory[i] = 0.0;
    }
}

void BatteryManager::begin(int pin, float ratio) {
    adcPin = pin;
    voltageRatio = ratio;
    
    Serial.println("[BATTERY] Inizializzazione BatteryManager...");
    
    // Prova prima INA219 (più preciso)
    ina219Available = initINA219();
    
    if (ina219Available) {
        Serial.println("[BATTERY] INA219 trovato e inizializzato");
    } else {
        Serial.println("[BATTERY] INA219 non trovato, provo fallback ADC...");
        
        // Configura pin ADC come fallback
        pinMode(adcPin, INPUT);
        adcAvailable = testADC();
        
        if (adcAvailable) {
            Serial.println("[BATTERY] ADC disponibile su pin " + String(adcPin));
        } else {
            Serial.println("[BATTERY] ATTENZIONE: Nessun sensore batteria disponibile");
            voltage = 0.0;
            percentage = 0;
            state = BATTERY_UNKNOWN;
            isCharging = false;
            return;
        }
    }
    
    // Prima lettura immediata (forza update saltando il controllo intervallo)
    lastRead = millis() - READ_INTERVAL - 1;
    update();
    Serial.printf("[BATTERY] Init completato: V=%.2fV %%=%d\n", voltage, percentage);
    if (ina219Available) {
        Serial.println("[BATTERY] >>> SENSORE ATTIVO: INA219 (I2C)");
    } else if (adcAvailable) {
        Serial.printf("[BATTERY] >>> SENSORE ATTIVO: ADC (pin %d, ratio %.2f)\n", adcPin, voltageRatio);
    } else {
        Serial.println("[BATTERY] >>> SENSORE ATTIVO: NESSUNO");
    }
}

bool BatteryManager::initINA219() {
    // Inizializza I2C sui pin corretti per Lolin32 Lite
    Wire.begin(INA219_SDA_PIN, INA219_SCL_PIN);
    
    // Inizializza INA219 con indirizzo configurato (0x41 con A0 saldato)
    ina219 = Adafruit_INA219(INA219_I2C_ADDR);
    
    if (!ina219.begin(&Wire)) {
        Serial.println("[BATTERY] INA219 begin() fallito");
        return false;
    }
    
    // Configura per batteria LiPo
    ina219.setCalibration_32V_1A();
    
    // Test lettura
    float testVoltage1 = ina219.getBusVoltage_V();
    float testCurrent1 = ina219.getCurrent_mA();
    delay(50);
    float testVoltage2 = ina219.getBusVoltage_V();
    float testCurrent2 = ina219.getCurrent_mA();
    Serial.printf("[BATTERY] Test INA219: V1=%.2fV I1=%.1fmA | V2=%.2fV I2=%.1fmA\n",
                  testVoltage1, testCurrent1, testVoltage2, testCurrent2);
    
    // Range valido per LiPo
    if (testVoltage1 < 0.1 || testVoltage1 > 10.0) {
        Serial.printf("[BATTERY] INA219 lettura anomala: %.2fV\n", testVoltage1);
        return false;
    }
    
    // Falso positivo: se tensione e corrente sono identiche al centesimo tra due letture,
    // probabilmente è un dispositivo I2C "fantasma" o non collegato correttamente
    bool voltageIdentical = (abs(testVoltage1 - testVoltage2) < 0.01f);
    bool currentZero = (abs(testCurrent1) < 0.1f && abs(testCurrent2) < 0.1f);
    if (voltageIdentical && currentZero) {
        Serial.println("[BATTERY] INA219 rilevato ma valori bloccati (falso positivo?) - uso ADC fallback");
        return false;
    }
    
    return true;
}

void BatteryManager::readINA219() {
    if (!ina219Available) return;
    
    // Leggi tutti i valori dall'INA219
    float busVoltage = ina219.getBusVoltage_V();
    shuntVoltage_mV = ina219.getShuntVoltage_mV();
    float rawCurrent = ina219.getCurrent_mA();
    power_mW = ina219.getPower_mW();

#if INA219_REVERSED
    // VIN- (dove si misura la tensione di bus) è lato batteria
    voltage = busVoltage;
    // Letta positiva in carica: riportata a "positiva = scarica"
    current_mA = -rawCurrent;
#else
    // Montaggio standard: VIN+ lato batteria = bus + caduta sullo shunt
    voltage = busVoltage + (shuntVoltage_mV / 1000.0f);
    current_mA = rawCurrent;
#endif

    Serial.printf("[BATTERY] RAW bus=%.3fV shunt=%.2fmV cur=%.1fmA -> V=%.3fV I=%.1fmA (positiva = scarica)\n",
                  busVoltage, shuntVoltage_mV, rawCurrent, voltage, current_mA);
}

void BatteryManager::setADCAvailable(bool available) {
    adcAvailable = available;
    Serial.print("[BATTERY] ADC ");
    Serial.println(available ? "ABILITATO" : "DISABILITATO");
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
    unsigned long now = millis();
    if (now - lastRead < READ_INTERVAL) {
        return;
    }
    lastRead = now;
    Serial.printf("[BATTERY] update() eseguito a %lums, ina219Avail=%d\n", now, ina219Available);
    
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
    
    // Stuck detection: se tensione è identica per troppi cicli, sensore probabilmente non collegato correttamente
    // TEMPORANEAMENTE DISABILITATO PER DEBUG CRASH AP
    /*
    if (abs(voltage - lastVoltage) < 0.001f) {
        stuckCounter++;
        if (stuckCounter >= 10) {
            Serial.printf("[BATTERY] WARNING: tensione bloccata a %.2fV per %d letture consecutive - verificare collegamento sensore!\n", voltage, stuckCounter);
            stuckCounter = 0; // reset per evitare flood log
        }
    } else {
        stuckCounter = 0;
    }
    lastVoltage = voltage;
    */
    
    // Aggiorna history
    updateHistory(voltage);

    // Tensione a vuoto stimata: mentre scorre corrente la tensione ai morsetti
    // è più alta (carica) o più bassa (scarica) per la resistenza interna.
    // Con l'INA219 si compensa con la corrente misurata (positiva = scarica).
    float vRest = voltage;
    if (ina219Available) {
        vRest += (current_mA / 1000.0f) * BATTERY_INTERNAL_RESISTANCE;
    }

    // Filtro esponenziale: la percentuale non salta con i picchi del WiFi
    if (restVoltage <= 0.0f) {
        restVoltage = vRest;
    } else {
        restVoltage = 0.7f * restVoltage + 0.3f * vRest;
    }

    // Calcola percentuale
    percentage = voltageToPercentage(restVoltage);

    // Rileva stato carica
    detectChargingState();

    // Determina stato batteria
    if (isCharging) {
        // Fine carica: il caricatore riduce la corrente quasi a zero a 4,2 V
        bool chargeDone = voltage >= 4.15f && fabsf(current_mA) < BATTERY_CURRENT_THRESHOLD_MA;
        if (percentage >= 95 || chargeDone) {
            state = BATTERY_FULL;
            if (chargeDone) percentage = 100;
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
    
    // Log valori batteria
    if (ina219Available) {
        Serial.printf("[BATTERY] V=%.2fV, I=%.1fmA, %%=%d\n", 
                      voltage, current_mA, percentage);
    } else {
        Serial.printf("[BATTERY] V=%.2fV, %%=%d\n", 
                      voltage, percentage);
    }
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
    historyCount++;
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
        // current_mA è positiva in scarica e negativa in carica (vedi readINA219)
        if (current_mA < -BATTERY_CURRENT_THRESHOLD_MA) {
            isCharging = true;
        } else if (current_mA > BATTERY_CURRENT_THRESHOLD_MA) {
            isCharging = false;
        }
        // Corrente vicina a zero: si mantiene lo stato precedente
        // (a fine carica la corrente scende quasi a zero ma il caricatore è collegato)
        return;
    }
    
    // Fallback: usa trend tensione (meno affidabile)
    // Se la history non è ancora popolata (indice < 10 letture), default a non in carica
    if (historyCount < 10) {
        isCharging = false;
        return;
    }
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
}

int BatteryManager::voltageToPercentage(float v) {
    // Curva LiPo realistica basata su discharge curve 0.5C a 25°C
    // Punti: tensione, percentuale (interpolazione lineare)
    static const float curve[][2] = {
        {4.20f, 100.0f},  // Full charge
        {4.15f,  97.0f},  // Top plateau
        {4.10f,  92.0f},  // 
        {4.05f,  87.0f},  // 
        {4.00f,  80.0f},  // Start of main plateau
        {3.95f,  72.0f},  // 
        {3.90f,  63.0f},  // 
        {3.85f,  55.0f},  // 
        {3.80f,  48.0f},  // 
        {3.75f,  42.0f},  // 
        {3.70f,  35.0f},  // 
        {3.65f,  27.0f},  // 
        {3.60f,  20.0f},  // 
        {3.55f,  15.0f},  // 
        {3.50f,  10.0f},  // 
        {3.45f,   6.0f},  // 
        {3.40f,   3.0f},  // 
        {3.35f,   1.0f},  // Near cutoff
        {3.30f,   0.0f},  // Minimum safe voltage
        {3.00f,   0.0f}   // Deep discharge (should not reach)
    };
    const int n = sizeof(curve) / sizeof(curve[0]);

    // Clamp voltage
    if (v >= 4.25f) return 100;
    if (v <= 3.30f) return 0;
    if (v >= 4.20f) return 100;

    // Interpolazione lineare tra i punti
    for (int i = 0; i < n - 1; i++) {
        if (v <= curve[i][0] && v >= curve[i+1][0]) {
            float vRange = curve[i][0] - curve[i+1][0];
            float pRange = curve[i][1] - curve[i+1][1];
            float frac = (curve[i][0] - v) / vRange;
            return (int)(curve[i][1] - frac * pRange + 0.5f);  // Round to nearest
        }
    }
    return 0;
}

int BatteryManager::getEstimatedTimeRemaining() {
    if (isCharging) {
        return -1;  // N/A durante carica
    }
    
    if (!ina219Available && !adcAvailable) {
        return -1;  // Nessun sensore
    }
    
    float currentDraw;

    // Se abbiamo INA219, usa la corrente di scarica reale (positiva = scarica)
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
