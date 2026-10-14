#include <Arduino.h>
#include <CompositeVideo.h>
#include <CompositeGraphics.h>
#include <CompositeColor.h>
#include <AudioPWM.h>
#include "esp_pm.h"

#include "font6x8.h"
Font<CompositeGraphics> font(6, 8, font6x8::pixels);
AudioPWM audio;

CompositeVideoBuffer scanout_buffer; // where we push
CompositeVideoBuffer frame_buffer; // where we render

VideoResolution_t resolution = VideoResolution_t::VIDEO_RES_NTSC_336x240P;
SubSampling_t subsampling = SubSampling_t::SUBSAMPLING_422; // recommended, saves memory and looks almost identical

CompositeGraphics graphics;

void test_rainbow()
{
    uint16_t xres = graphics.xres;
    uint16_t yres = graphics.yres;

    for (uint16_t x = 0; x < xres; x++) {
        float angle = ((float)x / (float)xres) * 2.0f * M_PI;
        
        float base_u = cosf(angle);
        float base_v = sinf(angle);

        for (uint16_t y = 0; y < yres; y++) {
            float brightness_factor = 1.0f - ((float)y / (float)yres);
            brightness_factor = brightness_factor * brightness_factor;
            uint8_t luma = (uint8_t)(brightness_factor * 255.0f);
            float saturation = 127.0f * brightness_factor;
            uint8_t u = (uint8_t)(128.0f + saturation * base_u);
            uint8_t v = (uint8_t)(128.0f + saturation * base_v);

            graphics.dotFast(x, y, {luma, u, v});
        }
    }
}


float phase = 0.0;
bool has_fr_buffer = false;

void setup(){
    esp_pm_lock_handle_t powerManagementLock;
    esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "compositeCorePerformanceLock", &powerManagementLock);
    esp_pm_lock_acquire(powerManagementLock);

    scanout_buffer.init(resolution, subsampling);
    has_fr_buffer = frame_buffer.init(resolution, subsampling);

    if(has_fr_buffer){graphics.set_buffer(frame_buffer);}
    else{graphics.set_buffer(scanout_buffer);} // we dont have enough Memory, draw directly.
    CompositeVideoBlitter::setup_video(scanout_buffer);

    graphics.setFont(font);
    graphics.setTextColor(Color::fromRGB(255,255,255));
    graphics.setCursor(graphics.xres / 2, graphics.yres / 2);
    audio.begin(26, 8000, 0, 100000);
    // i dont recommend pin 26 since its next to the video pin and generates noise
    // but i do it since i already soldered it ;-;

	test_rainbow();
    graphics.print("Mejolov24");
}

void loop()
{
    CompositeVideoBlitter::video_sync();
    if(has_fr_buffer){scanout_buffer.pushFromBuffer(frame_buffer);}
    
    while (audio.available() < 1024) {
    int16_t sine_chunk[512];

    for (int i = 0; i < 512; i++) {
        sine_chunk[i] = (int16_t)(32767.0f * sinf(phase));
        phase += 2.0f * M_PI * 440 / 8000;
        
        if (phase >= 2.0f * M_PI) {
            phase -= 2.0f * M_PI;
        }
    }

    audio.fill_buffer(sine_chunk, 512);
    }
}