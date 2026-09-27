#include "BatteryManager.h"
#include <Preferences.h>

// Global instance
BatteryManager battery;

// A learned "full" value below this is not trusted (e.g. a charge that ended
// early because of a fault): the plain curve is used instead
static const float MIN_FULL_PERCENT = 85.0f;

void BatteryManager::begin() {
    Serial.println("[BATTERY] Starting...");
    Preferences prefs;
    if (prefs.begin("battery", true)) {
        float stored = prefs.getFloat("full_pct", 100.0f);
        if (stored >= MIN_FULL_PERCENT && stored <= 100.0f) fullPercent = stored;
        prefs.end();
    }
    ina219Available = initINA219();
    if (!ina219Available) {
        Serial.println("[BATTERY] INA219 not found: battery not monitored");
        state = BATTERY_UNKNOWN;
        return;
    }
    measure();  // First reading at once, without waiting for the interval
    lastRead = millis();
    Serial.printf("[BATTERY] INA219 active: V=%.2fV I=%.1fmA %d%%\n", voltage, current_mA, percentage);
}

bool BatteryManager::initINA219() {
    Wire.begin(INA219_SDA_PIN, INA219_SCL_PIN);
    ina219 = Adafruit_INA219(INA219_I2C_ADDR);
    if (!ina219.begin(&Wire)) {
        Serial.println("[BATTERY] INA219 not responding");
        return false;
    }
    ina219.setCalibration_32V_1A();

    // Check: plausible voltage
    float v1 = ina219.getBusVoltage_V();
    float i1 = ina219.getCurrent_mA();
    delay(50);
    float v2 = ina219.getBusVoltage_V();
    float i2 = ina219.getCurrent_mA();
    if (v1 < 0.1f || v1 > 10.0f) {
        Serial.printf("[BATTERY] INA219: implausible voltage %.2f V\n", v1);
        return false;
    }
    // No check for "frozen" values: a charged battery with the charger
    // idle has a steady voltage and no current, and was mistaken for a
    // disconnected sensor (2.1.12). A missing INA219 already fails begin().
    (void)v2; (void)i1; (void)i2;
    return true;
}

void BatteryManager::readINA219() {
    float busVoltage = ina219.getBusVoltage_V();
    shuntVoltage_mV = ina219.getShuntVoltage_mV();
    float rawCurrent = ina219.getCurrent_mA();
    power_mW = ina219.getPower_mW();

#if INA219_REVERSED
    // VIN- (where the bus voltage is measured) is on the battery side
    voltage = busVoltage;
    // Reads positive while charging: converted to "positive = discharging"
    current_mA = -rawCurrent;
#else
    // Standard mounting: VIN+ on the battery side = bus + drop across the shunt
    voltage = busVoltage + (shuntVoltage_mV / 1000.0f);
    current_mA = rawCurrent;
#endif
}

bool BatteryManager::pollCharging() {
    if (!ina219Available) return false;
    bool before = isCharging;
    float rawCurrent = ina219.getCurrent_mA();
#if INA219_REVERSED
    current_mA = -rawCurrent;
#else
    current_mA = rawCurrent;
#endif
    detectChargingState();
    if (before == isCharging) return false;
    Serial.printf("[BATTERY] Charger %s (%.0f mA)\n", isCharging ? "plugged in" : "unplugged", current_mA);
    return true;
}

void BatteryManager::update() {
    unsigned long now = millis();
    if (!ina219Available) {
        // Sensor not found at boot: new attempt every minute
        if (now - lastRead < 60000UL) return;
        lastRead = now;
        ina219Available = initINA219();
        if (!ina219Available) return;
        measure();
        Serial.printf("[BATTERY] INA219 found: V=%.2fV I=%.1fmA %d%%\n", voltage, current_mA, percentage);
        return;
    }
    if (now - lastRead < READ_INTERVAL) return;
    lastRead = now;
    measure();
    Serial.printf("[BATTERY] V=%.2fV I=%.1fmA %d%%%s\n", voltage, current_mA, percentage,
                  isCharging ? " (in carica)" : "");
}

void BatteryManager::measure() {
    readINA219();

    // Estimated open-circuit voltage: while current flows the terminal voltage
    // is higher (charging) or lower (discharging) because of the internal resistance
    float vRest = voltage + (current_mA / 1000.0f) * BATTERY_INTERNAL_RESISTANCE;

    // Exponential filter: the percentage does not jump with the WiFi current peaks
    restVoltage = (restVoltage <= 0.0f) ? vRest : 0.7f * restVoltage + 0.3f * vRest;
    float raw = voltageToPercentage(restVoltage);
    percentage = min(100, (int)(raw * 100.0f / fullPercent + 0.5f));

    detectChargingState();

    if (isCharging) {
        // End of charge: the charger brings the cell to 4.2 V and then stops the
        // current; at rest the voltage settles around 4.10-4.15 V (4.14 V measured
        // after a full charge), so the threshold is 4.10 V with almost no current
        bool chargeDone = voltage >= 4.10f && fabsf(current_mA) < BATTERY_CURRENT_THRESHOLD_MA;
        if (chargeDone) learnFullPercent(raw);
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

// While the charge is finished the resting voltage settles for a while:
// the value is followed, and written to NVS only when it changes by 1 point
void BatteryManager::learnFullPercent(float raw) {
    if (raw < MIN_FULL_PERCENT || fabsf(raw - fullPercent) < 1.0f) return;
    fullPercent = raw;
    Preferences prefs;
    if (prefs.begin("battery", false)) {
        prefs.putFloat("full_pct", fullPercent);
        prefs.end();
    }
    Serial.printf("[BATTERY] Full battery = %.0f%% on the voltage curve: now shown as 100%%\n", fullPercent);
}

void BatteryManager::detectChargingState() {
    // current_mA is positive when discharging and negative when charging (see readINA219)
    // "Charging" here means powered by the charger. On battery the awake board
    // always draws more than 20 mA: an almost zero (or negative) current
    // means the charger is plugged in, even when the charge is finished.
    if (current_mA < BATTERY_EXTERNAL_POWER_MAX_MA) {
        isCharging = true;
    } else if (current_mA > BATTERY_CURRENT_THRESHOLD_MA) {
        isCharging = false;
    }
    // Between the two thresholds the previous state is kept
}

int BatteryManager::getEstimatedTimeRemaining() {
    if (!ina219Available || isCharging || current_mA <= 5.0f) return -1;
    float remainingMah = (percentage / 100.0f) * BATTERY_CAPACITY_MAH;
    return (int)(remainingMah / current_mA * 60.0f);
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
    // Realistic LiPo curve from a 0.5C discharge curve at 25°C
    // Points: voltage, percentage (linear interpolation)
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

    // Linear interpolation between the points
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
