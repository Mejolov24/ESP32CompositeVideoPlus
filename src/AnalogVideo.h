
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
 * Oct 2021 - Original code from https://github.com/rossumur/esp_8_bit
 *            Modified by Marcio Teixeira to extract Atari framebuffer
 *            and make it compatible with Bitluni's code. I don't
 *            really understand this code very well, but there are
 *            some implementation details in the esp_8_bit repo.
 * 
 */
#ifndef ANALOG_VIDEO_H
    #define ANALOG_VIDEO_H
#include <Arduino.h>
#include "esp_types.h"
#include "esp_heap_caps.h"
#include "esp_attr.h"
#include "esp_intr_alloc.h"
#include "esp_err.h"
#include "soc/gpio_reg.h"
#include "soc/rtc.h"
#include "soc/soc.h"
#include "soc/i2s_struct.h"
#include "soc/i2s_reg.h"
#include "soc/ledc_struct.h"
#include "soc/rtc_io_reg.h"
#include "soc/io_mux_reg.h"
#include "rom/gpio.h"
#include "rom/lldesc.h"
#include "driver/periph_ctrl.h"
#include "driver/dac.h"
#include "driver/gpio.h"
#include "driver/i2s.h"
#include "Color.h"

#if !defined(SUPPORT_NTSC) && !defined(SUPPORT_PAL)
    #define SUPPORT_NTSC 1
    #define SUPPORT_PAL 1
#endif

#define SUBSAMPLING_444 0
#define SUBSAMPLING_422 1
#define SUBSAMPLING_400 2
typedef int subsampling_t;

#define SUBSAMPLING_BYTES(sub) ( \
    (sub) == SUBSAMPLING_444 ? 3 : \
    (sub) == SUBSAMPLING_422 ? 2 : 1)

#define VIDEO_RES_NTSC_256x240P 0
#define VIDEO_RES_NTSC_320x240P 1
#define VIDEO_RES_NTSC_336x240P 2
#define VIDEO_RES_PAL_256x288P  3
#define VIDEO_RES_PAL_320x288P  4
#define VIDEO_RES_PAL_352x288P  5
#define VIDEO_RES_PAL_384x288P  6
#define VIDEO_RES_COUNT         7

typedef int video_res_t;

typedef struct {
    video_res_t mode;
    uint16_t width;
    uint16_t height;
    float refresh_rate;
    bool is_pal;
    uint16_t total_scanlines; // 262 for NTSC, 312 for PAL
} video_config_t;

constexpr video_config_t VIDEO_CONFIGS[VIDEO_RES_COUNT] = {
    // NTSC 240p Modes (262 total scanlines)
    [VIDEO_RES_NTSC_256x240P] = { VIDEO_RES_NTSC_256x240P, 256, 240, 59.94f, false, 262 },
    [VIDEO_RES_NTSC_320x240P] = { VIDEO_RES_NTSC_320x240P, 320, 240, 59.94f, false, 262 },
    [VIDEO_RES_NTSC_336x240P] = { VIDEO_RES_NTSC_336x240P, 336, 240, 59.94f, false, 262 },
    // PAL 288p Modes (312 total scanlines)
    [VIDEO_RES_PAL_256x288P]  = { VIDEO_RES_PAL_256x288P,  256, 288, 50.00f, true,  312 },
    [VIDEO_RES_PAL_320x288P]  = { VIDEO_RES_PAL_320x288P,  320, 288, 50.00f, true,  312 },
    [VIDEO_RES_PAL_352x288P]  = { VIDEO_RES_PAL_352x288P,  352, 288, 50.00f, true,  312 },
    [VIDEO_RES_PAL_384x288P]  = { VIDEO_RES_PAL_384x288P,  384, 288, 50.00f, true,  312 }

};

#ifndef VIDEO_PIN
    #define VIDEO_PIN   26
#endif
#ifndef VIDEO_RES
    #define VIDEO_RES VIDEO_RES_NTSC_336x240P
