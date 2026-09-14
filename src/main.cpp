//this is a test file
#define VIDEO_RES VIDEO_RES_NTSC_256x240P
#include <Arduino.h>
#include <AnalogVideo.h>
#include <CompositeGraphics.h>
CompositeGraphics graphics;
void test_rainbow() {
    uint16_t xres = graphics.xres;
    uint16_t yres = graphics.yres;

    // Saturation radius (max ~90 to avoid DAC voltage clipping)
    const float saturation = 90.0f; 

    for (uint16_t x = 0; x < xres; x++) {
        // Calculate Hue angle (0 to 2*PI) across X axis
        float angle = ((float)x / (float)xres) * 2.0f * M_PI;

        // Convert hue angle to U and V chroma vectors centered at 128
        uint8_t u = (uint8_t)(128.0f + saturation * cosf(angle));
        uint8_t v = (uint8_t)(128.0f + saturation * sinf(angle));

        for (uint16_t y = 0; y < yres; y++) {
            // Map Y axis to Luma (255 at top, 0 at bottom)
            uint8_t luma = map(y, 0, yres - 1, 255, 0);

            Color color = { luma, u, v };
            graphics.dotFast(x, y, color);
        }
    }
}

void setup(){
    RawCompositeVideoBlitter::video_init();
    graphics.init();
}

void loop(){
    graphics.begin();
    test_rainbow();
    RawCompositeVideoBlitter::video_sync();
}