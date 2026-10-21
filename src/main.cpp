#include <Arduino.h>
#include <CompositeVideo.h>
#include <CompositeGraphics.h>
#include <CompositeColor.h>
#include <AudioPWM.h>
#include <Jet.hpp>
#include <JetDriver.h>
#include "font6x8.h"
#include <Cube.hpp>
#include <output.h>

#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_pm.h"
#include "esp_efuse.h"

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

// Jet Stuff
JetBuffer scene_buffer;
Scene* scene = nullptr;
JetDriver jet;
volatile int16_t fps = 0;
unsigned long lastFrameTime = 0;

TaskHandle_t jetTaskHandle = nullptr;
TaskHandle_t ArduinoloopTaskHandle = nullptr;


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
	}
}

void rainbow() {
    uint16_t xres = graphics.xres;
    uint16_t yres = graphics.yres;

    for (uint16_t x = 0; x < xres; x++) {
        float angle = (float)x / xres * 2.0f * PI;
        uint8_t U = 128 + (uint8_t)(112.0f * cosf(angle));
        uint8_t V = 128 + (uint8_t)(112.0f * sinf(angle));

        for (uint16_t y = 0; y < yres; y++) {
            uint8_t Y = map(y, 0, yres, 255, 0);
            graphics.dotFast(x, y, {Y, U, V});
        }
    }
}

static constexpr CompositeColor firePalette[37] = {
	{0, 128, 128},
	{15, 178, 161},
	{35, 169, 128},
	{55, 161, 96},
	{74, 153, 63},
	{76, 97, 1},
	{76, 97, 1},
	{79, 95, 2},
	{81, 93, 3},
	{83, 91, 4},
	{86, 89, 5},
	{114, 64, 17},
	{153, 32, 34},
	{191, 0, 50},
	{209, 0, 58},
	{212, 0, 59},
	{214, 0, 60},
	{217, 0, 61},
	{219, 0, 62},
	{221, 0, 63},
	{224, 0, 64},
	{226, 0, 65},
	{255, 65, 128},
	{255, 65, 128},
	{255, 65, 128},
	{255, 65, 128},
	{255, 65, 128},
	{255, 65, 128},
	{255, 65, 128},
	{255, 65, 128},
	{255, 65, 128},
	{255, 65, 128},
	{255, 65, 128},
	{255, 65, 128},
	{255, 65, 128},
	{255, 65, 128},
	{255, 65, 128}
};

