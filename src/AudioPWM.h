#ifndef AUDIO_PWM_H
#define AUDIO_PWM_H

#ifndef BUFFER_TYPE
    #define BUFFER_TYPE int8_t
#endif

#include <Arduino.h>

class AudioPWM {
private:
    const BUFFER_TYPE *_buffer = nullptr;
    uint32_t buffer_size = 0;
    volatile uint32_t current_index = 0;
    uint8_t pwm_channel;
    hw_timer_t* sample_timer;

    static AudioPWM* instance;

    void IRAM_ATTR handleTick();
    static void IRAM_ATTR onTimerTick();

public:
    AudioPWM();
    void begin(uint8_t pin, int sample_rate = 16000, uint8_t channel = 0, uint32_t pwm_freq = 62500);
    uint32_t available();
    void set_buffer(const BUFFER_TYPE* buffer, uint32_t size);
};

#endif