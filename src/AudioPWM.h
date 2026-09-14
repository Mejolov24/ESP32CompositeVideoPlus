#ifndef AUDIO_PWM_H
#define AUDIO_PWM_H

#include <Arduino.h>

class AudioPWM {
private:
    static const int BUFFER_SIZE = 2048;
    volatile int16_t ring_buffer[BUFFER_SIZE];
    volatile uint32_t read_idx;
    volatile uint32_t write_idx;
    
    uint8_t pwm_channel;
    hw_timer_t* sample_timer;

    static AudioPWM* instance;

    void IRAM_ATTR handleTick();
    static void IRAM_ATTR onTimerTick();

public:
    AudioPWM();
    void begin(uint8_t pin, int sample_rate = 16000, uint8_t channel = 0, uint32_t pwm_freq = 62500);
    uint32_t available();
    void fill_buffer(const int16_t* buffer, int len);
    void fill_buffer(const int8_t* buffer, int len);
};

#endif