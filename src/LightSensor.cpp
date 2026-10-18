#include "LightSensor.h"
#include "config.h"

LightSensor::LightSensor() {}
LightSensor::~LightSensor() {}

void LightSensor::begin() {
    adcAttachPin(PIN);
    analogSetAttenuation(LDR_ADC_ATTEN);
    // 初始讀取填滿緩衝
    for (int i = 0; i < LDR_SMOOTH_N; i++) {
        samples[i] = readRaw();
        delay(2);
    }
    sampleCount = LDR_SMOOTH_N;
    // 設定初始背光
    int smoothed = smooth(samples[0]);
    lastDuty = mapToDuty(smoothed);
    ledcWrite(CH_BL, lastDuty);
    #ifdef DEBUG_SERIAL
    Serial.printf("[LDR] init raw=%d duty=%d\n", samples[0], lastDuty);
    #endif
}

int LightSensor::readRaw() {
    return analogRead(PIN);  // 0-4095 (12-bit)
}

int LightSensor::smooth(int raw) {
    samples[sampleIdx] = raw;
    sampleIdx = (sampleIdx + 1) % LDR_SMOOTH_N;
    if (sampleCount < LDR_SMOOTH_N) sampleCount++;

    long sum = 0;
    for (int i = 0; i < sampleCount; i++) sum += samples[i];
    return sum / sampleCount;
}

uint8_t LightSensor::mapToDuty(int smoothedRaw) {
    int constrained = constrain(smoothedRaw, LDR_RAW_MIN, LDR_RAW_MAX);
    int duty = map(constrained, LDR_RAW_MIN, LDR_RAW_MAX, LDR_DUTY_MIN, LDR_DUTY_MAX);
    // 死區：變化小於 LDR_DEADZONE 不更新
    if (abs(duty - lastDuty) < LDR_DEADZONE) return lastDuty;
    return duty;
}

void LightSensor::tick(unsigned long now) {
    if (now - lastUpdate < LDR_REFRESH_MS) return;
    lastUpdate = now;

    int raw = readRaw();
    int smoothed = smooth(raw);
    uint8_t duty = mapToDuty(smoothed);

    if (duty != lastDuty) {
        lastDuty = duty;
        ledcWrite(CH_BL, duty);
        #ifdef DEBUG_SERIAL
        Serial.printf("[LDR] raw=%d smooth=%d duty=%d\n", raw, smoothed, duty);
        #endif
    }
}

int LightSensor::getRaw() const {
    return samples[(sampleIdx - 1 + LDR_SMOOTH_N) % LDR_SMOOTH_N];
}

int LightSensor::getSmoothed() const {
    long sum = 0;
    for (int i = 0; i < sampleCount; i++) sum += samples[i];
    return sampleCount ? (sum / sampleCount) : 0;
}