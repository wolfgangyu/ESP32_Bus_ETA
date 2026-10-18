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
    // 共陽極 RGB LED：LOW=亮，HIGH=滅
    static const uint8_t PIN_RED   = 4;   // 紅
    static const uint8_t PIN_GREEN = 16;  // 綠
    static const uint8_t PIN_BLUE  = 17;  // 藍
    static const uint8_t PIN_BL    = 27;  // 背光（共陰極，正向）

    static const uint8_t CH_RED   = 0;
    static const uint8_t CH_GREEN = 1;
    static const uint8_t CH_BLUE  = 2;
    static const uint8_t CH_BL    = 3;

    static const uint32_t PWM_FREQ = 5000;
    static const uint8_t PWM_RES   = 8;

    // 共陽極：duty=0 最亮，duty=255 最暗
    static const uint8_t DUTY_ON  = 0;
    static const uint8_t DUTY_OFF = 255;

    LedState currentState = LED_OFF;
    unsigned long lastTick = 0;
    unsigned int phase = 0;

public:
    LEDManager();
    ~LEDManager();

    void begin();
    void tick(unsigned long now);
    void setState(LedState state);
    LedState getState() const { return currentState; }
};

#endif