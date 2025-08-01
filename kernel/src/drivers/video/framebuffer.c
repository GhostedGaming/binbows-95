#include <framebuffer.h>
#include <limine.h>
#include <stddef.h>
#include <serial.h>
#include <stdint.h>
#include <font.h>
#include <util.h>
#include <stdbool.h>

extern volatile struct limine_framebuffer_request framebuffer_request;

int fb_width = 0;
int fb_height = 0;
int fb_pitch = 0;
uint32_t *fb_ptr = NULL;

uint32_t rgb_to_color(uint8_t r, uint8_t g, uint8_t b) {
    return (0xFF << 24) | (r << 16) | (g << 8) | b;
}

void clear_screen() {
    uint64_t pixels = (uint64_t)fb_width * fb_height;
    for (uint64_t i = 0; i < pixels; i++) {
        fb_ptr[i] = 0x00000000;
    }
}

void init_fb() {
    while (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count == 0);

    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];
    
    fb_width = (int)framebuffer->width;
    fb_height = (int)framebuffer->height;
    fb_pitch = (int)(framebuffer->pitch / 4);

    fb_ptr = (uint32_t *)framebuffer->address;

    // Clear screen to black
    uint64_t pixels = (uint64_t)fb_width * fb_height;
    for (uint64_t i = 0; i < pixels; i++) {
        fb_ptr[i] = 0x00000000;
    }
}

void draw_char(int x, int y, char c, uint32_t color) {
    if (!is_printable_char(c)) return;

    const uint8_t *char_bitmap = font8x16[(int)c - 32];

    for (int row = 0; row < FONT_HEIGHT; row++) {
        uint8_t byte = char_bitmap[row];
        for (int col = 0; col < FONT_WIDTH; col++) {
            if (byte & (0x80 >> col)) {
                int px = x + col;
                int py = y + row;
                if (px >= 0 && px < fb_width && py >= 0 && py < fb_height) {
                    fb_ptr[py * fb_pitch + px] = color;
                }
            }
        }
    }
}

void draw_text(int x, int y, const char *str, uint32_t color, bool clear) {
    if (clear == true) {
        clear_screen();
    }

    if (!str) return;

    int len = strlen(str);
    if (x < 0) x = (fb_width - len * FONT_WIDTH) / 2;
    if (y < 0) y = (fb_height - FONT_HEIGHT) / 2;

    while (*str) {
        draw_char(x, y, *str, color);
        x += FONT_WIDTH;
        str++;
    }
}