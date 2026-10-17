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
Scene scene;
JetDriver jet;
unsigned long lastFrameTime = 0;


void setup(){
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
    scene_buffer.init(336,240,Memory_t::PREFER_INTERNAL,Memory_t::PREFER_INTERNAL);
    scene = jet.create_scene(scene_buffer);
    Cube::init(scene);
}

void loop()
{
    CompositeVideoBlitter::video_sync();

	const unsigned long now = millis();

	const float delta_time = (now - lastFrameTime) / 1000.0f;
	lastFrameTime = now;

    Cube::update(delta_time);
    jet.render(scene, scene_buffer);
}