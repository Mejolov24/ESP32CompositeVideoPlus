#include "AudioPWM.h"

AudioPWM* AudioPWM::instance = nullptr;

AudioPWM::AudioPWM() : pwm_channel(0), sample_timer(nullptr) {
    instance = this;
}

void IRAM_ATTR AudioPWM::handleTick(){
	if (_buffer != nullptr && current_index < buffer_size)
	{
		int8_t sample = _buffer[current_index++];
		uint32_t duty = static_cast<uint8_t>(sample) ^ 0x80;
		ledcWrite(pwm_channel, duty);
	}
	else{ledcWrite(pwm_channel, 128);}
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
    return buffer_size - current_index;
}

void AudioPWM::set_buffer(const BUFFER_TYPE* buffer, uint32_t size) {
    _buffer = buffer;
    buffer_size = size;
    current_index = 0;
}