#include "CompositeVideo.h"
#include "Font.h"

class CompositeGraphics
{ 
    private:
    
    uint16_t cursorX = 0;
    uint16_t cursorY = 0;
    uint16_t cursorBaseX = 0;
    public:
    uint16_t xres = 0;
    uint16_t yres = 0;
    private:
    
    CompositeColor frontColor;
    CompositeColor backColor;
    bool hasBackColor = 0;
    Font<CompositeGraphics> *font = nullptr;
    CompositeVideoBuffer* frame_buffer = nullptr;
    SubSampling_t subsampling_mode;
    public:


    void set_buffer(CompositeVideoBuffer& render_buffer){
        VideoMode_t video_mode = render_buffer.getVideoMode();
        subsampling_mode = video_mode.color_mode;
        xres = video_mode.width;
        yres = video_mode.height;
        frame_buffer = &render_buffer;
    }

    void setTextColor(CompositeColor front, CompositeColor back = CompositeColor{0, 0, 0})
    {
        frontColor = front;
        backColor = back;
        hasBackColor = (back.y != 0 || back.u != 0 || back.v != 0);
    }

    void setFont(Font<CompositeGraphics> &font)
    {
        this->font = &font;
    }

    void setCursor(int x, int y)
    {
        cursorX = cursorBaseX = x;  
        cursorY = y;  
    }

    void print(const char *str, bool transparent_bg = true)
    {
        if (!font) return;
        while (*str)
        {
            if (*str >= 32 && *str < 128)
                font->drawChar(*this, cursorX, cursorY, *str, frontColor, backColor, transparent_bg);
            cursorX += font->xres;
            if (cursorX + font->xres > xres || *str == '\n')
            {
                cursorX = cursorBaseX;
                cursorY += font->yres;        
            }
            str++;
        }
    }

    void print(int number, int base = 10, int minCharacters = 1,bool transparent_bg = true)
    {
        bool sign = number < 0;
        if (sign) number = -number;
        const char baseChars[] = "0123456789ABCDEF";
        char temp[33];
        temp[32] = 0;
        int i = 31;
        do
        {
            temp[i--] = baseChars[number % base];
            number /= base;
        } while (number > 0);

        if (sign) temp[i--] = '-';
        for (; i > 31 - minCharacters; i--) temp[i] = ' ';
        print(&temp[i + 1],transparent_bg);
    }

    inline void clear(CompositeColor clearColor = CompositeColor{0, 128, 128})
{
	CompositeVideoBlitter::frame_clear(clearColor);

}

    inline void dotFast(int x, int y, CompositeColor color)
{
	
    uint8_t* line = frame_buffer->lines[y];

        switch(subsampling_mode){
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
        case SubSampling_t::SUBSAMPLING_400:{
            line[x] = color.y;
            break;
        }
        default :
            break;
    }
}

    inline void double_dot422(int x, int y, CompositeColor c0, CompositeColor c1)
    {
        CompositeVideoBlitter::double_dot422(x,y,c0,c1);
    }

    inline void dot(int x, int y, CompositeColor color)
    {
        if ((unsigned int)x < (unsigned int)xres && (unsigned int)y < (unsigned int)yres)
            dotFast(x, y, color);
    }

    inline void xLine(int x0, int x1, int y, CompositeColor color)
    {
        if (x0 > x1) { int xb = x0; x0 = x1; x1 = xb; }
        if (x0 < 0) x0 = 0;
        if (x1 > xres) x1 = xres;
        for (int x = x0; x < x1; x++)
            dotFast(x, y, color);
    }

