#include "AudioPWM.h"

AudioPWM* AudioPWM::instance = nullptr;

AudioPWM::AudioPWM() : read_idx(0), write_idx(0), pwm_channel(0), sample_timer(nullptr) {
    instance = this;
}

void IRAM_ATTR AudioPWM::handleTick() {
    if (read_idx != write_idx) {
        int16_t sample = ring_buffer[read_idx];
        read_idx = (read_idx + 1) % BUFFER_SIZE;
        
        uint32_t duty = (sample + 32768) >> 8;
        ledcWrite(pwm_channel, duty);
    } else {
        ledcWrite(pwm_channel, 128); // Silence midpoint
    }
}

void IRAM_ATTR AudioPWM::onTimerTick() {
    if (instance) {
        instance->handleTick();
    }
}

void AudioPWM::begin(uint8_t pin, int sample_rate, uint8_t channel, uint32_t pwm_freq) {
    pwm_channel = channel;
    
    ledcSetup(pwm_channel, pwm_freq, 8);
    ledcAttachPin(pin, pwm_channel);
    ledcWrite(pwm_channel, 128);

    sample_timer = timerBegin(0, 8, true);
    uint32_t ticks = (10000000UL + sample_rate / 2) / sample_rate;

    timerAttachInterrupt(sample_timer, &AudioPWM::onTimerTick, true);
    timerAlarmWrite(sample_timer, ticks, true);
    timerAlarmEnable(sample_timer);
}

uint32_t AudioPWM::available() {
    if(write_idx >= read_idx){return write_idx - read_idx;}
    return BUFFER_SIZE - read_idx + write_idx;
}

void AudioPWM::fill_buffer(const int16_t* buffer, int len) {
    for (int i = 0; i < len; i++) {
        uint32_t next_write = (write_idx + 1) % BUFFER_SIZE;
        if (next_write != read_idx) {
            ring_buffer[write_idx] = buffer[i];
            write_idx = next_write;
        } else {
            break;
        }
    }
}

void AudioPWM::fill_buffer(const int8_t* buffer, int len) {
    for (int i = 0; i < len; i++) {
        uint32_t next_write = (write_idx + 1) % BUFFER_SIZE;
        if (next_write != read_idx) {
            ring_buffer[write_idx] = (int16_t)buffer[i] << 8;
            write_idx = next_write;
        } else {
            break;
        }
    }
}