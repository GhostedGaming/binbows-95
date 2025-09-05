#ifndef FONT_H
#define FONT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define FONT_WIDTH 8
#define FONT_HEIGHT 16

extern const uint8_t font8x16[95][16];

void draw_char(int x, int y, char c, uint32_t color);
void draw_text(int x, int y, const char *str, uint32_t color, bool clear);

static inline int get_char_width(char c) { (void)c; return FONT_WIDTH; }
static inline int get_string_width(const char *str) { int w=0; while(*str) w+=FONT_WIDTH, str++; return w; }
static inline int get_font_height(void) { return FONT_HEIGHT; }
static inline int is_printable_char(char c) { return (c >= 32 && c <= 126); }
static inline const uint8_t* get_char_bitmap(char c) { return (is_printable_char(c) ? font8x16[(int)c - 32] : NULL); }

#endif // FONT_H