    void triangle(short *v0, short *v1, short *v2, CompositeColor color)
    {
        short *v[3] = {v0, v1, v2};
        if (v[1][1] < v[0][1]) { short *vb = v[0]; v[0] = v[1]; v[1] = vb; }
        if (v[2][1] < v[1][1]) { short *vb = v[1]; v[1] = v[2]; v[2] = vb; }
        if (v[1][1] < v[0][1]) { short *vb = v[0]; v[0] = v[1]; v[1] = vb; }

        int y = v[0][1];
        int xac = v[0][0] << 16;
        int xab = v[0][0] << 16;
        int xbc = v[1][0] << 16;
        int xaci = 0, xabi = 0, xbci = 0;

        if (v[1][1] != v[0][1]) xabi = ((v[1][0] - v[0][0]) << 16) / (v[1][1] - v[0][1]);
        if (v[2][1] != v[0][1]) xaci = ((v[2][0] - v[0][0]) << 16) / (v[2][1] - v[0][1]);
        if (v[2][1] != v[1][1]) xbci = ((v[2][0] - v[1][0]) << 16) / (v[2][1] - v[1][1]);

        for (; y < v[1][1] && y < yres; y++) {
            if (y >= 0) xLine(xab >> 16, xac >> 16, y, color);
            xab += xabi;
            xac += xaci;
        }
        for (; y < v[2][1] && y < yres; y++) {
            if (y >= 0) xLine(xbc >> 16, xac >> 16, y, color);
            xbc += xbci;
            xac += xaci;
        }
    }

    void line(int x1, int y1, int x2, int y2, CompositeColor color)
    {
        int x, y, xe, ye;
        int dx = x2 - x1, dy = y2 - y1;
        int dx1 = labs(dx), dy1 = labs(dy);
        int px = 2 * dy1 - dx1, py = 2 * dx1 - dy1;

        if (dy1 <= dx1) {
            if (dx >= 0) { x = x1; y = y1; xe = x2; }
            else         { x = x2; y = y2; xe = x1; }
            dot(x, y, color);
            for (int i = 0; x < xe; i++) {
                x++;
                if (px < 0) px += 2 * dy1;
                else {
                    if ((dx < 0 && dy < 0) || (dx > 0 && dy > 0)) y++;
                    else y--;
                    px += 2 * (dy1 - dx1);
                }
                dot(x, y, color);
            }
        } else {
            if (dy >= 0) { x = x1; y = y1; ye = y2; }
            else         { x = x2; y = y2; ye = y1; }
            dot(x, y, color);
            for (int i = 0; y < ye; i++) {
                y++;
                if (py <= 0) py += 2 * dx1;
                else {
                    if ((dx < 0 && dy < 0) || (dx > 0 && dy > 0)) x++;
                    else x--;
                    py += 2 * (dx1 - dy1);
                }
                dot(x, y, color);
            }
        }
    }

    void fillRect(int x, int y, int w, int h, CompositeColor color)
    {
        if (x < 0) { w += x; x = 0; }
        if (y < 0) { h += y; y = 0; }
        if (x + w > xres) w = xres - x;
        if (y + h > yres) h = yres - y;

        for (int j = y; j < y + h; j++)
            for (int i = x; i < x + w; i++)
                dotFast(i, j, color);
    }

    void rect(int x, int y, int w, int h, CompositeColor color)
    {
        fillRect(x, y, w, 1, color);
        fillRect(x, y, 1, h, color);
        fillRect(x, y + h - 1, w, 1, color);
        fillRect(x + w - 1, y, 1, h, color);
    }
void fillRectGradient(int x, int y, int w, int h,
	CompositeColor color1, CompositeColor color2, bool horizontal = true)
{
	if (w <= 0 || h <= 0) return;

	int start = horizontal ? x : y;
	int length = horizontal ? w : h;

	int x0 = x;
	int y0 = y;
	int x1 = x + w;
	int y1 = y + h;

	if (x0 < 0) x0 = 0;
	if (y0 < 0) y0 = 0;
	if (x1 > xres) x1 = xres;
	if (y1 > yres) y1 = yres;

	if (x0 >= x1 || y0 >= y1) return;

	int dy = (int)color2.y - color1.y;
	int du = (int)color2.u - color1.u;
	int dv = (int)color2.v - color1.v;

	for (int j = y0; j < y1; j++) {
		for (int i = x0; i < x1; i++) {
			int pos = (horizontal ? i : j) - start;

			CompositeColor c = {
				(uint8_t)((int)color1.y + dy * pos / (length > 1 ? length - 1 : 1)),
				(uint8_t)((int)color1.u + du * pos / (length > 1 ? length - 1 : 1)),
				(uint8_t)((int)color1.v + dv * pos / (length > 1 ? length - 1 : 1))
			};

			dotFast(i, j, c);
		}
	}
}
};