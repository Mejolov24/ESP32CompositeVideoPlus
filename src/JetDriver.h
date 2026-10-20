#ifndef JET_DRIVER_H
    #define JET_DRIVER_H
    #include <Arduino.h>
    #include <Jet.hpp>
    #include <JetConfig.hpp>
    #include <CompositeVideo.h>

#ifndef RENDER_DIVIDER
    #define RENDER_DIVIDER 8
#endif

enum class Memory_t{
    INTERNAL,
    SPIRAM,
    PREFER_INTERNAL,
    PREFER_SPIRAM,

    ZBUFFER_FAILED,
    FRAMEBUFFER_FAILED,
    ALL_FAILED,
    SUCCESFUL
};

class JetBuffer{
    public:
    uint16_t* frame_buffer = nullptr;
    uint16_t* z_buffer = nullptr;
    uint16_t width = 0;
    uint16_t height = 0;
    
    Memory_t init(uint16_t xres, uint16_t yres, Memory_t frame_buffer_type, Memory_t z_buffer_type){
        uint32_t z_buffer_size = 0;
        uint32_t frame_buffer_size = 0;
        width = xres;
        height = yres;

        #if HALF_WIDTH_BUFFERS
            frame_buffer_size = (xres / 2) * yres * sizeof(uint16_t);
            z_buffer_size = (xres / 2) * yres * sizeof(uint16_t);
        #else
            frame_buffer_size = xres * yres * sizeof(uint16_t);
            z_buffer_size = xres * yres * sizeof(uint16_t);
        #endif
        
        switch (frame_buffer_type){
        case Memory_t::INTERNAL:
            frame_buffer = (uint16_t*)heap_caps_malloc(frame_buffer_size,MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
            break;
        case Memory_t::SPIRAM:
            frame_buffer = (uint16_t*)heap_caps_malloc(frame_buffer_size,MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            break;
        case Memory_t::PREFER_INTERNAL:
            frame_buffer = (uint16_t*)heap_caps_malloc(frame_buffer_size,MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
            if(!frame_buffer){frame_buffer = (uint16_t*)heap_caps_malloc(frame_buffer_size,MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);}
            break;
        case Memory_t::PREFER_SPIRAM:
            frame_buffer = (uint16_t*)heap_caps_malloc(frame_buffer_size,MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if(!frame_buffer){frame_buffer = (uint16_t*)heap_caps_malloc(frame_buffer_size,MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);}
            break;
        default:
            break;
        }

        #ifdef Z_BUFFERING
        switch (z_buffer_type){
        case Memory_t::INTERNAL:
            z_buffer = (uint16_t*)heap_caps_malloc(z_buffer_size,MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
            break;
        case Memory_t::SPIRAM:
            z_buffer = (uint16_t*)heap_caps_malloc(z_buffer_size,MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            break;
        case Memory_t::PREFER_INTERNAL:
            z_buffer = (uint16_t*)heap_caps_malloc(z_buffer_size,MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
            if(!z_buffer){z_buffer = (uint16_t*)heap_caps_malloc(z_buffer_size,MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);}
            break;
        case Memory_t::PREFER_SPIRAM:
            z_buffer = (uint16_t*)heap_caps_malloc(z_buffer_size,MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if(!z_buffer){z_buffer = (uint16_t*)heap_caps_malloc(z_buffer_size,MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);}
            break;
        default:
            break;
        }
        #endif

        if (!frame_buffer) {
            #ifdef Z_BUFFERING
                if (!z_buffer) return Memory_t::ALL_FAILED;
            #endif
            return Memory_t::FRAMEBUFFER_FAILED;
        }

        #ifdef Z_BUFFERING
            if (!z_buffer) return Memory_t::ZBUFFER_FAILED;
        #endif

        return Memory_t::SUCCESFUL;
    }

    void deinit(){
        heap_caps_free(frame_buffer);
        heap_caps_free(z_buffer);
    }
    
};

class JetDriver{
    public:
    Scene* create_scene(JetBuffer& buffer) {
        return new Scene(buffer.frame_buffer, buffer.z_buffer, buffer.width, buffer.height);
    }
    void delete_scene(Scene* scene) {
        delete scene;
    }

    void render(JetBuffer& buffer){

        for (int y = 0; y < buffer.height; y++){
            const uint16_t* src;

        #if FIELD_BUFFERS
                const int physRow = y >> 1;
        #else
                const int physRow = y;
        #endif

        #if HALF_WIDTH_BUFFERS
            src = buffer.frame_buffer + physRow * (buffer.width / 2);
            for (int x = 0; x < buffer.width; x++)
            {
                const uint16_t rgb = src[x >> 1];
                CompositeVideoBlitter::set_pixel(x,y,rgb565_lut[rgb]);
            }
        #else
                src = buffer.frame_buffer + physRow * buffer.width;

                for (int x = 0; x < buffer.width; x++)
                {
                    const uint16_t rgb = src[x];
                    CompositeVideoBlitter::set_pixel(x,y,rgb565_lut[rgb]);
                }
        #endif
        }
    }


    void render422(JetBuffer& buffer){

        for (int y = 0; y < buffer.height; y++){
            const uint16_t* src;

        #if FIELD_BUFFERS
                const int physRow = y >> 1;
        #else
                const int physRow = y;
        #endif

        #if HALF_WIDTH_BUFFERS
            src = buffer.frame_buffer + physRow * (buffer.width / 2);
            for (int x = 0; x < buffer.width; x++){
				const uint16_t rgb = src[x >> 1];
				const CompositeColor c = rgb565_lut[rgb];

				CompositeVideoBlitter::double_dot422(x, y, c, c);
            }
        #else
                src = buffer.frame_buffer + physRow * buffer.width;

                for (int x = 0; x < buffer.width; x += 2){
                    const uint16_t rgb = src[x];
                    const CompositeColor c0 = rgb565_lut[src[x]];
                    const CompositeColor c1 = rgb565_lut[src[x + 1]];
                    CompositeVideoBlitter::double_dot422(x,y,c0,c1);
                }
        #endif
        }
    }

int current_line = 0;

void renderScanlinePacked(JetBuffer& buffer){
        if(current_line >= buffer.height){
            current_line = 0;
        }
        int target_line = current_line + (buffer.height / RENDER_DIVIDER);
        if (target_line > buffer.height) target_line = buffer.height;


        for (current_line; current_line < target_line; current_line++){
            const uint16_t* src;

        #if FIELD_BUFFERS
                const int physRow = current_line >> 1;
        #else
                const int physRow = current_line;
        #endif

        #if HALF_WIDTH_BUFFERS
            src = buffer.frame_buffer + physRow * (buffer.width / 2);
            for (int x = 0; x < buffer.width; x++)
            {
                const uint16_t rgb = src[x >> 1];
                CompositeVideoBlitter::set_pixel(x,current_line,rgb565_lut[rgb]);
            }
        #else
                src = buffer.frame_buffer + physRow * buffer.width;

                for (int x = 0; x < buffer.width; x++)
                {
                    const uint16_t rgb = src[x];
                    CompositeVideoBlitter::set_pixel(x,current_line,rgb565_lut[rgb]);
                }
        #endif
        }
}
void renderScanlineCore(bool core,JetBuffer& buffer)
{
	const int half_height = buffer.height / 2;

	int start_line = core ? 0 : half_height;
	const int target_line = core ? half_height : buffer.height;


        for (start_line; start_line < target_line; start_line++){
            const uint16_t* src;

        #if FIELD_BUFFERS
                const int physRow = start_line >> 1;
        #else
                const int physRow = start_line;
        #endif

        #if HALF_WIDTH_BUFFERS
            src = buffer.frame_buffer + physRow * (buffer.width / 2);
            for (int x = 0; x < buffer.width; x++)
            {
                const uint16_t rgb = src[x >> 1];
                CompositeVideoBlitter::set_pixel(x,start_line,rgb565_lut[rgb]);
            }
        #else
                src = buffer.frame_buffer + physRow * buffer.width;

                for (int x = 0; x < buffer.width; x++)
                {
                    const uint16_t rgb = src[x];
                    CompositeVideoBlitter::set_pixel(x,start_line,rgb565_lut[rgb]);
                }
        #endif
        }
}

};

#endif