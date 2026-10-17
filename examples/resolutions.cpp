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

VideoResolution_t resolution = VideoResolution_t::VIDEO_RES_NTSC_256x240P;
SubSampling_t subsampling = SubSampling_t::SUBSAMPLING_422; // recommended, saves memory and looks almost identical

CompositeGraphics graphics;

#define AUDIO_SAMPLE_RATE 8000
#define TONE_CHUNK_SIZE 128

float phase = 0.0f;

struct ToneState {
	uint16_t hz = 0;
	uint32_t samples_remaining = 0;
	bool active = false;
};

ToneState tone_state;

void _tone(uint16_t hz, uint16_t time) {
	if (hz == 0 || time == 0) {
		tone_state.active = false;
		return;
	}

	tone_state.hz = hz;
	tone_state.samples_remaining =
		((uint32_t)AUDIO_SAMPLE_RATE * time) / 1000;

	tone_state.active = true;
}

void tone_update() {
	if (!tone_state.active) return;

	int16_t samples[TONE_CHUNK_SIZE];
	uint32_t queued = audio.available();

	// Keep some room in the ring buffer.
	if (queued >= 1024 - TONE_CHUNK_SIZE) return;

	uint32_t count = tone_state.samples_remaining;
	if (count > TONE_CHUNK_SIZE) count = TONE_CHUNK_SIZE;

	for (uint32_t i = 0; i < count; i++) {
		samples[i] = (int16_t)(32767.0f * sinf(phase));

		phase += 2.0f * M_PI * tone_state.hz / AUDIO_SAMPLE_RATE;
		if (phase >= 2.0f * M_PI) {
			phase -= 2.0f * M_PI;
		}
	}

	audio.fill_buffer(samples, count);
	tone_state.samples_remaining -= count;

	if (tone_state.samples_remaining == 0) {
		tone_state.active = false;
	}
}

void toneBlocking(uint16_t hz, uint16_t time)
{
	if (hz == 0 || time == 0)
		return;

	int16_t samples[TONE_CHUNK_SIZE];
	uint32_t samples_remaining =
		((uint32_t)AUDIO_SAMPLE_RATE * time) / 1000;

	const float phase_step = 2.0f * PI * hz / AUDIO_SAMPLE_RATE;

	while (samples_remaining > 0) {
		if (audio.available() >= 1024 - TONE_CHUNK_SIZE) {
			delay(1);
			continue;
		}

		uint32_t count = samples_remaining;
		if (count > TONE_CHUNK_SIZE)
			count = TONE_CHUNK_SIZE;

		for (uint32_t i = 0; i < count; i++) {
			samples[i] = (int16_t)(32767.0f * sinf(phase));

			phase += phase_step;
			if (phase >= 2.0f * PI)
				phase -= 2.0f * PI;
		}

		audio.fill_buffer(samples, count);
		samples_remaining -= count;
	}

	// Wait for the queued audio to finish playing.
	while (audio.available() > 0)
		delay(1);
}

void test_grayscale_picker(uint8_t Y){
    // bar render
    uint16_t xres = graphics.xres;
    uint16_t yres = graphics.yres;
    uint16_t w = xres / 16;
    uint16_t h = yres;
    uint16_t x = graphics.xres - w;
    uint16_t y = 0;


    CompositeColor color_bottom = CompositeColor(0, 128, 128);
    CompositeColor color_top = CompositeColor(255, 128, 128);

    graphics.fillRectGradient(x,y,w,h,color_bottom,color_top,false);

    // arrow render
    uint16_t x2_size = 16;
    uint16_t y2_size = 8;
    
    uint16_t x2 = x - x2_size - y2_size;
    uint16_t y2 = map(Y,0,255,0,yres - y2_size);
    graphics.fillRect(x2,y2,x2_size,y2_size,CompositeColor(255,128,128));

    short v0[] = {x2 + x2_size, y2 - y2_size};
    short v1[] = {x2 + x2_size, y2 + (y2_size*2)};
    short v2[] = {x2 + x2_size + y2_size, y2 + (y2_size/2)};

    graphics.triangle(v0, v1, v2, CompositeColor(255, 128, 128));
    char buffer[128];
    sprintf(buffer, "Y = %u", Y);
    graphics.setCursor((xres / 2), 16);
    graphics.print(buffer);
}

