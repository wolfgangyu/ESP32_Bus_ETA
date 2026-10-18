#ifndef LIGHT_SENSOR_H
#define LIGHT_SENSOR_H

#include <Arduino.h>
#include "config.h"

class LightSensor {
private:
    static const uint8_t PIN = LDR_PIN;
    static const uint8_t CH_BL = LED_CH_BL;  // 背光 PWM channel

    int samples[LDR_SMOOTH_N] = {0};
    int sampleIdx = 0;
    int sampleCount = 0;
    int lastDuty = 128;
    unsigned long lastUpdate = 0;

    int readRaw();
    int smooth(int raw);
    uint8_t mapToDuty(int smoothedRaw);

public:
    LightSensor();
    ~LightSensor();

    void begin();
    void tick(unsigned long now);
    int getRaw() const;
    int getSmoothed() const;
    uint8_t getDuty() const { return lastDuty; }
};

#endif