
/* Copyright (c) 2020, Peter Barrett
**
** Permission to use, copy, modify, and/or distribute this software for
** any purpose with or without fee is hereby granted, provided that the
** above copyright notice and this permission notice appear in all copies.
**
** THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
** WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR
** BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES
** OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
** WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
** ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
** SOFTWARE.
*/

/**
 * Change log:
 *
 * Oct 2026 - Original code from https://github.com/rossumur/esp_8_bit
 *            Modified by Marcio Teixeira to extract Analog color generaation
 *            and make it compatible with Bitluni's code
 * 
 */
#ifndef ANALOG_VIDEO_H
    #define ANALOG_VIDEO_H

#include <Arduino.h>
#include "esp_heap_caps.h"
#include "esp_attr.h"
#include "esp_intr_alloc.h"
#include "soc/rtc.h"
#include "soc/i2s_struct.h"
#include "soc/i2s_reg.h"
#include "rom/lldesc.h"
#include "driver/periph_ctrl.h"
#include "driver/dac.h"
#include "driver/gpio.h"
#include "driver/i2s.h"
#include "CompositeColor.h"

enum class SubSampling_t{
    SUBSAMPLING_444, // Full color res
    SUBSAMPLING_422, // Half color res, almost identical to the human eye as full color. (recommended)
    SUBSAMPLING_400  // No color
};


#define SUBSAMPLING_BYTES(sub) ( \
    (sub) == SubSampling_t::SUBSAMPLING_444 ? 3 : \
    (sub) == SubSampling_t::SUBSAMPLING_422 ? 2 : 1)
enum class VideoResolution_t{
    VIDEO_RES_NTSC_256x240P,
    VIDEO_RES_NTSC_320x240P,
    VIDEO_RES_NTSC_336x240P,
    VIDEO_RES_PAL_256x288P,
    VIDEO_RES_PAL_320x288P,
    VIDEO_RES_PAL_352x288P,
    VIDEO_RES_PAL_384x288P,
};

struct VideoMode_t{
    VideoResolution_t _video_mode;
    uint16_t width;
    uint16_t height;
    bool is_pal;

    uint16_t line_width;
    SubSampling_t color_mode;
};

constexpr VideoMode_t _video_modes[] = {
    // NTSC 240p (262 scanlines)
    { VideoResolution_t::VIDEO_RES_NTSC_256x240P, 256, 240, false },
    { VideoResolution_t::VIDEO_RES_NTSC_320x240P, 320, 240, false },
    { VideoResolution_t::VIDEO_RES_NTSC_336x240P, 336, 240, false },
    // PAL 288p (312 scanlines)
    { VideoResolution_t::VIDEO_RES_PAL_256x288P, 256, 288, true  },
    { VideoResolution_t::VIDEO_RES_PAL_320x288P, 320, 288, true  },
    { VideoResolution_t::VIDEO_RES_PAL_352x288P, 352, 288, true  },
    { VideoResolution_t::VIDEO_RES_PAL_384x288P, 384, 288, true  },
};

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

class CompositeVideoBuffer{
    private:

