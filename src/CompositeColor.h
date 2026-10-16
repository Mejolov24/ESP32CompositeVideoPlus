#ifndef COMPOSITE_COLOR_H
    #define COMPOSITE_COLOR_H
    #include <stdint.h>
    #include <algorithm>


struct Color {
    uint8_t y; // Luma: 0 to 255
    uint8_t u; // Chroma (B-Y): 0 to 255 (128 = neutral/no color)
    uint8_t v; // Chroma (R-Y): 0 to 255 (128 = neutral/no color)

    // Default constructor (Black)
    
    Color() : y(0), u(128), v(128) {}

    // Direct YUV constructor
    Color(uint32_t raw)
	: y((raw >> 16) & 0xFF),
	  u((raw >> 8) & 0xFF),
	  v(raw & 0xFF) {}
    Color(uint8_t _y, uint8_t _u, uint8_t _v) : y(_y), u(_u), v(_v) {}

    static Color fromRGB(uint8_t r, uint8_t g, uint8_t b){
        float y = 0.299f * r + 0.587f * g + 0.114f * b;
        // Normalize RGB to [0, 1].
        float R = r / 255.0f;
        float G = g / 255.0f;
        float B = b / 255.0f;

        // Custom chroma transform based on the measured screen colors.
        // Neutral chroma is centered at (128, 128).
        float u = 128.0f  + 128.0f * (-0.25f * R - 1.0f * G + 0.75f * B);

        float v = 128.0f + 128.0f * (-1.0f * R + 0.5f * G + 0.5f * B);

        auto clamp = [](float value) -> uint8_t {
            if (value < 0.0f) return 0;
            if (value > 255.0f) return 255;
            return (uint8_t)(value + 0.5f);
        };

        return Color(clamp(y), clamp(u), clamp(v));
    }

static Color fromRGBFast(uint8_t r, uint8_t g, uint8_t b)
{
	int32_t y = (299 * r + 587 * g + 114 * b + 500) / 1000;

	// Custom chroma transform, using fixed-point coefficients.
	int32_t u = 128 + (-32 * (int32_t)r - 128 * (int32_t)g + 96 * (int32_t)b + 127) / 255;

	int32_t v = 128 + (-128 * (int32_t)r + 64 * (int32_t)g + 64 * (int32_t)b + 127) / 255;

	auto clamp = [](int32_t value) -> uint8_t {
		if (value < 0) return 0;
		if (value > 255) return 255;
		return (uint8_t)value;
	};

	return Color(clamp(y), clamp(u), clamp(v));
}

};
#endif

