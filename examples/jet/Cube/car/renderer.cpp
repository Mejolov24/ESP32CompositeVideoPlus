#include <Arduino.h>
#include <CompositeVideo.h>
#include <CompositeGraphics.h>
#include <CompositeColor.h>
#include <AudioPWM.h>
#include "esp_pm.h"
#include "font6x8.h"
#include <Jet.hpp>
#include <JetDriver.h>
#include <Workshop.hpp>
#include <Workshop.hpp>

// Video Stuff
Font<CompositeGraphics> font(6, 8, font6x8::pixels);
CompositeVideoBuffer scanout_buffer; // where we push

VideoResolution_t resolution = VideoResolution_t::VIDEO_RES_NTSC_256x240P ;
SubSampling_t subsampling = SubSampling_t::SUBSAMPLING_422; // recommended, saves memory and looks almost identical
CompositeGraphics graphics;

// Jet Stuff
JetBuffer scene_buffer;
Scene* scene = nullptr;
JetDriver jet;
unsigned long lastFrameTime = 0;

TaskHandle_t jetTaskHandle = nullptr;
TaskHandle_t ArduinoloopTaskHandle = nullptr;

volatile int16_t fps = 0;

void jetRenderTask(void* parameter)
{
	while (true) {
		// Wait until scene->render() has finished.
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

		// Convert the complete framebuffer
		jet.render(scene_buffer);
        graphics.setCursor(graphics.xres - 64,+8);
        graphics.print("FPS: ");
        graphics.print(fps);


		// Tell loop() that the source framebuffer can be reused.
		xTaskNotifyGive(ArduinoloopTaskHandle);
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
    scene_buffer.init(256,240,Memory_t::PREFER_INTERNAL,Memory_t::PREFER_INTERNAL);
    scene = jet.create_scene(scene_buffer);
    Workshop::init(*scene);
    ArduinoloopTaskHandle = xTaskGetCurrentTaskHandle();
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

    Workshop::update(delta_time);
const int64_t start = esp_timer_get_time();

scene->render();

const int64_t elapsed = esp_timer_get_time() - start;

fps = 1000000.0 / elapsed;
	// Signal that a complete source frame is ready.
	xTaskNotifyGive(jetTaskHandle);

	// Do not overwrite the source framebuffer while it is being read.
	ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
} 