#ifndef COMPOSITE_COLOR_H
    #define COMPOSITE_COLOR_H
    #include <stdint.h>
    #include <algorithm>
    #include <array>

struct CompositeColor {
    uint8_t y; // Luma: 0 to 255
    uint8_t u; // Chroma (B-Y): 0 to 255 (128 = neutral/no color)
    uint8_t v; // Chroma (R-Y): 0 to 255 (128 = neutral/no color)

    // Default constructor (Black)
    
    constexpr CompositeColor() : y(0), u(128), v(128) {}

    // Direct YUV constructor
    constexpr CompositeColor(uint32_t raw)
	: y((raw >> 16) & 0xFF),
	  u((raw >> 8) & 0xFF),
	  v(raw & 0xFF) {}
    constexpr CompositeColor(uint8_t _y, uint8_t _u, uint8_t _v) : y(_y), u(_u), v(_v) {}

    static constexpr uint8_t clamp(int32_t value)
    {
        return value < 0 ? 0 : (value > 255 ? 255 : value);
    }

    static constexpr CompositeColor fromRGB(uint8_t r, uint8_t g, uint8_t b){
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

        return CompositeColor(clamp(y), clamp(u), clamp(v));
    }

static constexpr CompositeColor fromRGBFast(uint8_t r, uint8_t g, uint8_t b)
{
	const int32_t y = (299 * r + 587 * g + 114 * b + 500) / 1000;
	const int32_t u = 128 + (-32 * (int32_t)r - 128 * (int32_t)g + 96 * (int32_t)b + 127) / 255;
	const int32_t v = 128 + (-128 * (int32_t)r + 64 * (int32_t)g + 64 * (int32_t)b + 127) / 255;

	return CompositeColor(clamp(y), clamp(u), clamp(v));
}
static constexpr CompositeColor fromRGBFast565(uint16_t rgb){
	const uint8_t r5 = (rgb >> 11) & 0x1F;
	const uint8_t g6 = (rgb >> 5) & 0x3F;
	const uint8_t b5 = rgb & 0x1F;
	 // Expand RGB565 components to 8-bit RGB.
	const uint8_t r = (r5 << 3) | (r5 >> 2);
	const uint8_t g = (g6 << 2) | (g6 >> 4);
	const uint8_t b = (b5 << 3) | (b5 >> 2);

	return fromRGBFast(r, g, b);
}

};

constexpr std::array<CompositeColor, 65536> makeRGB565LUT()
{
	std::array<CompositeColor, 65536> lut{};

	for (uint32_t i = 0; i < 65536; i++)
		lut[i] = CompositeColor::fromRGBFast565(i);

	return lut;
}

inline constexpr auto rgb565_lut = makeRGB565LUT();

#endif
