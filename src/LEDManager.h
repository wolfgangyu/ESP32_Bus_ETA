#ifndef LED_MANAGER_H
#define LED_MANAGER_H

#include <Arduino.h>

enum LedState {
    LED_RED_ON,      // 紅燈常亮：無網路
    LED_FAST,        // 綠燈快閃：連線中 (0.5s 週期)
    LED_HEARTBEAT,   // 綠燈心跳：已連線 (1.5s 週期，亮 100ms)
    LED_OFF          // 全熄
};

class LEDManager {
private:
    static const uint8_t CH_RED   = 0;
    static const uint8_t CH_GREEN = 1;
    static const uint8_t CH_BLUE  = 2;
    static const uint8_t CH_BL    = 3;   // 背光專用

    static const uint8_t PIN_RED   = 4;
    static const uint8_t PIN_GREEN = 17;
    static const uint8_t PIN_BLUE  = 16;
    static const uint8_t PIN_BL    = 27;

    static const uint32_t PWM_FREQ = 5000;
    static const uint8_t PWM_RES   = 8;

    LedState currentState = LED_OFF;
    unsigned long lastTick = 0;
    unsigned int phase = 0;

    // 方波/呼吸波查表 (沿用 main.cpp 既有 cosTab)
    static const unsigned char cosTab[128];

public:
    LEDManager();
    ~LEDManager();

    void begin();
    void tick(unsigned long now);
    void setState(LedState state);
    LedState getState() const { return currentState; }
};

#endif