#endif
#ifndef VIDEO_SUBSAMPLING
    #define VIDEO_SUBSAMPLING SUBSAMPLING_422
#endif
#define CURRENT_RES         VIDEO_CONFIGS[VIDEO_RES]
#define VIDEO_LINE_WIDTH    (CURRENT_RES.width)
#define VIDEO_LINE_COUNT    (CURRENT_RES.height)
#define VIDEO_BPP           SUBSAMPLING_BYTES(VIDEO_SUBSAMPLING)
#define VIDEO_BUFFER_BYTES  (VIDEO_LINE_WIDTH * VIDEO_LINE_COUNT * VIDEO_BPP)
#define VIDEO_BUFFER_SIZE   ((VIDEO_BUFFER_BYTES + 3) / 4)

#define COLORBURST 1

namespace RawCompositeVideoBlitter {

int _pal_ = 0;

#define FB_CHUNK_LINES 16
#define FB_CHUNKS ((VIDEO_LINE_COUNT + FB_CHUNK_LINES - 1) / FB_CHUNK_LINES)

inline static uint8_t* _lines[VIDEO_LINE_COUNT] = {};

uint16_t y_lut[256];
uint16_t uv_lut[256];

inline void set_pixel(uint16_t x, uint16_t y, Color color)
{
	uint8_t* line = _lines[y];

	#if VIDEO_SUBSAMPLING == SUBSAMPLING_422

		uint16_t evenX = x & ~1;
		size_t block_index = evenX * 2;

		line[block_index + (x & 1)] = color.y;
		line[block_index + 2] = color.u;
		line[block_index + 3] = color.v;

	#elif VIDEO_SUBSAMPLING == SUBSAMPLING_400

		line[x] = color.y;

	#elif VIDEO_SUBSAMPLING == SUBSAMPLING_444

		size_t index = x * 3;

		line[index + 0] = color.y;
		line[index + 1] = color.u;
		line[index + 2] = color.v;

	#endif
}

void frame_clear(Color color)
{
	for (int y = 0; y < VIDEO_LINE_COUNT; y++) {

		uint8_t* line = _lines[y];

		#if VIDEO_SUBSAMPLING == SUBSAMPLING_422

			for (int x = 0; x < VIDEO_LINE_WIDTH; x += 2) {
				line[0] = color.y;
				line[1] = color.y;
				line[2] = color.u;
				line[3] = color.v;

				line += 4;
			}

		#elif VIDEO_SUBSAMPLING == SUBSAMPLING_400

			memset(line, color.y, VIDEO_LINE_WIDTH);

		#elif VIDEO_SUBSAMPLING == SUBSAMPLING_444

			for (int x = 0; x < VIDEO_LINE_WIDTH; x++) {
				line[0] = color.y;
				line[1] = color.u;
				line[2] = color.v;

				line += 3;
			}

		#endif
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

    #if SUPPORT_NTSC
        if (!_pal_) {
            switch (samples_per_cc) {
                case 3: rtc_clk_apll_enable(1,0x46,0x97,0x4,2);   break;    // 10.7386363636 3x NTSC (10.7386398315mhz)
                case 4: rtc_clk_apll_enable(1,0x46,0x97,0x4,1);   break;    // 14.3181818182 4x NTSC (14.3181864421mhz)
            }
        }
    #endif
    #if SUPPORT_PAL
        if (_pal_) {
            rtc_clk_apll_enable(1,0x04,0xA4,0x6,1);     // 17.734476mhz ~4x PAL
        }
    #endif

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

int _active_lines;
int _line_count;

int _line_width;
int _samples_per_cc = 4; // 3 or 4
float _sample_rate;

int _hsync;
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

inline uint16_t IRAM_ATTR clamp_dac(int32_t val) {
    if (val < (int32_t)BLANKING_LEVEL) return (uint16_t)BLANKING_LEVEL;
    if (val > (int32_t)MAX_DAC_1V)     return (uint16_t)MAX_DAC_1V;
    return (uint16_t)val;
}

static void init_yuv_luts(){
    uint32_t black = BLACK_LEVEL;
    uint32_t white = WHITE_LEVEL;
    int32_t luma_range = (uint32_t)white - (uint32_t)black;
    for(int i = 0; i < 256; i++){
        y_lut[i] = (uint16_t)(black + ((i * luma_range) / 255));
        uv_lut[i] = (int16_t)((((int32_t)i -128) * (luma_range * 25 / 100)) / 128);
    }
}void video_init()
{
	video_config_t config = CURRENT_RES;
	_pal_ = config.is_pal ? 1 : 0;
	init_yuv_luts();

	size_t line_bytes = VIDEO_LINE_WIDTH * VIDEO_BPP;

	Serial.printf("Framebuffer: %u bytes total\n", VIDEO_BUFFER_BYTES);
	Serial.printf("Line: %u bytes\n", line_bytes);

	for (int y = 0; y < VIDEO_LINE_COUNT; y++) {

		_lines[y] = (uint8_t*)heap_caps_malloc(
			line_bytes,
			MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT
		);

		if (!_lines[y]) {
			Serial.printf(
				"ERROR: Failed to allocate framebuffer line %d (%u bytes)\n",
				y,
				line_bytes
			);

			while (1) {
				delay(1000);
			}
		}
	}

	#if SUPPORT_PAL
		if (_pal_)
			pal_init();
	#endif

	#if SUPPORT_NTSC
		if (!_pal_)
			ntsc_init();
	#endif

	frame_clear(Color());

	_active_lines = config.height;
	video_init_hw(_line_width, _samples_per_cc);
}

#define BEGIN_TIMING()
#define END_TIMING()
#define ISR_BEGIN()
#define ISR_END()
void perf(){};

//===================================================================================================
//===================================================================================================
// PAL

#if SUPPORT_PAL
    void pal_init()
    {
        _sample_rate = PAL_FREQUENCY*_samples_per_cc/1000000.0;       // DAC rate in mhz
        _line_width = PAL_COLOR_CLOCKS_PER_SCANLINE*_samples_per_cc;
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
        pal_sync2(line,_line_width/2, t & 2);
        pal_sync2(line+_line_width/2,_line_width/2, t & 1);
    }

#endif

//===================================================================================================
//===================================================================================================
// ntsc tables
// AA AA                // 2 pixels, 1 color clock - atari
// AA AB BB             // 3 pixels, 2 color clocks - nes
// AAA ABB BBC CCC      // 4 pixels, 3 color clocks - sms

// cc == 3 gives 684 samples per line, 3 samples per cc, 3 pixels for 2 cc
// cc == 4 gives 912 samples per line, 4 samples per cc, 2 pixels per cc

#if SUPPORT_NTSC
    void ntsc_init() {
        _sample_rate = 315.0/88 * _samples_per_cc;   // DAC rate
        _line_width = NTSC_COLOR_CLOCKS_PER_SCANLINE*_samples_per_cc;
        _line_count = NTSC_LINES;
        _hsync_long = usec(63.555-4.7);
        _active_start = usec(_samples_per_cc == 4 ? 10 : 10.5);
        _hsync = usec(4.7);
    }

    // draw a line of game in NTSC
    void IRAM_ATTR blit_ntsc(uint8_t* src, uint16_t* dst)
    {
        #if VIDEO_SUBSAMPLING == SUBSAMPLING_422
        for(int i = 0; i < VIDEO_LINE_WIDTH; i += 2){
            uint8_t y0 = src[0];
            uint8_t y1 = src[1];
            uint8_t u = src[2];
            uint8_t v = src[3];
            int16_t u_val = uv_lut[u];
            int16_t v_val = uv_lut[v];
            dst[0^1] = clamp_dac(y_lut[y0] + u_val);
            dst[1^1] = clamp_dac(y_lut[y0] + v_val);
            dst[2^1] = clamp_dac(y_lut[y1] - u_val);
            dst[3^1] = clamp_dac(y_lut[y1] - v_val);

            dst += 4;
            src += 4;
        }
        #elif VIDEO_SUBSAMPLING == SUBSAMPLING_444
        for(int i = 0; i < VIDEO_LINE_WIDTH; i += 2){
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

            dst[0^1] = clamp_dac(y_lut[y0] + u_val);
            dst[1^1] = clamp_dac(y_lut[y0] + v_val);
            dst[2^1] = clamp_dac(y_lut[y1] - u_val);
            dst[3^1] = clamp_dac(y_lut[y1] - v_val);

            dst += 4;
            src += 6;
        }
        #elif VIDEO_SUBSAMPLING == SUBSAMPLING_400
        for(int i = 0; i < VIDEO_LINE_WIDTH; i += 2){
            uint16_t ly0 = y_lut[src[0]];
            uint16_t ly1 = y_lut[src[1]];

            dst[0^1] = ly0;
            dst[1^1] = ly0;
            dst[2^1] = ly1;
            dst[3^1] = ly1;

            dst += 4;
            src += 2;
        }
        #endif
        END_TIMING();
    }
    
    void IRAM_ATTR burst_ntsc(uint16_t* line)
    {
        int i,phase;
        switch (_samples_per_cc) {
            case 4:
                // 4 samples per color clock
                for (i = _hsync; i < _hsync + (4*10); i += 4) {
                    #if COLORBURST
                        line[i+1] = BLANKING_LEVEL;
                        line[i+0] = BLANKING_LEVEL + BLANKING_LEVEL/2;
                        line[i+3] = BLANKING_LEVEL;
                        line[i+2] = BLANKING_LEVEL - BLANKING_LEVEL/2;
                    #else
                        line[i+1] = BLANKING_LEVEL;
                        line[i+0] = BLANKING_LEVEL;
                        line[i+3] = BLANKING_LEVEL;
                        line[i+2] = BLANKING_LEVEL;
                    #endif
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
#endif

void IRAM_ATTR blit(uint8_t* src, uint16_t* dst)
{
    BEGIN_TIMING();
    #if SUPPORT_PAL
    if (_pal_) blit_pal(src,dst);
    #endif
    #if SUPPORT_NTSC
    if (!_pal_) blit_ntsc(src,dst);
    #endif
    END_TIMING();
}

void IRAM_ATTR burst(uint16_t* line)
{
    #if SUPPORT_PAL
        if (_pal_) burst_pal(line);
    #endif
    #if SUPPORT_NTSC
        if(!_pal_) burst_ntsc(line);
    #endif
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
    for (int i = syncwidth; i < _line_width; i++)
        line[i] = BLANKING_LEVEL;
    if (!vbl)
        burst(line);    // no burst during vbl
}

// Wait for blanking before starting drawing
// avoids tearing in our unsynchonized world
#ifdef ESP_PLATFORM
    void video_sync()
    {
      if (!_lines)
        return;
      int n = 0;
      if (_pal_) {
        if (_line_counter < _active_lines)
          n = (_active_lines - _line_counter)*1000/15600;
      } else {
        if (_line_counter < _active_lines)
          n = (_active_lines - _line_counter)*1000/15720;
      }
      vTaskDelay(n+1);
    }
#endif

#ifdef ESP_PLATFORM
void wait_for_vblank()
{
	while (_line_counter < _active_lines)
		taskYIELD();

	while (_line_counter >= _active_lines)
		taskYIELD();
}
#endif

// Workhorse ISR handles audio and video updates
extern "C"
void IRAM_ATTR video_isr(volatile void* vbuf)
{
    if (!_lines)
        return;

    ISR_BEGIN();

    #if SUPPORT_AUDIO
        uint8_t s = _audio_r < _audio_w ? _audio_buffer[_audio_r++ & (sizeof(_audio_buffer)-1)] : 0x20;
        audio_sample(s);
        //audio_sample(_sin64[_x++ & 0x3F]);
    #endif

    #ifdef IR_PIN
        ir_sample();
    #endif

    int i = _line_counter++;
    uint16_t* buf = (uint16_t*)vbuf;
    #if SUPPORT_PAL
        if (_pal_) {
            // pal
            if (i < 32) {
                blanking(buf,false);                // pre render/black 0-32
            } else if (i < _active_lines + 32) {    // active video 32-272
                sync(buf,_hsync);
                burst(buf);
                blit(_lines[i-32],buf + _active_start);
            } else if (i < 304) {                   // post render/black 272-304
                if (i < 274)                        // slight optimization here, once you have 2 blanking buffers
                    blanking(buf,false);
            } else {
                pal_sync(buf,i);                    // 8 lines of sync 304-312
            }
        }
    #endif
    #if SUPPORT_NTSC
        if(!_pal_) {
            // ntsc
            if (i < _active_lines) {                // active video
                sync(buf,_hsync);
                burst(buf);
                blit(_lines[i],buf + _active_start);

            } else if (i < (_active_lines + 5)) {   // post render/black
                blanking(buf,false);

            } else if (i < (_active_lines + 8)) {   // vsync
                blanking(buf,true);

            } else {                                // pre render/black
                blanking(buf,false);
            }
        }
    #endif

    if (_line_counter == _line_count) {
        _line_counter = 0;                      // frame is done
        _frame_counter++;
    }

    ISR_END();
}

//===================================================================================================
//===================================================================================================
// sound routines from "src/gui.cpp" and "src/emu.cpp"

#if SUPPORT_AUDIO
    //  audio is buffered as 6 bit unsigned samples
    uint8_t _audio_buffer[1024];
    uint32_t _audio_r = 0;
    uint32_t _audio_w = 0;
    void audio_write_16(const int16_t* s, int len, int channels)
    {
        int b;
        while (len--) {
            if (_audio_w == (_audio_r + sizeof(_audio_buffer)))
                break;
            if (channels == 2) {
                b = (s[0] + s[1]) >> 9;
                s += 2;
            } else
                b = *s++ >> 8;
            if (b < -32) b = -32;
            if (b > 31) b = 31;
            _audio_buffer[_audio_w++ & (sizeof(_audio_buffer)-1)] = b + 32;
        }
    }
#endif

#if 0
    Emu::Emu(const char* n,int w,int h, int st, int aformat, int cc, int f) :
        name(n),width(w),height(h),standard(st),audio_format(aformat),cc_width(cc),flavor(f)
    {
        //audio_frequency = 15625; // requires fixed point sampler
        audio_frequency = standard == 1 ? 15720 : 15600;
        audio_frame_samples = standard ? (audio_frequency << 16)/60 : (audio_frequency << 16)/50;   // fixed point sampler
        audio_fraction = 0;
    }

    int audio_format = (16 | (1 << 8));

    // soft click wave soundy thing
    const uint16_t _wav[16] =  {
        0x0000,0x187D,0x2D41,0x3B20,0x3FFF,0x3B20,0x2D41,0x187D,
        0x0000,0xE783,0xD2BF,0xC4E0,0xC001,0xC4E0,0xD2BF,0xE783
    };

    void update_audio()
    {
        int16_t abuffer[313*2];
        int format = _emu->audio_format >> 8;
        int sample_count = _emu->frame_sample_count();
        if (_visible) {
            format = 1;
            if (_click) {
                _click = 0;
                for (int i = 0; i < sample_count; i++)
                    abuffer[i] = _wav[i&0xF];  // just a signed sine click
            } else
              memset(abuffer,0,sizeof(abuffer));
        } else {
            sample_count = _emu->audio_buffer(abuffer,sizeof(abuffer));
        }
        audio_write_16(abuffer,sample_count,format);
    }
#endif

} // Namespace RawCompositeVideoBlitter
#endif