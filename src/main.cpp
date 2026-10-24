#include <Arduino.h>
#include <CompositeVideo.h>
#include <CompositeColor.h>
#include <AudioPWM.h>
#include "esp_pm.h"
#include <LovyanGFX.h>
LovyanGFX lcd; // Base LGFX device context needed for color conversions
LGFX_Sprite display(&lcd);

AudioPWM audio;

CompositeVideoBuffer scanout_buffer; // where we push'¿
CompositeVideoBuffer frame_buffer; // where we render

VideoResolution_t resolution = VideoResolution_t::VIDEO_RES_NTSC_336x240P;

void test_rainbow()
{
for (int y = 0; y < 240; y++) {
	for (int x = 0; x < 336; x++) {
		uint8_t hue = (x * 256) / 336;

		uint8_t r, g, b;

		if (hue < 43) {
			r = 255;
			g = hue * 6;
			b = 0;
		} else if (hue < 86) {
			r = 255 - (hue - 43) * 6;
			g = 255;
			b = 0;
		} else if (hue < 128) {
			r = 0;
			g = 255;
			b = (hue - 86) * 6;
		} else if (hue < 171) {
			r = 0;
			g = 255 - (hue - 128) * 6;
			b = 255;
		} else if (hue < 213) {
			r = (hue - 171) * 6;
			g = 0;
			b = 255;
		} else {
			r = 255;
			g = 0;
			b = 255 - (hue - 213) * 6;
		}

		uint16_t color =
			((r >> 3) << 11) |
			((g >> 2) << 5) |
			(b >> 3);

		display.drawPixel(x, y, color);
	}
}
}

float phase = 0.0;
bool has_fr_buffer = false;

void setup(){
    esp_pm_lock_handle_t powerManagementLock;
    esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "compositeCorePerformanceLock", &powerManagementLock);
    esp_pm_lock_acquire(powerManagementLock);

    scanout_buffer.init(resolution, MemoryStorage_t::PREFER_INTERNAL);
    CompositeVideoBlitter::setup_video(scanout_buffer);
    display.setColorDepth(16);
    display.setBuffer(scanout_buffer.buffer,336,240);
    display.fillScreen(0xFFE0);
    display.fillRect(0,0,32,32,0xFFFF);

	//test_rainbow();
}

void loop()
{
    CompositeVideoBlitter::video_sync();
}