void test_rainbow_wheel(uint8_t Y)
{
    
    uint16_t xres = graphics.xres;
    uint16_t yres = graphics.yres;
    uint16_t x = xres/2 - xres/6;
    uint16_t y = yres/2;
    const int radius = y - float(yres) / 1.1f;
    const int angle = 360;
    const int precision = 3;
	const int chroma = 128;


	for (int i = 0; i < 360 * precision; i++) {
		float degrees = 90.0f + i / float(precision);
		float theta = degrees * PI / 180.0f;

		int x1 = x + radius * cosf(theta);
		int y1 = y + radius * sinf(theta);

		CompositeColor color = CompositeColor(Y, 128 + chroma * cosf(theta), 128 + chroma * sinf(theta) );

		int x2 = x + (radius + 1) * cosf(theta);
		int y2 = y + (radius + 1) * sinf(theta);

		graphics.line(x, y, x1, y1, color);
		graphics.line(x, y, x2, y2, color);
        tone_update();
	}
    tone_update();
}
bool has_fr_buffer = false;
void resolution_set(VideoResolution_t res){
    
    scanout_buffer.deinit();
    frame_buffer.deinit();

    scanout_buffer.init(res, subsampling);
    has_fr_buffer = false; //frame_buffer.init(res, subsampling);

    if(has_fr_buffer){graphics.set_buffer(frame_buffer);}
    else{graphics.set_buffer(scanout_buffer);} // we dont have enough Memory, draw directly.

    CompositeVideoBlitter::setup_video(scanout_buffer);
}

void test() {
    for(uint16_t res = 0; res <= 2; res++){
    uint16_t xres = graphics.xres;
    uint16_t yres = graphics.yres;
    float hz = 440.0f * powf(2.0f, ((res + 72) - 69) / 12.0f);
        switch (res)
        {
        case 0:
            graphics.setCursor((xres / 2) - 32, (yres/2));
            graphics.print("NTSC_256x240P");
            toneBlocking(hz,1500);
            resolution_set(VideoResolution_t::VIDEO_RES_NTSC_256x240P);
            break;
        case 1:
            graphics.setCursor((xres / 2) - 32, (yres/2));
            graphics.print("NTSC_320x240P");
            toneBlocking(hz,1500);
            resolution_set(VideoResolution_t::VIDEO_RES_NTSC_320x240P);
            break;
        case 2:
            graphics.setCursor((xres / 2) - 32, (yres/2));
            graphics.print("NTSC_336x240P");
            toneBlocking(hz,1500);
            resolution_set(VideoResolution_t::VIDEO_RES_NTSC_336x240P);
            break;
        
        default:
            break;
        }


        graphics.clear();
        delay(100);

        for (uint16_t note = 69; note <= 81; note++) {
            float hz = 440.0f * powf(2.0f, (note - 69) / 12.0f);
            _tone(hz, 800);

            uint8_t Y = map(note, 69, 81, 0, 255);
            test_grayscale_picker(Y);
            test_rainbow_wheel(Y);
            graphics.clear();
        }
    }

}
// YUV COLORSPACE:
// 0,0,0 orange
// 0,255,0 purple
// 0,0,255 green
// 0,255,255 blue

// YUV COLORSPACE:
// 0128,0,0 yellow
// 0128,255,0 pink
// 0128,0,255 green
// 0128,255,255 light blue


void setup(){
    delay(500);
    esp_pm_lock_handle_t powerManagementLock;
    esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "compositeCorePerformanceLock", &powerManagementLock);
    esp_pm_lock_acquire(powerManagementLock);

    resolution_set(resolution);

    graphics.setFont(font);
    graphics.setTextColor(CompositeColor::fromRGB(255,255,255));
    audio.begin(27, 8000, 0, 100000);
    // i dont recommend pin 26 since its next to the video pin and generates noise
    // but i do it since i already soldered it ;-;

	
}
float current_offset = 0.0f;
void loop()
{
    CompositeVideoBlitter::video_sync();
    test();
    if(has_fr_buffer){scanout_buffer.pushFromBuffer(frame_buffer);}
}