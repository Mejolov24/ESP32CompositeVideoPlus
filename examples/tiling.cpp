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

uint16_t current_tone = 500;
uint16_t tone_step = 100;
uint16_t max_tone = 1000;

void generate_tone(){
  if(current_tone <= max_tone) {
    current_tone += tone_step;
  }
  else{
    current_tone = 500;
  }
}

uint32_t fast_random() {
    static uint32_t state = 0xACE1u; // Seed
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

struct Rect {
	uint16_t x;
	uint16_t y;
	uint16_t w;
	uint16_t h;
};


#define MAX_RECTS 64

Rect rect_stack[MAX_RECTS];
int rect_stack_size = 0;

uint32_t last_subdivide = 0;
uint32_t subdivide_interval = 0;

bool subdividing = false;

void start_subdivide()
{
	rect_stack_size = 1;
	rect_stack[0] = {0, 0, 336, 240};
	subdividing = true;
}

void update_subdivide()
{

	if (millis() - last_subdivide < subdivide_interval) {
		return;
	}

	last_subdivide = millis();

	if (!subdividing) {
        graphics.clear();
        start_subdivide();}

	if (rect_stack_size == 0) {
		subdividing = false;
		return;
	}
	Rect rect = rect_stack[--rect_stack_size];
	if (rect.w <= 2 || rect.h <= 2) {return;}

	current_tone = ((rect.x * rect.y) % 800) + 200;
	Color color = Color(fast_random(),fast_random(),fast_random());
	if (rect.w >= rect.h) {
		uint16_t half = rect.w / 2;
		graphics.fillRect(rect.x,rect.y,half,rect.h,color);
		rect_stack[rect_stack_size++] = {(uint16_t)(rect.x + half), rect.y, (uint16_t)(rect.w - half),rect.h};
		rect_stack[rect_stack_size++] = {rect.x,rect.y,half,rect.h};
	}
	else {
		uint16_t half = rect.h / 2;

		graphics.fillRect(rect.x,rect.y,rect.w,half,color);
		rect_stack[rect_stack_size++] = {rect.x, (uint16_t)(rect.y + half), rect.w, (uint16_t)(rect.h - half) };
		rect_stack[rect_stack_size++] = {rect.x, rect.y, rect.w, half};
	}
}
float phase = 0.0;

void setup(){
    Serial.begin(921600);
    esp_pm_lock_handle_t powerManagementLock;
    esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "compositeCorePerformanceLock", &powerManagementLock);
    esp_pm_lock_acquire(powerManagementLock);

    scanout_buffer.init(resolution, subsampling);
    bool has_fr_buffer = frame_buffer.init(resolution, subsampling);

    if(has_fr_buffer){graphics.set_buffer(frame_buffer);}
    else{graphics.set_buffer(scanout_buffer);} // we dont have enough Memory, draw directly.
    CompositeVideoBlitter::setup_video(scanout_buffer);

    graphics.setFont(font);
    graphics.setTextColor(Color::fromRGB(255,255,255));
    graphics.setCursor(graphics.xres / 2, graphics.yres / 2);
    audio.begin(26, 8000, 0, 100000);
    // i dont recommend pin 26 since its next to the video pin and generates noise
    // but i do it since i already soldered it ;-;
    start_subdivide();
}


void loop()
{
    //RawCompositeVideoBlitter::wait_for_vblank();
	//graphics.clear();
    update_subdivide();
    while (audio.available() < 1024) {
    int16_t sine_chunk[512];

    for (int i = 0; i < 512; i++) {
        sine_chunk[i] = (int16_t)(32767.0f * sinf(phase));
        phase += 2.0f * M_PI * current_tone / 8000;
        
        if (phase >= 2.0f * M_PI) {
            phase -= 2.0f * M_PI;
        }
    }

    audio.fill_buffer(sine_chunk, 512);
    }
}