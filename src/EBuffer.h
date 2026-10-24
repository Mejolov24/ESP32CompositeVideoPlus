#ifndef EBUFFER_H
    #define EBUFFER_H
#include <Arduino.h>

enum class MemoryStorage_t{
    INTERNAL,
    SPIRAM,
    PREFER_INTERNAL,
    PREFER_SPIRAM,
    NONE
};

enum class MemoryType_t{
    EXECUTABLE,
    BIT_SIZE_32,
    BIT_SIZE_8,
    DMA_CAPABLE,
    MEMORY_DEFAULT,
    IRAM_8BIT,
    RETENTION_DMA,
    MEMORY_RTCAM,
};

enum class MemoryResult_t{
    SUCCESFUL,
    NOT_PREFERRED,
    FAILED,
    NONE
};

constexpr uint32_t toEspCaps(MemoryType_t type)
{
	switch (type) {
		case MemoryType_t::EXECUTABLE:		return MALLOC_CAP_EXEC;
		case MemoryType_t::BIT_SIZE_32:		return MALLOC_CAP_32BIT;
		case MemoryType_t::BIT_SIZE_8:		return MALLOC_CAP_8BIT;
		case MemoryType_t::DMA_CAPABLE:		return MALLOC_CAP_DMA;
		case MemoryType_t::MEMORY_DEFAULT:	return MALLOC_CAP_DEFAULT;
		case MemoryType_t::IRAM_8BIT:		return MALLOC_CAP_IRAM_8BIT;
		case MemoryType_t::RETENTION_DMA:	return MALLOC_CAP_RETENTION;
		case MemoryType_t::MEMORY_RTCAM:	return MALLOC_CAP_RTCRAM;
	}

	return 0;
}

class EBuffer{
    public:
    MemoryStorage_t memory_storage = MemoryStorage_t::NONE;
    MemoryType_t memory_type = MemoryType_t::MEMORY_DEFAULT;
    uint32_t* buffer32 = nullptr;
    uint16_t* buffer16 = nullptr;
    uint8_t* buffer = nullptr;
    uint32_t buffer_size = 0;
    
    MemoryResult_t init(uint32_t size, MemoryStorage_t storage, MemoryType_t type){
        buffer_size = size;
        memory_storage = storage;
        memory_type = type;
        uint32_t memory_raw_caps = toEspCaps(type);
        bool is_not_preferred = false;

        switch (storage){
        case MemoryStorage_t::INTERNAL:
            buffer = (uint8_t*)heap_caps_malloc(buffer_size,MALLOC_CAP_INTERNAL | memory_raw_caps);
            break;
        case MemoryStorage_t::SPIRAM:
            buffer = (uint8_t*)heap_caps_malloc(buffer_size,MALLOC_CAP_SPIRAM | memory_raw_caps);
            break;
        case MemoryStorage_t::PREFER_INTERNAL:
            buffer = (uint8_t*)heap_caps_malloc(buffer_size,MALLOC_CAP_INTERNAL | memory_raw_caps);
            if(!buffer){
                buffer = (uint8_t*)heap_caps_malloc(buffer_size,MALLOC_CAP_SPIRAM | memory_raw_caps);
                memory_storage = MemoryStorage_t::SPIRAM; 
                is_not_preferred = true;
            }
            break;
        case MemoryStorage_t::PREFER_SPIRAM:
            buffer = (uint8_t*)heap_caps_malloc(buffer_size,MALLOC_CAP_SPIRAM | memory_raw_caps);
            if(!buffer){
                buffer = (uint8_t*)heap_caps_malloc(buffer_size,MALLOC_CAP_INTERNAL | memory_raw_caps);
                memory_storage = MemoryStorage_t::INTERNAL; 
                is_not_preferred = true;
            }
            break;
        default:
            break;
        }
        buffer16 = reinterpret_cast<uint16_t*>(buffer);
        buffer32 = reinterpret_cast<uint32_t*>(buffer);
        if (!buffer) {return MemoryResult_t::FAILED;}
        if(is_not_preferred){return MemoryResult_t::NOT_PREFERRED;}
        return MemoryResult_t::SUCCESFUL;
    }

    void deinit(){
        heap_caps_free(buffer);
        memory_storage = MemoryStorage_t::NONE;
    }
    ~EBuffer(){deinit();}
    
};

#endif