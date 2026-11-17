/*
 * Minimal text console on the ABL splash framebuffer. With the MMU off
 * every access is Device memory, so stay aligned (-mstrict-align).
 * On SDM670 (bonito), panels are MIPI DSI Command Mode, so we also trigger
 * SDE CTL_START (0x0ae0201c) after drawing to push the updated frame.
 */
#include "console.h"
#include "font.h"

#define SCALE           3
#define CH_W            (8 * SCALE)
#define CH_H            (16 * SCALE)
#define MARGIN          24
#define LABEL_COLS      8

#define SDE_CTL0_START  0x0ae0201cUL

volatile uint32_t *con_fb = (volatile uint32_t *)FB_BASE;
static uint32_t con_bg;
static int row;

static void con_kick_dsi(void)
{
#ifdef __aarch64__
        __asm__ volatile("dsb sy" ::: "memory");
        *(volatile uint32_t *)SDE_CTL0_START = 1;
        __asm__ volatile("dsb sy" ::: "memory");
        uint64_t freq, start, now;
        __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(freq));
        __asm__ volatile("mrs %0, cntpct_el0" : "=r"(start));
        uint64_t wait = freq / 50; /* 20 ms for 60Hz command-mode frame transfer */
        do {
                __asm__ volatile("mrs %0, cntpct_el0" : "=r"(now));
        } while ((now - start) < wait);
#endif
}

static inline void put_px(int x, int y, uint32_t c)
{
#if FB_BPP == 3
        volatile uint8_t *p = (volatile uint8_t *)con_fb + y * FB_STRIDE + x * 3;
        p[0] = (uint8_t)c;
        p[1] = (uint8_t)(c >> 8);
        p[2] = (uint8_t)(c >> 16);
#else
        con_fb[y * (FB_STRIDE / 4) + x] = c;
#endif
}

void con_clear(uint32_t bg)
{
        for (int y = 0; y < FB_H; y++)
                for (int x = 0; x < FB_W; x++)
                        put_px(x, y, bg);
        con_bg = bg;
        row = 0;
        con_kick_dsi();
}

static void put_char(int col, char ch, uint32_t fg)
{
        if (ch < 0x20 || ch > 0x7e)
                ch = '?';
        const unsigned char *g = font8x16[ch - 0x20];
        int x0 = MARGIN + col * CH_W, y0 = MARGIN + row * CH_H;
        if (x0 + CH_W > FB_W || y0 + CH_H > FB_H)
                return;
        for (int gy = 0; gy < 16; gy++)
                for (int gx = 0; gx < 8; gx++) {
                        uint32_t c = (g[gy] & (0x80 >> gx)) ? fg : con_bg;
                        for (int sy = 0; sy < SCALE; sy++)
                                for (int sx = 0; sx < SCALE; sx++)
                                        put_px(x0 + gx * SCALE + sx, y0 + gy * SCALE + sy, c);
                }
}

static int put_str(int col, const char *s, uint32_t fg)
{
        while (*s)
                put_char(col++, *s++, fg);
        return col;
}

static void fmt_hex(char *buf, uint64_t v, int digits)
{
        static const char hex[] = "0123456789ABCDEF";
        for (int i = 0; i < digits; i++)
                buf[i] = hex[(v >> (4 * (digits - 1 - i))) & 0xf];
        buf[digits] = 0;
}

void con_skip(void)
{
        row++;
}

void con_str(const char *label, const char *s)
{
        put_str(0, label, YELLOW);
        put_str(LABEL_COLS, s, WHITE);
        row++;
        con_kick_dsi();
}

void con_hex(const char *label, uint64_t v)
{
        char buf[17];
        fmt_hex(buf, v, 16);
        con_str(label, buf);
}

void con_hex2(const char *label, const char *a, uint32_t va, const char *b, uint32_t vb)
{
        char buf[9];
        int col = put_str(0, label, YELLOW);
        col = col < LABEL_COLS ? LABEL_COLS : col + 1;
        col = put_str(col, a, YELLOW);
        fmt_hex(buf, va, 8);
        col = put_str(col, buf, WHITE);
        if (b) {
                col = put_str(col + 1, b, YELLOW);
                fmt_hex(buf, vb, 8);
                put_str(col, buf, WHITE);
        }
        row++;
        con_kick_dsi();
}