    uint8_t* allocate_line_buffer(uint32_t line_bytes, Memory_t requested, Memory_t& actual) {
        uint8_t* buffer = nullptr;
        switch (requested) {
            case Memory_t::INTERNAL:
                buffer = (uint8_t*)heap_caps_malloc(line_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
                actual = Memory_t::INTERNAL;
                break;
            case Memory_t::SPIRAM:
                buffer = (uint8_t*)heap_caps_malloc(line_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
                actual = Memory_t::SPIRAM;
                break;
            case Memory_t::PREFER_INTERNAL:
                buffer = (uint8_t*)heap_caps_malloc(line_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
                if (buffer) {
                    actual = Memory_t::INTERNAL;
                } else {
                    buffer = (uint8_t*)heap_caps_malloc(line_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
                    actual = Memory_t::SPIRAM;
                }
                break;
            case Memory_t::PREFER_SPIRAM:
                buffer = (uint8_t*)heap_caps_malloc(line_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
                if (buffer) {
                    actual = Memory_t::SPIRAM;
                } else {
                    buffer = (uint8_t*)heap_caps_malloc(line_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
                    actual = Memory_t::INTERNAL;
                }
                break;

            default:
                break;
        }
        return buffer;
    }

    VideoMode_t video_mode;

    public:
    uint8_t** lines = nullptr;
    Memory_t memory_type = Memory_t::ALL_FAILED;
    uint32_t total_buffer_size = 0;

    Memory_t init(VideoResolution_t resolution, SubSampling_t subsampling, Memory_t memory_type) {
        video_mode = _video_modes[static_cast<uint8_t>(resolution)];
        uint32_t line_bytes = video_mode.width * SUBSAMPLING_BYTES(subsampling);
        total_buffer_size = video_mode.height * video_mode.width * sizeof(uint8_t*);
        video_mode.color_mode = subsampling;

        // Pointer array always stays in internal RAM
        lines = (uint8_t**)heap_caps_malloc(video_mode.height * sizeof(uint8_t*), MALLOC_CAP_INTERNAL);
        if (!lines) {
            memory_type = Memory_t::ALL_FAILED;
            return Memory_t::ALL_FAILED;
        }

        for (uint32_t y = 0; y < video_mode.height; y++) {
            lines[y] = allocate_line_buffer(line_bytes, memory_type, memory_type);
            if (!lines[y]) {
                for (uint32_t x = 0; x < y; x++) heap_caps_free(lines[x]);
                heap_caps_free(lines);
                lines = nullptr;
                memory_type = Memory_t::ALL_FAILED;
                return Memory_t::ALL_FAILED;
            }
        }
        return memory_type;
    }


    void pushFromBuffer(CompositeVideoBuffer& buffer){
        VideoMode_t source_mode = buffer.getVideoMode();

        uint32_t source_bytes = source_mode.width * SUBSAMPLING_BYTES(source_mode.color_mode);
        uint32_t current_bytes = source_mode.width * SUBSAMPLING_BYTES(video_mode.color_mode);
        if (source_bytes != current_bytes) return;

        for (uint32_t y = 0; y < source_mode.height; y++){
            memcpy(lines[y], buffer.lines[y], source_bytes);
        }
    }


    VideoMode_t getVideoMode(){
        return video_mode;
    }

    void deinit(){
        if (lines) {
            for (uint32_t y = 0; y < video_mode.height; y++)
                if (lines[y]) heap_caps_free(lines[y]);
            heap_caps_free(lines);
            lines = nullptr;
        }
    }
    ~CompositeVideoBuffer(){
        deinit();
    }
};

namespace CompositeVideoBlitter {


VideoMode_t _video_mode;
CompositeVideoBuffer* frame_buffer = nullptr;
uint16_t y_lut[256];
uint16_t uv_lut[256];

    inline void double_dot422(int x, int y, CompositeColor c0, CompositeColor c1)
    {
        uint8_t* line = frame_buffer->lines[y];
        size_t index = (x & ~1) * 2;

        line[index + 0] = c0.y;
        line[index + 1] = c1.y;
        line[index + 2] = (c0.u + c1.u) >> 1;
        line[index + 3] = (c0.v + c1.v) >> 1;
    }

inline void set_pixel(uint16_t x, uint16_t y, CompositeColor color)
{
	uint8_t* line = frame_buffer->lines[y];
    switch (_video_mode.color_mode){
    case SubSampling_t::SUBSAMPLING_444:{
		size_t index = x * 3;

		line[index + 0] = color.y;
		line[index + 1] = color.u;
		line[index + 2] = color.v;
        break;
    }
    case SubSampling_t::SUBSAMPLING_422:{
		uint16_t evenX = x & ~1;
		size_t block_index = evenX * 2;

		line[block_index + (x & 1)] = color.y;
		line[block_index + 2] = color.u;
		line[block_index + 3] = color.v;
        break;
    }
    case SubSampling_t::SUBSAMPLING_400:
        line[x] = color.y;
        break;
    default:
        break;
    }
}

void frame_clear(CompositeColor color)
{
	for (uint32_t y = 0; y < _video_mode.height; y++) {
        for (uint32_t x = 0; x < _video_mode.width; x++){set_pixel(x,y,color);}
	}
}
/**
 * Creates a framebuffer. The caps argument can be one of the following:
 *
 *   MALLOC_CAP_8BIT   - Accessible in single bytes
 *   MALLOC_CAP_32BIT  - All access must be 32-bit aligned
 *
 * The latter allows the ESP32 to store the data in IRAM (instruction RAM),
 * but makes access to individual bytes more difficult.
 */


//====================================================================================================
//====================================================================================================
//
// low level HW setup of DAC/DMA/APLL/PWM
//

lldesc_t _dma_desc[4] = {0};
intr_handle_t _isr_handle;

extern "C"
void IRAM_ATTR video_isr(volatile void* buf);

// simple isr
static void IRAM_ATTR i2s_intr_handler_video(void *arg) {
    if (I2S0.int_st.out_eof)
        video_isr(((lldesc_t*)I2S0.out_eof_des_addr)->buf); // get the next line of video
    I2S0.int_clr.val = I2S0.int_st.val;                     // reset the interrupt
}

static esp_err_t start_dma(int line_width,int samples_per_cc, int ch = 1)
{
    periph_module_enable(PERIPH_I2S0_MODULE);

    // setup interrupt
    if (esp_intr_alloc(ETS_I2S0_INTR_SOURCE, ESP_INTR_FLAG_LEVEL1 | ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_INTRDISABLED,
        i2s_intr_handler_video, 0, &_isr_handle) != ESP_OK)
        return -1;

    // reset conf
    I2S0.conf.val = 1;
    I2S0.conf.val = 0;
    I2S0.conf.tx_right_first = 1;
    I2S0.conf.tx_mono = (ch == 2 ? 0 : 1);

    I2S0.conf2.lcd_en = 1;
    I2S0.fifo_conf.tx_fifo_mod_force_en = 1;
    I2S0.sample_rate_conf.tx_bits_mod = 16;
    I2S0.conf_chan.tx_chan_mod = (ch == 2) ? 0 : 1;

    // Create TX DMA buffers
    for (int i = 0; i < 2; i++) {
        int n = line_width*2*ch;
        if (n >= 4092) {
            printf("DMA chunk too big:%s\n",n);
            return -1;
        }
        _dma_desc[i].buf = (uint8_t*)heap_caps_calloc(1, n, MALLOC_CAP_DMA);
        if (!_dma_desc[i].buf)
            return -1;
        
        _dma_desc[i].owner = 1;
        _dma_desc[i].eof = 1;
        _dma_desc[i].length = n;
        _dma_desc[i].size = n;
        _dma_desc[i].empty = (uint32_t)(i == 1 ? _dma_desc : _dma_desc+1);
    }
    I2S0.out_link.addr = (uint32_t)_dma_desc;

    //  Setup up the apll: See ref 3.2.7 Audio PLL
    //  f_xtal = (int)rtc_clk_xtal_freq_get() * 1000000;
    //  f_out = xtal_freq * (4 + sdm2 + sdm1/256 + sdm0/65536); // 250 < f_out < 500
    //  apll_freq = f_out/((o_div + 2) * 2)
    //  operating range of the f_out is 250 MHz ~ 500 MHz
    //  operating range of the apll_freq is 16 ~ 128 MHz.
    //  select sdm0,sdm1,sdm2 to produce nice multiples of colorburst frequencies

    //  see calc_freq() for math: (4+a)*10/((2 + b)*2) mhz
    //  up to 20mhz seems to work ok:
    //  rtc_clk_apll_enable(1,0x00,0x00,0x4,0);   // 20mhz for fancy DDS
    if(_video_mode.is_pal){
        rtc_clk_apll_enable(1,0x04,0xA4,0x6,1);     // 17.734476mhz ~4x PAL
    }
    else{
            switch (samples_per_cc) {
                case 3: rtc_clk_apll_enable(1,0x46,0x97,0x4,2);   break;    // 10.7386363636 3x NTSC (10.7386398315mhz)
                case 4: rtc_clk_apll_enable(1,0x46,0x97,0x4,1);   break;    // 14.3181818182 4x NTSC (14.3181864421mhz)
            }
    }

    I2S0.clkm_conf.clkm_div_num = 1;            // I2S clock divider’s integral value.
    I2S0.clkm_conf.clkm_div_b = 0;              // Fractional clock divider’s numerator value.
    I2S0.clkm_conf.clkm_div_a = 1;              // Fractional clock divider’s denominator value
    I2S0.sample_rate_conf.tx_bck_div_num = 1;
    I2S0.clkm_conf.clka_en = 1;                 // Set this bit to enable clk_apll.
    I2S0.fifo_conf.tx_fifo_mod = (ch == 2) ? 0 : 1; // 32-bit dual or 16-bit single channel data

    dac_output_enable(DAC_CHANNEL_1);           // DAC, video on GPIO25
    dac_i2s_enable();                           // start DAC!

    I2S0.conf.tx_start = 1;                     // start DMA!
    I2S0.int_clr.val = 0xFFFFFFFF;
    I2S0.int_ena.out_eof = 1;
    I2S0.out_link.start = 1;
    return esp_intr_enable(_isr_handle);        // start interruprs!
}

void video_init_hw(int line_width, int samples_per_cc)
{
    // setup apll 4x NTSC or PAL colorburst rate
    start_dma(line_width,samples_per_cc,1);
}

uint32_t cpu_ticks()
{
  return xthal_get_ccount();
}

uint32_t us() {
    return cpu_ticks()/240;
}

// Color clock frequency is 315/88 (3.57954545455)
// DAC_MHZ is 315/11 or 8x color clock
// 455/2 color clocks per line, round up to maintain phase
// HSYNCH period is 44/315*455 or 63.55555..us
// Field period is 262*44/315*455 or 16651.5555us

#define IRE(_x)          ((uint32_t)(((_x)+40)*255/3.3/147.5) << 8)   // 3.3V DAC
#define SYNC_LEVEL       IRE(-40)
#define BLANKING_LEVEL   IRE(0)
#define BLACK_LEVEL      IRE(7.5)
#define GRAY_LEVEL       IRE(50)
#define WHITE_LEVEL      IRE(100)

#define MAX_DAC_1V WHITE_LEVEL // Caps modulation ceiling at ~0.95 V

#define P0 (color >> 16)
#define P1 (color >> 8)
#define P2 (color)
#define P3 (color << 8)

volatile int _line_counter = 0;
volatile int _frame_counter = 0;

int _samples_per_cc = 4; // 3 or 4
float _sample_rate;

int _hsync;
int _line_count;
int _hsync_long;
int _hsync_short;
int _burst_start;
int _burst_width;
int _active_start;

int16_t* _burst0 = 0; // pal bursts
int16_t* _burst1 = 0;

static int usec(float us)
{
    uint32_t r = (uint32_t)(us*_sample_rate);
    return ((r + _samples_per_cc)/(_samples_per_cc << 1))*(_samples_per_cc << 1);  // multiple of color clock, word align
}

#define NTSC_COLOR_CLOCKS_PER_SCANLINE 228       // really 227.5 for NTSC but want to avoid half phase fiddling for now
#define NTSC_FREQUENCY (315000000.0/88)
#define NTSC_LINES 262

#define PAL_COLOR_CLOCKS_PER_SCANLINE 284        // really 283.75 ?
#define PAL_FREQUENCY 4433618.75
#define PAL_LINES 312

void ntsc_init();
void pal_init();

static inline uint16_t IRAM_ATTR clamp_dac(int32_t val) {
	val = (val < BLANKING_LEVEL) ? BLANKING_LEVEL : val;
	return (val > MAX_DAC_1V) ? MAX_DAC_1V : val;
}

static void init_yuv_luts(){
    uint32_t black = BLACK_LEVEL;
    uint32_t white = WHITE_LEVEL;
    int32_t luma_range = (uint32_t)white - (uint32_t)black;
    for(int i = 0; i < 256; i++){
        y_lut[i] = (uint16_t)(black + ((i * luma_range) / 255));
        uv_lut[i] = (int16_t)((((int32_t)i -128) * (luma_range * 25 / 100)) / 128);
    }
}

void setup_video(CompositeVideoBuffer& render_buffer){
    _video_mode = render_buffer.getVideoMode();
    frame_buffer = &render_buffer;
	init_yuv_luts();
    if(_video_mode.is_pal){
        pal_init();
    }
    else{
        ntsc_init();
    }

	frame_clear(CompositeColor());
	video_init_hw(_video_mode.line_width, _samples_per_cc);
}

#define BEGIN_TIMING()
#define END_TIMING()
#define ISR_BEGIN()
#define ISR_END()
void perf(){};

//===================================================================================================
//===================================================================================================
// PAL
    void pal_init()
    {
        _sample_rate = PAL_FREQUENCY*_samples_per_cc/1000000.0;       // DAC rate in mhz
        _video_mode.line_width = PAL_COLOR_CLOCKS_PER_SCANLINE*_samples_per_cc;
        _line_count = PAL_LINES;
        _hsync_short = usec(2);
        _hsync_long = usec(30);
        _hsync = usec(4.7);
        _burst_start = usec(5.6);
        _burst_width = (int)(10*_samples_per_cc + 4) & 0xFFFE;
        _active_start = usec(10.4);

        // make colorburst tables for even and odd lines
        _burst0 = new int16_t[_burst_width];
        _burst1 = new int16_t[_burst_width];
        float phase = 2*M_PI/2;
        for (int i = 0; i < _burst_width; i++)
        {
            _burst0[i] = BLANKING_LEVEL + sin(phase + 3*M_PI/4) * BLANKING_LEVEL/1.5;
            _burst1[i] = BLANKING_LEVEL + sin(phase - 3*M_PI/4) * BLANKING_LEVEL/1.5;
            phase += 2*M_PI/_samples_per_cc;
        }
    }

    void IRAM_ATTR blit_pal(uint8_t* src, uint16_t* dst)
    {
        // i wont use pal by now
        /*
        const bool even = _line_counter & 1;
        const uint32_t* p = even ? _palette : _palette + 256;
        uint32_t c,color;
        uint8_t c0,c1,c2,c3,c4;
        uint8_t y1,y2,y3;

        // pal is 5/4 wider than ntsc to account for pal 288 color clocks per line vs 228 in ntsc
        // so do an ugly stretch on pixels (actually luma) to accomodate -> 384 pixels are now 240 pal color clocks wide

        const int left = 0;
        const int right = 336;
        dst += 40;
        for (int i = left; i < right; i += 4) {
            c = *((uint32_t*)(src+i));

            // make 5 colors out of 4 by interpolating y: 0000 0111 1122 2223 3333
            c0 = c;
            c1 = c >> 8;
            c3 = c >> 16;
            c4 = c >> 24;
            y1 = (((c1 & 0xF) << 1) + ((c0 + c1) & 0x1F) + 2) >> 2;    // (c0 & 0xF)*0.25 + (c1 & 0xF)*0.75;
            y2 = ((c1 + c3 + 1) >> 1) & 0xF;                           // (c1 & 0xF)*0.50 + (c2 & 0xF)*0.50;
            y3 = (((c3 & 0xF) << 1) + ((c3 + c4) & 0x1F) + 2) >> 2;    // (c2 & 0xF)*0.75 + (c3 & 0xF)*0.25;
            c1 = (c1 & 0xF0) + y1;
            c2 = (c1 & 0xF0) + y2;
            c3 = (c3 & 0xF0) + y3;

            color = p[c0];
            dst[0^1] = P0;
            dst[1^1] = P1;
            color = p[c1];
            dst[2^1] = P2;
            dst[3^1] = P3;
            color = p[c2];
            dst[4^1] = P0;
            dst[5^1] = P1;
            color = p[c3];
            dst[6^1] = P2;
            dst[7^1] = P3;
            color = p[c4];
            dst[8^1] = P0;
            dst[9^1] = P1;

            i += 4;
            c = *((uint32_t*)(src+i));
            
            // make 5 colors out of 4 by interpolating y: 0000 0111 1122 2223 3333
            c0 = c;
            c1 = c >> 8;
            c3 = c >> 16;
            c4 = c >> 24;
            y1 = (((c1 & 0xF) << 1) + ((c0 + c1) & 0x1F) + 2) >> 2;    // (c0 & 0xF)*0.25 + (c1 & 0xF)*0.75;
            y2 = ((c1 + c3 + 1) >> 1) & 0xF;                           // (c1 & 0xF)*0.50 + (c2 & 0xF)*0.50;
            y3 = (((c3 & 0xF) << 1) + ((c3 + c4) & 0x1F) + 2) >> 2;    // (c2 & 0xF)*0.75 + (c3 & 0xF)*0.25;
            c1 = (c1 & 0xF0) + y1;
            c2 = (c1 & 0xF0) + y2;
            c3 = (c3 & 0xF0) + y3;

            color = p[c0];
            dst[10^1] = P2;
            dst[11^1] = P3;
            color = p[c1];
            dst[12^1] = P0;
            dst[13^1] = P1;
            color = p[c2];
            dst[14^1] = P2;
            dst[15^1] = P3;
            color = p[c3];
            dst[16^1] = P0;
            dst[17^1] = P1;
            color = p[c4];
            dst[18^1] = P2;
            dst[19^1] = P3;
            dst += 20;
        }
    */
    }

    void IRAM_ATTR burst_pal(uint16_t* line)
    {
        line += _burst_start;
        int16_t* b = (_line_counter & 1) ? _burst0 : _burst1;
        for (int i = 0; i < _burst_width; i += 2) {
            line[i^1] = b[i];
            line[(i+1)^1] = b[i+1];
        }
    }

    // Fancy pal non-interlace
    // http://martin.hinner.info/vga/pal.html
    void IRAM_ATTR pal_sync2(uint16_t* line, int width, int swidth)
    {
        swidth = swidth ? _hsync_long : _hsync_short;
        int i;
        for (i = 0; i < swidth; i++)
            line[i] = SYNC_LEVEL;
        for (; i < width; i++)
            line[i] = BLANKING_LEVEL;
    }

    uint8_t DRAM_ATTR _sync_type[8] = {0,0,0,3,3,2,0,0};
    void IRAM_ATTR pal_sync(uint16_t* line, int i)
    {
        uint8_t t = _sync_type[i-304];
        pal_sync2(line,_video_mode.line_width/2, t & 2);
        pal_sync2(line+_video_mode.line_width/2,_video_mode.line_width/2, t & 1);
    }



//===================================================================================================
//===================================================================================================
// NTSC

    void ntsc_init() {
        _sample_rate = 315.0/88 * _samples_per_cc;   // DAC rate
        _video_mode.line_width = NTSC_COLOR_CLOCKS_PER_SCANLINE*_samples_per_cc;
        _line_count = NTSC_LINES;
        _hsync_long = usec(63.555-4.7);
        _active_start = usec(_samples_per_cc == 4 ? 10 : 10.5);
        _hsync = usec(4.7);
    }

    // draw a line of game in NTSC
    void IRAM_ATTR blit_ntsc(uint8_t* src, uint16_t* dst)
    {   
        switch (_video_mode.color_mode){
        case SubSampling_t::SUBSAMPLING_444:
            for(int i = 0; i < _video_mode.width; i += 2){
                uint8_t y0 = src[0];
                uint8_t u0 = src[1];
                uint8_t v0 = src[2];

                uint8_t y1 = src[3];
                uint8_t u1 = src[4];
                uint8_t v1 = src[5];

                // Average or select a stable chroma vector for the color clock pair 
                // to preserve subcarrier phase integrity while keeping luma independent
                int16_t u_val = uv_lut[(u0 + u1) / 2];
                int16_t v_val = uv_lut[(v0 + v1) / 2];

                dst[0] = clamp_dac(y_lut[y0] - u_val);
                dst[1] = clamp_dac(y_lut[y0] + v_val);
                dst[2] = clamp_dac(y_lut[y1] + u_val);
                dst[3] = clamp_dac(y_lut[y1] - v_val);

                dst += 4;
                src += 6;
            }
            break;
        case SubSampling_t::SUBSAMPLING_422:
            for(int i = 0; i < _video_mode.width; i += 2){
                uint8_t y0 = src[0];
                uint8_t y1 = src[1];
                uint8_t u = src[2];
                uint8_t v = src[3];
                int16_t u_val = uv_lut[u];
                int16_t v_val = uv_lut[v];
                dst[0] = clamp_dac(y_lut[y0] - u_val);
                dst[1] = clamp_dac(y_lut[y0] + v_val);
                dst[2] = clamp_dac(y_lut[y1] + u_val);
                dst[3] = clamp_dac(y_lut[y1] - v_val);

                dst += 4;
                src += 4;
            }
            break;
        case SubSampling_t::SUBSAMPLING_400:
            for(int i = 0; i <  _video_mode.width; i += 2){
                uint16_t ly0 = y_lut[src[0]];
                uint16_t ly1 = y_lut[src[1]];

                dst[0] = ly0;
                dst[1] = ly0;
                dst[2] = ly1;
                dst[3] = ly1;

                dst += 4;
                src += 2;
            }
        break;

        default:
            break;
        }
        END_TIMING();
    }
    
    void IRAM_ATTR burst_ntsc(uint16_t* line)
    {
        int i,phase;
        switch (_samples_per_cc) {
            case 4:
                // 4 samples per color clock
                for (i = _hsync; i < _hsync + (4*10); i += 4) {
                    if(_video_mode.color_mode == SubSampling_t::SUBSAMPLING_444 or _video_mode.color_mode == SubSampling_t::SUBSAMPLING_422){
                        line[i+1] = BLANKING_LEVEL;
                        line[i+0] = BLANKING_LEVEL + BLANKING_LEVEL/2;
                        line[i+3] = BLANKING_LEVEL;
                        line[i+2] = BLANKING_LEVEL - BLANKING_LEVEL/2;
                    }
                    else{
                        line[i+1] = BLANKING_LEVEL;
                        line[i+0] = BLANKING_LEVEL;
                        line[i+3] = BLANKING_LEVEL;
                        line[i+2] = BLANKING_LEVEL;
                    }
                }
                break;
            case 3:
                // 3 samples per color clock
                phase = 0.866025*BLANKING_LEVEL/2;
                for (i = _hsync; i < _hsync + (3*10); i += 6) {
                    line[i+1] = BLANKING_LEVEL;
                    line[i+0] = BLANKING_LEVEL + phase;
                    line[i+3] = BLANKING_LEVEL - phase;
                    line[i+2] = BLANKING_LEVEL;
                    line[i+5] = BLANKING_LEVEL + phase;
                    line[i+4] = BLANKING_LEVEL - phase;
                }
                break;
        }
    }

void IRAM_ATTR blit(uint8_t* src, uint16_t* dst)
{
    BEGIN_TIMING();
    if(_video_mode.is_pal){blit_pal(src,dst);}
    else{blit_ntsc(src,dst);}

    END_TIMING();
}

void IRAM_ATTR burst(uint16_t* line)
{
    if(_video_mode.is_pal){burst_pal(line);}
    else{burst_ntsc(line);}
}

void IRAM_ATTR sync(uint16_t* line, int syncwidth)
{
    for (int i = 0; i < syncwidth; i++)
        line[i] = SYNC_LEVEL;
}

void IRAM_ATTR blanking(uint16_t* line, bool vbl)
{
    int syncwidth = vbl ? _hsync_long : _hsync;
    sync(line,syncwidth);
    for (int i = syncwidth; i < _video_mode.line_width; i++)
        line[i] = BLANKING_LEVEL;
    if (!vbl)
        burst(line);    // no burst during vbl
}

// Wait for blanking before starting drawing
// avoids tearing in our unsynchonized world
#ifdef ESP_PLATFORM
    void video_sync()
    {
      if (!frame_buffer->lines)
        return;
      int n = 0;
      if (_video_mode.is_pal) {
        if (_line_counter < _video_mode.height)
          n = (_video_mode.height - _line_counter)*1000/15600;
      } else {
        if (_line_counter < _video_mode.height)
          n = (_video_mode.height - _line_counter)*1000/15720;
      }
      vTaskDelay(n+1);
    }
#endif

int get_current_scanline()
{
    return _line_counter;
}

// Workhorse ISR handles audio and video updates
extern "C"
void IRAM_ATTR video_isr(volatile void* vbuf)
{
    if (!frame_buffer->lines)
        return;

    ISR_BEGIN();

    int i = _line_counter++;
    uint16_t* buf = (uint16_t*)vbuf;

        if (_video_mode.is_pal) {
            // pal
            if (i < 32) {
                blanking(buf,false);                // pre render/black 0-32
            } else if (i < _video_mode.height + 32) {    // active video 32-272
                sync(buf,_hsync);
                burst(buf);
                blit(frame_buffer->lines[i-32],buf + _active_start);
            } else if (i < 304) {                   // post render/black 272-304
                if (i < 274)                        // slight optimization here, once you have 2 blanking buffers
                    blanking(buf,false);
            } else {
                pal_sync(buf,i);                    // 8 lines of sync 304-312
            }
        }
        else {
            // ntsc
            if (i < _video_mode.height) {                // active video
                sync(buf,_hsync);
                burst(buf);
                blit(frame_buffer->lines[i],buf + _active_start);

            } else if (i < (_video_mode.height + 5)) {   // post render/black
                blanking(buf,false);

            } else if (i < (_video_mode.height + 8)) {   // vsync
                blanking(buf,true);

            } else {                                // pre render/black
                blanking(buf,false);
            }
        }

    if (_line_counter == _line_count) {
        _line_counter = 0;                      // frame is done
        _frame_counter++;
    }

    ISR_END();
}

} // Namespace CompositeVideoBlitter
#endif