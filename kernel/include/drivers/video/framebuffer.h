#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>
#include <stdbool.h>

#define FONT_WIDTH 8
#define FONT_HEIGHT 16

extern int fb_width;
extern int fb_height;
extern int fb_pitch;
extern uint32_t *fb_ptr;

extern uint16_t cursor_position_x;
extern uint16_t cursor_position_y;

void init_fb(void);

uint32_t rgb_to_color(uint8_t r, uint8_t g, uint8_t b);
void clear_screen(void);
void draw_char(int x, int y, char c, uint32_t color);
void draw_rect(int x, int y, int width, int height, uint32_t color);

void fb_print(const char *str, uint32_t color);
void fb_printf(const char *format, ...);

void move_cursor_to(uint16_t x, uint16_t y);
uint16_t get_cursor_x(void);
uint16_t get_cursor_y(void);

int get_screen_width(void);
int get_screen_height(void);

#endif