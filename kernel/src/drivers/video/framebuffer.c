#include <framebuffer.h>
#include <limine.h>
#include <stddef.h>
#include <serial.h>
#include <stdint.h>
#include <font.h>
#include <util.h>
#include <timer.h>
#include <stdbool.h>

extern volatile struct limine_framebuffer_request framebuffer_request;

int fb_width = 0;
int fb_height = 0;
int fb_pitch = 0;
uint32_t *fb_ptr = NULL;

// Cursor position globals
uint16_t cursor_position_x = 0;
uint16_t cursor_position_y = 0;

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

// Additional functions from draw.c

static int string_length(const char *str) {
    int len = 0;
    while (str[len] != '\0') len++;
    return len;
}

void draw_char_with_bg(int x, int y, char c, uint32_t fg_color, uint32_t bg_color) {
    if (c >= 32 && c <= 126) {
        const uint8_t *char_bitmap = font8x16[(unsigned char)c - 32];
        
        for (int row = 0; row < FONT_HEIGHT; row++) {
            uint8_t byte = char_bitmap[row];
            for (int col = 0; col < FONT_WIDTH; col++) {
                int px = x + col;
                int py = y + row;
                if (px >= 0 && px < fb_width && py >= 0 && py < fb_height) {
                    uint32_t color = (byte & (0x80 >> col)) ? fg_color : bg_color;
                    fb_ptr[py * fb_pitch + px] = color;
                }
            }
        }
    } else {
        // Draw background for invalid characters
        for (int row = 0; row < FONT_HEIGHT; row++) {
            for (int col = 0; col < FONT_WIDTH; col++) {
                int px = x + col;
                int py = y + row;
                if (px >= 0 && px < fb_width && py >= 0 && py < fb_height) {
                    fb_ptr[py * fb_pitch + px] = bg_color;
                }
            }
        }
    }
}

void draw_text_with_bg(int x, int y, const char *str, uint32_t fg_color, uint32_t bg_color) {
    int current_x = x;
    
    for (int i = 0; str[i]; i++) {
        draw_char_with_bg(current_x, y, str[i], fg_color, bg_color);
        current_x += FONT_WIDTH;
    }
}

void draw_text_centered(int y, const char *str, uint32_t color) {
    int len = string_length(str);
    int str_width = len * FONT_WIDTH;
    int cx = (fb_width - str_width) / 2;
    draw_text(cx, y, str, color, false);
}

void draw_text_centered_with_bg(int y, const char *str, uint32_t fg_color, uint32_t bg_color) {
    int len = string_length(str);
    int str_width = len * FONT_WIDTH;
    int cx = (fb_width - str_width) / 2;
    draw_text_with_bg(cx, y, str, fg_color, bg_color);
}

void draw_text_center_screen(const char *str, uint32_t color) {
    int len = string_length(str);
    int str_width = len * FONT_WIDTH;
    int cx = (fb_width - str_width) / 2;
    int cy = (fb_height - FONT_HEIGHT) / 2;
    draw_text(cx, cy, str, color, false);
}

void draw_text_center_screen_with_bg(const char *str, uint32_t fg_color, uint32_t bg_color) {
    int len = string_length(str);
    int str_width = len * FONT_WIDTH;
    int cx = (fb_width - str_width) / 2;
    int cy = (fb_height - FONT_HEIGHT) / 2;
    draw_text_with_bg(cx, cy, str, fg_color, bg_color);
}

void draw_rect(int x, int y, int width, int height, uint32_t color) {
    for (int py = y; py < y + height && py < fb_height; py++) {
        for (int px = x; px < x + width && px < fb_width; px++) {
            if (px >= 0 && py >= 0) {
                fb_ptr[py * fb_pitch + px] = color;
            }
        }
    }
}

void remove_rect(int x, int y, int width, int height, uint32_t bg_color) {
    for (int py = y; py < y + height && py < fb_height; py++) {
        for (int px = x; px < x + width && px < fb_width; px++) {
            if (px >= 0 && py >= 0) {
                fb_ptr[py * fb_pitch + px] = bg_color;
            }
        }
    }
}

// Cursor movement functions

uint16_t move_cursor_right(uint16_t amount) {
    if (cursor_position_x + amount < fb_width) {
        cursor_position_x += amount;
    } else {
        cursor_position_x = fb_width - 1;
    }
    return cursor_position_x;
}

uint16_t move_cursor_up(uint16_t amount) {
    if (cursor_position_y >= amount) {
        cursor_position_y -= amount;
    } else {
        cursor_position_y = 0;
    }
    return cursor_position_y;
}

uint16_t move_cursor_down(uint16_t amount) {
    if (cursor_position_y + amount < fb_height) {
        cursor_position_y += amount;
    } else {
        cursor_position_y = fb_height - 1;
    }
    return cursor_position_y;
}

uint16_t move_cursor_left(uint16_t amount) {
    if (cursor_position_x >= amount) {
        cursor_position_x -= amount;
    } else {
        cursor_position_x = 0;
    }
    return cursor_position_x;
}

void move_cursor_to(uint16_t x, uint16_t y) {
    if (x < fb_width) {
        cursor_position_x = x;
    }
    if (y < fb_height) {
        cursor_position_y = y;
    }
}

void set_cursor_pos(uint16_t x, uint16_t y) {
    move_cursor_to(x, y);
}

void move_cursor_by_chars(int chars) {
    if (chars > 0) {
        move_cursor_right(chars * FONT_WIDTH);
    } else if (chars < 0) {
        move_cursor_left((-chars) * FONT_WIDTH);
    }
}

void move_cursor_by_lines(int lines) {
    if (lines > 0) {
        move_cursor_down(lines * FONT_HEIGHT);
    } else if (lines < 0) {
        move_cursor_up((-lines) * FONT_HEIGHT);
    }
}

void cursor_newline(void) {
    move_cursor_to(0, cursor_position_y + FONT_HEIGHT);
}

uint16_t get_cursor_x(void) {
    return cursor_position_x;
}

uint16_t get_cursor_y(void) {
    return cursor_position_y;
}

int get_screen_width(void) {
    return fb_width;
}

int get_screen_height(void) {
    return fb_height;
}