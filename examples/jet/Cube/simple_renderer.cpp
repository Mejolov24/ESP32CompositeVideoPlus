#include <Arduino.h>
#include <CompositeVideo.h>
#include <CompositeGraphics.h>
#include <CompositeColor.h>
#include <AudioPWM.h>
#include "esp_pm.h"
#include "font6x8.h"
#include <Jet.hpp>
#include <JetDriver.h>
#include <Cube.hpp>

// Video Stuff
Font<CompositeGraphics> font(6, 8, font6x8::pixels);
CompositeVideoBuffer scanout_buffer; // where we push

VideoResolution_t resolution = VideoResolution_t::VIDEO_RES_NTSC_336x240P;
SubSampling_t subsampling = SubSampling_t::SUBSAMPLING_422; // recommended, saves memory and looks almost identical
CompositeGraphics graphics;

// Jet Stuff
JetBuffer scene_buffer;
Scene* scene = nullptr;
JetDriver jet;
unsigned long lastFrameTime = 0;

TaskHandle_t jetTaskHandle = nullptr;

void scanline_render_test()
{
	const int lead = 8;

	int scanline =
		CompositeVideoBlitter::get_current_scanline();

	scanline = (scanline + lead) % graphics.yres;

	graphics.dot(
		graphics.xres / 2,
		scanline,
		CompositeColor::fromRGBFast(0, 255, 0)
	);
}

void jetRenderTask(void* parameter)
{
	uint32_t batches = 0;

	while (true) {
		jet.renderScanline(scene_buffer);

		if (++batches >= 100) {
			batches = 0;
			vTaskDelay(1);
		}
	}
}

void setup(){
    Serial.begin(115200);
    esp_pm_lock_handle_t powerManagementLock;
    esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "compositeCorePerformanceLock", &powerManagementLock);
    esp_pm_lock_acquire(powerManagementLock);

    // setup Video
    scanout_buffer.init(resolution, subsampling);

    graphics.set_buffer(scanout_buffer);
    CompositeVideoBlitter::setup_video(scanout_buffer);

    graphics.setFont(font);
    graphics.setTextColor(CompositeColor::fromRGB(255,255,255));

    // setup Jet
    scene_buffer.init(336,240,Memory_t::PREFER_SPIRAM,Memory_t::PREFER_SPIRAM);
    scene = jet.create_scene(scene_buffer);
    Cube::init(*scene);
    xTaskCreatePinnedToCore(
        jetRenderTask,
        "JetRender",
        8192,
        nullptr,
        1,
        &jetTaskHandle,
        0
    );
}



void loop()
{

	const unsigned long now = millis();

	const float delta_time = (now - lastFrameTime) / 1000.0f;
	lastFrameTime = now;

    Cube::update(delta_time);
    scene->render();
}