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

uint32_t rgb_to_color(uint8_t r, uint8_t g, uint8_t b);

void clear_screen(void);
void fill_screen(uint32_t color);
void init_fb(void);

void draw_char(int x, int y, char c, uint32_t color);
void draw_text(int x, int y, const char *str, uint32_t color, bool clear);
void draw_text_centered(int y, const char *str, uint32_t color);
void draw_text_center_screen(const char *str, uint32_t color);
void draw_rect(int x, int y, int width, int height, uint32_t color);
void scroll_screen(void);
void print_text(const char *str, uint32_t color);

void set_cursor_pos(uint16_t x, uint16_t y);
void move_cursor_by_chars(int chars);
void move_cursor_by_lines(int lines);
void cursor_newline(void);
uint16_t get_cursor_x(void);
uint16_t get_cursor_y(void);
uint16_t move_cursor_right(uint16_t amount);
uint16_t move_cursor_up(uint16_t amount);
uint16_t move_cursor_down(uint16_t amount);
uint16_t move_cursor_left(uint16_t amount);
void move_cursor_to(uint16_t x, uint16_t y);

int get_screen_width(void);
int get_screen_height(void);

#endif