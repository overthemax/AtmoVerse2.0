#include "BatteryManager.h"

// Istanza globale
BatteryManager battery;

void BatteryManager::begin() {
    Serial.println("[BATTERY] Inizializzazione...");
    ina219Available = initINA219();
    if (!ina219Available) {
        Serial.println("[BATTERY] INA219 non trovato: batteria non monitorata");
        state = BATTERY_UNKNOWN;
        return;
    }
    measure();  // Prima lettura subito, senza attendere l'intervallo
    lastRead = millis();
    Serial.printf("[BATTERY] INA219 attivo: V=%.2fV I=%.1fmA %d%%\n", voltage, current_mA, percentage);
}

bool BatteryManager::initINA219() {
    Wire.begin(INA219_SDA_PIN, INA219_SCL_PIN);
    ina219 = Adafruit_INA219(INA219_I2C_ADDR);
    if (!ina219.begin(&Wire)) {
        Serial.println("[BATTERY] INA219 non risponde");
        return false;
    }
    ina219.setCalibration_32V_1A();

    // Verifica: tensione plausibile e valori non bloccati
    float v1 = ina219.getBusVoltage_V();
    float i1 = ina219.getCurrent_mA();
    delay(50);
    float v2 = ina219.getBusVoltage_V();
    float i2 = ina219.getCurrent_mA();
    if (v1 < 0.1f || v1 > 10.0f) {
        Serial.printf("[BATTERY] INA219: tensione anomala %.2f V\n", v1);
        return false;
    }
    if (fabsf(v1 - v2) < 0.01f && fabsf(i1) < 0.1f && fabsf(i2) < 0.1f) {
        Serial.println("[BATTERY] INA219: valori bloccati, sensore non collegato correttamente");
        return false;
    }
    return true;
}

void BatteryManager::readINA219() {
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
}

void BatteryManager::update() {
    if (!ina219Available) return;
    unsigned long now = millis();
    if (now - lastRead < READ_INTERVAL) return;
    lastRead = now;
    measure();
    Serial.printf("[BATTERY] V=%.2fV I=%.1fmA %d%%%s\n", voltage, current_mA, percentage,
                  isCharging ? " (in carica)" : "");
}

void BatteryManager::measure() {
    readINA219();

    // Tensione a vuoto stimata: mentre scorre corrente la tensione ai morsetti
    // è più alta (carica) o più bassa (scarica) per la resistenza interna
    float vRest = voltage + (current_mA / 1000.0f) * BATTERY_INTERNAL_RESISTANCE;

    // Filtro esponenziale: la percentuale non salta con i picchi del WiFi
    restVoltage = (restVoltage <= 0.0f) ? vRest : 0.7f * restVoltage + 0.3f * vRest;
    percentage = voltageToPercentage(restVoltage);

    detectChargingState();

    if (isCharging) {
        // Fine carica: il caricatore riduce la corrente quasi a zero a 4,2 V
        bool chargeDone = voltage >= 4.15f && fabsf(current_mA) < BATTERY_CURRENT_THRESHOLD_MA;
        if (percentage >= 95 || chargeDone) {
            state = BATTERY_FULL;
            if (chargeDone) percentage = 100;
        } else {
            state = BATTERY_CHARGING;
        }
    } else if (percentage < 10) {
        state = BATTERY_CRITICAL;
    } else if (percentage < 20) {
        state = BATTERY_LOW;
    } else {
        state = BATTERY_DISCHARGING;
    }

    updateLevel();
}

void BatteryManager::detectChargingState() {
    // current_mA è positiva in scarica e negativa in carica (vedi readINA219)
    if (current_mA < -BATTERY_CURRENT_THRESHOLD_MA) {
        isCharging = true;
    } else if (current_mA > BATTERY_CURRENT_THRESHOLD_MA) {
        isCharging = false;
    }
    // Corrente vicina a zero: si mantiene lo stato precedente
    // (a fine carica la corrente scende quasi a zero ma il caricatore è collegato)
}

int BatteryManager::getEstimatedTimeRemaining() {
    if (!ina219Available || isCharging || current_mA <= 5.0f) return -1;
    float remainingMah = (percentage / 100.0f) * BATTERY_CAPACITY_MAH;
    return (int)(remainingMah / current_mA * 60.0f);
}

String BatteryManager::getStatusString() {
    if (!ina219Available) return "Sensore batteria non disponibile";
    String status = String(percentage) + "% (" + String(fabsf(current_mA), 0) + " mA)";
    if (isCharging) {
        status += " in carica";
    } else {
        int mins = getEstimatedTimeRemaining();
        if (mins > 0) status += " ~" + String(mins / 60) + "h" + String(mins % 60) + "m";
    }
    return status;
}

void BatteryManager::updateLevel() {
    if (isCharging || !ina219Available) {
        level = BATTERY_LEVEL_OK;
        return;
    }
    int p = percentage;
    switch (level) {
        case BATTERY_LEVEL_CRITICAL:
            if (p >= BATTERY_CRITICAL_EXIT_PERCENT) {
                level = (p < BATTERY_LOW_EXIT_PERCENT) ? BATTERY_LEVEL_LOW : BATTERY_LEVEL_OK;
            }
            break;
        case BATTERY_LEVEL_LOW:
            if (p <= BATTERY_CRITICAL_PERCENT) level = BATTERY_LEVEL_CRITICAL;
            else if (p >= BATTERY_LOW_EXIT_PERCENT) level = BATTERY_LEVEL_OK;
            break;
        default:
            if (p <= BATTERY_CRITICAL_PERCENT) level = BATTERY_LEVEL_CRITICAL;
            else if (p <= BATTERY_LOW_PERCENT) level = BATTERY_LEVEL_LOW;
            break;
    }
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
