#include "LEDManager.h"

LEDManager::LEDManager() {}
LEDManager::~LEDManager() {}

// 共陽極：LOW=亮，HIGH=滅；PWM duty 越小越亮（反向）
static const uint8_t PIN_RED   = 4;   // 紅
static const uint8_t PIN_GREEN = 16;  // 綠
static const uint8_t PIN_BLUE  = 17;  // 藍
static const uint8_t PIN_BL    = 27;  // 背光（共陰極，另行處理）

static const uint8_t CH_RED   = 0;
static const uint8_t CH_GREEN = 1;
static const uint8_t CH_BLUE  = 2;
static const uint8_t CH_BL    = 3;

static const uint32_t PWM_FREQ = 5000;
static const uint8_t PWM_RES   = 8;

// 共陽極：duty=0 最亮，duty=255 最暗
static const uint8_t DUTY_ON  = 0;    // 全亮
static const uint8_t DUTY_OFF = 255;  // 全滅
static const uint8_t DUTY_DIM = 220;  // 微亮（約 14%）

void LEDManager::begin() {
    // 紅燈
    ledcAttachChannel(PIN_RED, PWM_FREQ, PWM_RES, CH_RED);
    // 綠燈
    ledcAttachChannel(PIN_GREEN, PWM_FREQ, PWM_RES, CH_GREEN);
    // 藍燈
    ledcAttachChannel(PIN_BLUE, PWM_FREQ, PWM_RES, CH_BLUE);
    // 背光（共陰極，正向）
    ledcAttachChannel(PIN_BL, PWM_FREQ, PWM_RES, CH_BL);

    // 預設全滅（共陽極寫 DUTY_OFF=255）
    ledcWriteChannel(CH_RED, DUTY_OFF);
    ledcWriteChannel(CH_GREEN, DUTY_OFF);
    ledcWriteChannel(CH_BLUE, DUTY_OFF);
    // 背光全亮（共陰極寫 255）
    ledcWriteChannel(CH_BL, 255);
}

void LEDManager::setState(LedState state) {
    if (state != currentState) {
        currentState = state;
        phase = 0;
        lastTick = 0;
    }
}

void LEDManager::tick(unsigned long now) {
    if (now - lastTick < 20) return;
    lastTick = now;

    switch (currentState) {
        case LED_RED_ON:  // 無網路：紅燈常亮
            ledcWriteChannel(CH_RED, DUTY_ON);
            ledcWriteChannel(CH_GREEN, DUTY_OFF);
            ledcWriteChannel(CH_BLUE, DUTY_OFF);
            break;

        case LED_FAST: {  // 連線中：綠燈快閃 (0.5s 週期)
            static const unsigned long PERIOD = 500;
            phase = (phase + (unsigned int)(PERIOD / 256)) & 0xFF;
            uint8_t v = (phase < 128) ? DUTY_ON : DUTY_OFF;
            ledcWriteChannel(CH_GREEN, v);
            ledcWriteChannel(CH_RED, DUTY_OFF);
            ledcWriteChannel(CH_BLUE, DUTY_OFF);
            break;
        }

        case LED_HEARTBEAT: {  // 已連線：綠燈心跳 (1.5s 週期，亮 100ms)
            static uint32_t hbPhase = 0;
            hbPhase = (hbPhase + 20) % 1500;
            ledcWriteChannel(CH_GREEN, (hbPhase < 100) ? DUTY_ON : DUTY_OFF);
            ledcWriteChannel(CH_RED, DUTY_OFF);
            ledcWriteChannel(CH_BLUE, DUTY_OFF);
            break;
        }

        case LED_OFF:
        default:
            ledcWriteChannel(CH_RED, DUTY_OFF);
            ledcWriteChannel(CH_GREEN, DUTY_OFF);
            ledcWriteChannel(CH_BLUE, DUTY_OFF);
            break;
    }
    // 背光由 LightSensor 另行控制 (CH_BL)，此處不動
}