uint32_t fast_random() {
    static uint32_t state = 0xACE1u; // Seed
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

inline uint8_t fast_rand_mod3() {
    static uint32_t state = 0xACE1u;
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    // Fast pseudo-modulo 3 using bitwise masking
    return (state & 3) > 2 ? 0 : (state & 3); 
}

void clearFire(uint8_t* firePixels, uint16_t xres, uint16_t yres) {
    for (int i = 0; i < xres * yres; i++) {
        firePixels[i] = 0;
    }
    for (int y = yres - 128; y < yres; y++) {
        memset(firePixels + y * xres, 36, xres);
    }
}

// Update fire propagation frame
void updateFire(uint8_t* firePixels, uint16_t xres, uint16_t yres)
{
	for (int y = 1; y < yres; y++) {
		int row = y * xres;
		int prevRow = row - xres;

		for (int x = 0; x < xres; x++) {
			uint8_t pixel = firePixels[row + x];

			if (pixel == 0) {
				firePixels[prevRow + x] = 0;
			} else {
				int randIdx = fast_rand_mod3();
				int destX = x - randIdx + 1;

				if (destX >= 0 && destX < xres) {
					firePixels[prevRow + destX] =
						pixel - (randIdx & 1);
				}
			}
		}
	}
}

uint8_t* fire_pixels = nullptr;

void fire_test(uint8_t res)
{
	int xres = 0;
	int yres = 240;

	switch (res) {
		case 0: xres = 256; break;
		case 1: xres = 320; break;
		case 2: xres = 336; break;
		default: return;
	}

	clearFire(fire_pixels, xres, yres);


	for (int i = 0; i < 150; i++) {
		updateFire(fire_pixels, xres, yres);

		for (int y = 0; y < yres; y++) {
			for (int x = 0; x < xres; x +=2) {
                CompositeColor c0 = firePalette[fire_pixels[y * xres + x]];
                CompositeColor c1 = firePalette[fire_pixels[y * xres + x + 1]];

                graphics.double_dot422(x, y, c0, c1);
			}
		}
	}
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
uint16_t current_tone = 0;


void subdivide()
{
        for(uint8_t frame = 0; frame < 4; frame++){
        uint16_t xres = graphics.xres;
        uint16_t yres = graphics.yres;
        rect_stack_size = 1;
        rect_stack[0] = {0, 0, xres, yres};
        while (rect_stack_size > 0)
        {
            Rect rect = rect_stack[--rect_stack_size];

            if (rect.w <= 2 || rect.h <= 2)
                continue;
            CompositeColor color = CompositeColor(
                fast_random() & 0xFF,
                fast_random() & 0xFF,
                fast_random() & 0xFF
            );

            if (rect.w >= rect.h)
            {
                uint16_t half = rect.w / 2;

                graphics.fillRect(rect.x, rect.y, half, rect.h, color);

                if (rect_stack_size + 2 <= MAX_RECTS)
                {
                    rect_stack[rect_stack_size++] = {
                        (uint16_t)(rect.x + half), rect.y,
                        (uint16_t)(rect.w - half), rect.h
                    };

                    rect_stack[rect_stack_size++] = {
                        rect.x, rect.y, half, rect.h
                    };
                }
            }
            else
            {
                uint16_t half = rect.h / 2;

                graphics.fillRect(rect.x, rect.y, rect.w, half, color);

                if (rect_stack_size + 2 <= MAX_RECTS)
                {
                    rect_stack[rect_stack_size++] = {
                        rect.x, (uint16_t)(rect.y + half),
                        rect.w, (uint16_t)(rect.h - half)
                    };

                    rect_stack[rect_stack_size++] = {
                        rect.x, rect.y, rect.w, half
                    };
                }
            }   
        }
    graphics.clear();
    }
}

template<typename... Args>
void print_tab_data(const char* name, Args... args) {
    graphics.print(name);
    (graphics.print(args), ...);   // fold expression, C++17
    graphics.print("\n");
}


const char* chip_model_name(esp_chip_model_t model) {
    switch (model) {
        case CHIP_ESP32:    return "ESP32";
        case CHIP_ESP32S2:  return "ESP32-S2";
        case CHIP_ESP32S3:  return "ESP32-S3";
        case CHIP_ESP32C3:  return "ESP32-C3";
        case CHIP_ESP32H2:  return "ESP32-H2";
        default:            return "Unknown";
    }
}

const char* ram_name(Memory_t ram) {
    switch (ram) {
        case Memory_t::SPIRAM : return "(DRAM) ";
        case Memory_t::INTERNAL : return "(SRAM) ";
        case Memory_t::NONE : return "(Disabled) ";
        default:            return "(Unknown) ";
    }
}

void DEVICE_INFO(){

    uint16_t xres = graphics.xres;
    uint16_t yres = graphics.yres;

    // --- Chip info ---
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    esp_chip_model_t model = chip_info.model;
    uint32_t cores = chip_info.cores;
    uint32_t revision = chip_info.revision;

    uint32_t flash_size = spi_flash_get_chip_size();

    size_t psram_size = 0;
    if (heap_caps_get_total_size(MALLOC_CAP_SPIRAM) > 0) {
        psram_size = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    }

    // --- Heap / SRAM / DRAM ---
    size_t total_internal   = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    size_t total_psram      = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);

    // --- Runtime info ---
    uint32_t free_rtos_heap = xPortGetFreeHeapSize();

    graphics.setCursor((xres/2 - 32), 16);
    graphics.print("DEVICE INFO");
    graphics.setCursor(16, 32);

    print_tab_data("Model : ", chip_model_name(chip_info.model));  // string overload
    print_tab_data("Cores : ", cores);
    print_tab_data("Revision : ", revision);
    print_tab_data("Flash : ", flash_size / 1024, " KB");
    print_tab_data("SRAM : ", total_internal / 1024, " KB");
    print_tab_data("PSRAM : ", total_psram / 1024, " KB");
    print_tab_data("FreeRTOS heap : ", free_rtos_heap / 1024, " KB");
}

void RAM_INFO(){
    uint16_t xres = graphics.xres;
    uint16_t yres = graphics.yres;
    graphics.setCursor(xres/2 - 16, 16);
    graphics.print("RAM INFO");
    graphics.setCursor(16, 32);
    print_tab_data("VRAM 0 (Scanout) ", ram_name(scanout_buffer.memory_type), scanout_buffer.total_buffer_size / 1024, " KB");
    print_tab_data("VRAM 1 (Buffering) ", ram_name(frame_buffer.memory_type), frame_buffer.total_buffer_size / 1024, " KB");
    print_tab_data("VRAM 3D 0 (FRAME) ", ram_name(scene_buffer.frame_memory_type), scene_buffer.frame_buffer_size / 1024, " KB");
    print_tab_data("VRAM 3D 1 (Z) ", ram_name(scene_buffer.z_memory_type), scene_buffer.z_buffer_size / 1024, " KB");
}

#define GRID_SIZE 32

void draw_grid(uint16_t offset_x, uint16_t offset_y) {
    uint16_t xres = graphics.xres;
    uint16_t yres = graphics.yres;

    int ox = ((offset_x % GRID_SIZE) + GRID_SIZE) % GRID_SIZE;
    int oy = ((offset_y % GRID_SIZE) + GRID_SIZE) % GRID_SIZE;

    // Vertical lines — full height
    for (int x = ox; x < xres; x += GRID_SIZE)
        graphics.line(x, 0, x, yres, rgb565_lut[0xb01f]);

    // Horizontal lines — full width
    for (int y = oy; y < yres; y += GRID_SIZE)
        graphics.line(0, y, xres, y, rgb565_lut[0xfc60]);
}


void dual_buffer(uint8_t res){
    uint16_t xres = graphics.xres;
    uint16_t yres = graphics.yres;
        switch (res){
        case 0:
            frame_buffer.init(VideoResolution_t::VIDEO_RES_NTSC_256x240P,subsampling,Memory_t::PREFER_SPIRAM);
            break;
        case 1:
            frame_buffer.init(VideoResolution_t::VIDEO_RES_NTSC_320x240P,subsampling,Memory_t::PREFER_SPIRAM);
            break;
        case 2:
            frame_buffer.init(VideoResolution_t::VIDEO_RES_NTSC_336x240P,subsampling,Memory_t::PREFER_SPIRAM);
            break;
        }
        delay(100);
        graphics.set_buffer(frame_buffer);

        for(uint16_t frame = 0; frame < 256; frame++){
            draw_grid(frame,frame);
            scanout_buffer.pushFromBuffer(frame_buffer);
            graphics.clear();
        }

        frame_buffer.deinit();
        graphics.set_buffer(scanout_buffer);
}

void jetRenderTask(void* parameter){
	while (true) {
		// Wait until scene->render() has finished.
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

		// Convert the complete framebuffer
		jet.render422(scene_buffer);
        graphics.setCursor(graphics.xres - 64,+8);
        graphics.print("FPS: ");
        graphics.print(fps);

		// Tell loop that the source framebuffer can be reused.
		xTaskNotifyGive(ArduinoloopTaskHandle);
	}
}

void test_jet(){
    for(int frame = 0; frame < 150; frame++){
	const unsigned long now = millis();

	const float delta_time = (now - lastFrameTime) / 1000.0f;
	lastFrameTime = now;

    Cube::update(delta_time);
const int64_t start = esp_timer_get_time();

scene->render();

const int64_t elapsed = esp_timer_get_time() - start;

fps = 1000000.0 / elapsed;
	// Signal that a complete source frame is ready.
	xTaskNotifyGive(jetTaskHandle);

	// Do not overwrite the source framebuffer while it is being read.
	ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
}

void resolution_set(VideoResolution_t res){
    
    scanout_buffer.deinit();

    scanout_buffer.init(res, subsampling,Memory_t::PREFER_INTERNAL);
    graphics.set_buffer(scanout_buffer);

    CompositeVideoBlitter::setup_video(scanout_buffer);
}

void display_test_name(const char *str){
    uint16_t xres = graphics.xres;
    uint16_t yres = graphics.yres;
    graphics.setCursor((xres / 2) - 32, (yres/2));
    graphics.print(str);
    delay(1000);
}

void test() {
    graphics.clear();
    for(uint16_t res = 0; res <= 2; res++){
    uint16_t xres = graphics.xres;
    uint16_t yres = graphics.yres;
    float hz = 440.0f * powf(2.0f, ((res + 72) - 69) / 12.0f);
        switch (res){
        case 0:
            display_test_name("NTSC_256x240P");
            delay(1000);
            resolution_set(VideoResolution_t::VIDEO_RES_NTSC_256x240P);
            break;
        case 1:
            display_test_name("NTSC_320x240P");
            delay(1000);
            resolution_set(VideoResolution_t::VIDEO_RES_NTSC_320x240P);
            break;
        case 2:
            display_test_name("NTSC_336x240P");
            delay(1000);
            resolution_set(VideoResolution_t::VIDEO_RES_NTSC_336x240P);
            break;
        
        default:
            break;
        }

        graphics.clear();
        RAM_INFO();
        delay(5000);
        graphics.clear();
        display_test_name("Color wheel");
        for (uint16_t note = 69; note <= 81; note++) {

            uint8_t Y = map(note, 69, 81, 0, 255);
            test_grayscale_picker(Y);
            test_rainbow_wheel(Y);
            graphics.clear();
        }
        display_test_name("Color Rainbow");
        rainbow();
        delay(5000);
        graphics.clear();
        display_test_name("Subdivide");
        subdivide();
        graphics.clear();
        display_test_name("Fire");
        fire_test(res);
        graphics.clear();
        display_test_name("Double buffering");
        dual_buffer(res);
        graphics.clear();
        display_test_name("JET ENGINE");
        test_jet();
        graphics.clear();
    }

}

void setup(){
    delay(500);
    esp_pm_lock_handle_t powerManagementLock;
    esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "compositeCorePerformanceLock", &powerManagementLock);
    esp_pm_lock_acquire(powerManagementLock);

    resolution_set(resolution);
    // setup Jet
    scene_buffer.init(256,240,Memory_t::PREFER_INTERNAL,Memory_t::PREFER_INTERNAL);
    scene = jet.create_scene(scene_buffer);
    Cube::init(*scene);
    ArduinoloopTaskHandle = xTaskGetCurrentTaskHandle();
    xTaskCreatePinnedToCore(
        jetRenderTask,
        "JetRender",
        8192,
        nullptr,
        10,
        &jetTaskHandle,
        0
    );

    graphics.setFont(font);
    graphics.setTextColor(CompositeColor::fromRGB(255,255,255));
    audio.begin(27, 8000, 0, 100000);
    // i dont recommend pin 26 since its next to the video pin and generates noise
    // but i do it since i already soldered it ;-;
    fire_pixels = (uint8_t*)heap_caps_malloc(336*240,MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    audio.set_buffer(output_song,output_song_len);

    DEVICE_INFO();
    delay(10000);
	
}
float current_offset = 0.0f;
void loop()
{
    CompositeVideoBlitter::video_sync();
    test();
}