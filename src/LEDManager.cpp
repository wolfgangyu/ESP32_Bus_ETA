#include "LEDManager.h"

const unsigned char LEDManager::cosTab[128] = {
    0,7,14,22,29,36,44,51,59,66,73,80,88,95,102,109,115,122,129,135,142,148,154,160,166,172,177,182,187,192,196,201,
    205,208,212,215,218,221,224,226,228,231,233,234,236,237,238,239,240,241,241,242,242,243,243,243,243,243,243,243,
    242,242,241,241,240,239,238,237,236,234,233,231,228,226,224,221,218,215,212,208,205,201,196,192,187,182,177,172,
    166,160,154,148,142,135,129,122,115,109,102,95,88,80,73,66,59,51,44,36,29,22,14,7,0
};

LEDManager::LEDManager() {}
LEDManager::~LEDManager() {}

void LEDManager::begin() {
    // 紅燈
    ledcSetup(CH_RED, PWM_FREQ, PWM_RES);
    ledcAttachPin(PIN_RED, CH_RED);
    // 綠燈
    ledcSetup(CH_GREEN, PWM_FREQ, PWM_RES);
    ledcAttachPin(PIN_GREEN, CH_GREEN);
    // 藍燈
    ledcSetup(CH_BLUE, PWM_FREQ, PWM_RES);
    ledcAttachPin(PIN_BLUE, CH_BLUE);
    // 背光
    ledcSetup(CH_BL, PWM_FREQ, PWM_RES);
    ledcAttachPin(PIN_BL, CH_BL);
    // 預設全滅
    ledcWrite(CH_RED, 0);
    ledcWrite(CH_GREEN, 0);
    ledcWrite(CH_BLUE, 0);
    ledcWrite(CH_BL, 255);  // 背光全亮
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
        case LED_RED_ON:
            ledcWrite(CH_RED, 255);
            ledcWrite(CH_GREEN, 0);
            ledcWrite(CH_BLUE, 0);
            break;

        case LED_FAST: {  // 0.5s 週期方波
            static const unsigned long PERIOD = 500;
            phase = (phase + (unsigned int)(PERIOD / 256)) & 0xFF;
            unsigned int v = (phase < 128) ? 40 : 0;
            ledcWrite(CH_GREEN, v);
            ledcWrite(CH_RED, 0);
            ledcWrite(CH_BLUE, 0);
            break;
        }

        case LED_HEARTBEAT: {  // 1.5s 週期，亮 100ms
            static uint32_t hbPhase = 0;
            hbPhase = (hbPhase + 20) % 1500;
            ledcWrite(CH_GREEN, (hbPhase < 100) ? 40 : 0);
            ledcWrite(CH_RED, 0);
            ledcWrite(CH_BLUE, 0);
            break;
        }

        case LED_OFF:
        default:
            ledcWrite(CH_RED, 0);
            ledcWrite(CH_GREEN, 0);
            ledcWrite(CH_BLUE, 0);
            break;
    }
    // 背光由 LightSensor 另行控制 (CH_BL)，此處不動
}