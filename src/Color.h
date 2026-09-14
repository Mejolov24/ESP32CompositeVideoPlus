#include <stdint.h>
#include <algorithm>

#ifndef COMPOSITE_COLOR_H
    #define COMPOSITE_COLOR_Hstruct
struct Color {
    uint8_t y; // Luma: 0 to 255
    uint8_t u; // Chroma (B-Y): 0 to 255 (128 = neutral/no color)
    uint8_t v; // Chroma (R-Y): 0 to 255 (128 = neutral/no color)

    // Default constructor (Black)
    Color() : y(0), u(128), v(128) {}

    // Direct YUV constructor
    Color(uint8_t _y, uint8_t _u, uint8_t _v) : y(_y), u(_u), v(_v) {}

    // RGB (0..255) to YUV (BT.601 Full Range)
    static Color fromRGB(uint8_t r, uint8_t g, uint8_t b) {
        float y_val =  0.299000f * r + 0.587000f * g + 0.114000f * b;
        float u_val = -0.168736f * r - 0.331264f * g + 0.500000f * b + 128.0f;
        float v_val =  0.500000f * r - 0.418688f * g - 0.081312f * b + 128.0f;

        return Color(
            (uint8_t)std::min(255.0f, std::max(0.0f, y_val)),
            (uint8_t)std::min(255.0f, std::max(0.0f, u_val)),
            (uint8_t)std::min(255.0f, std::max(0.0f, v_val))
        );
    }

    // Fast integer-only RGB to YUV helper (no floating point)
    static Color fromRGBFast(uint8_t r, uint8_t g, uint8_t b) {
        int y_val = ( 77 * r + 150 * g +  29 * b) >> 8;
        int u_val = ((-43 * r -  85 * g + 128 * b) >> 8) + 128;
        int v_val = ((128 * r - 107 * g -  21 * b) >> 8) + 128;

        return Color(
            (uint8_t)std::min(255, std::max(0, y_val)),
            (uint8_t)std::min(255, std::max(0, u_val)),
            (uint8_t)std::min(255, std::max(0, v_val))
        );
    }
};
#endif

