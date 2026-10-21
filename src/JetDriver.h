#ifndef JET_DRIVER_H
    #define JET_DRIVER_H
    #include <stdint.h>
    #include <Arduino.h>
    #include <Jet.hpp>
    #include <JetConfig.hpp>
    #include <EBuffer.h>

#ifndef RENDER_DIVIDER
    #define RENDER_DIVIDER 8
#endif

enum class JetResult_t{
    ZBUFFER_FAILED,
    FRAMEBUFFER_FAILED,
    ALL_FAILED,
    SUCCESFUL
};


class JetBuffer{
    public:
    EBuffer frame_buffer;
    EBuffer z_buffer;
    uint16_t width = 0;
    uint16_t height = 0;
    uint16_t frame_buffer_width = 0;
    uint16_t frame_buffer_height = 0;

    JetResult_t init(uint16_t xres, uint16_t yres, MemoryStorage_t frame_buffer_type, MemoryStorage_t z_buffer_type){
        uint32_t z_buffer_size = 0;
        uint32_t frame_buffer_size = 0;
        width = xres;
        height = yres;

        #if HALF_WIDTH_BUFFERS
            frame_buffer_size = (xres / 2) * yres * sizeof(uint16_t);
            z_buffer_size = (xres / 2) * yres * sizeof(uint16_t);
            frame_buffer_width = xres / 2;
        #else
            frame_buffer_width = xres;
            frame_buffer_size = xres * yres * sizeof(uint16_t);
            z_buffer_size = xres * yres * sizeof(uint16_t);
        #endif
        frame_buffer_height = yres;

        #ifdef Z_BUFFERING
        
        #endif

        if (frame_buffer.memory_storage == MemoryStorage_t::NONE) {
            #ifdef Z_BUFFERING
                if (z_buffer.memory_storage == MemoryStorage_t::NONE) return JetResult_t::ALL_FAILED;
            #endif
            return JetResult_t::FRAMEBUFFER_FAILED;
        }

        #ifdef Z_BUFFERING
            if (z_buffer.memory_storage == MemoryStorage_t::NONE) return JetResult_t::ZBUFFER_FAILED;
        #endif

        return JetResult_t::SUCCESFUL;
    }
    
};

class JetDriver{
    public:
    Scene* create_scene(JetBuffer& buffer) {
        return new Scene(buffer.frame_buffer.buffer16, buffer.z_buffer.buffer16, buffer.width, buffer.height);
    }
    void delete_scene(Scene* scene) {delete scene;}

    #if __has_include(<Arduino_GFX.h>)
    void render(Scene& scene, Arduino_GFX* gfx, JetBuffer& buffer)
    {
        scene.render();
        const int width = buffer.width;
        const int height = buffer.height;

        constexpr int BATCH_ROWS = 4;
        static uint16_t batchBuffer[480 * BATCH_ROWS];

        for (int y = 0; y < height; y += BATCH_ROWS)
        {
            const int rows = (y + BATCH_ROWS <= height)
                ? BATCH_ROWS
                : height - y;

            for (int ry = 0; ry < rows; ry++)
            {
                const int outY = y + ry;

    #if FIELD_BUFFERS
                const int physRow = outY >> 1;
    #else
                const int physRow = outY;
    #endif

                uint16_t* dst = batchBuffer + ry * width;

    #if HALF_WIDTH_BUFFERS
                const uint16_t* src =
                    buffer.frame_buffer + physRow * (width / 2);

                for (int sx = 0; sx < width / 2; sx++)
                {
                    const uint16_t pixel = src[sx];
                    dst[sx * 2] = pixel;
                    dst[sx * 2 + 1] = pixel;
                }
    #else
                const uint16_t* src =
                    buffer.frame_buffer + physRow * width;

                memcpy(dst, src, width * sizeof(uint16_t));
    #endif
            }

            gfx->draw16bitRGBBitmap(
                0, y, batchBuffer, width, rows
            );
        }
    }
    #endif
};

#endif