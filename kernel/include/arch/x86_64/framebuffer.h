#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>
#include <stdbool.h>

// Framebuffer global info
extern int fb_width;
extern int fb_height;
extern int fb_pitch;
extern uint32_t *fb_ptr;

// Color utility
uint32_t rgb_to_color(uint8_t r, uint8_t g, uint8_t b);

// Framebuffer utilities
void clear_screen(void);
void init_fb(void);

// Text rendering
void draw_char(int x, int y, char c, uint32_t color);
void draw_text(int x, int y, const char *str, uint32_t color, bool clear);

#endif // FRAMEBUFFER_H