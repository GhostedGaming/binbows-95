#include <framebuffer.h>
#include <limine.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <font.h>
#include <util.h>


extern volatile struct limine_framebuffer_request framebuffer_request;

int fb_width = 0;
int fb_height = 0;
int fb_pitch = 0;
uint32_t *fb_ptr = NULL;

static int fb_count = 0;
static struct limine_framebuffer **framebuffers = NULL;

uint16_t cursor_position_x = 0;
uint16_t cursor_position_y = 0;

uint32_t rgb_to_color(uint8_t r, uint8_t g, uint8_t b) {
    return (0xFF << 24) | (r << 16) | (g << 8) | b;
}

void clear_screen() {
    for (int fb_idx = 0; fb_idx < fb_count; fb_idx++) {
        struct limine_framebuffer *fb = framebuffers[fb_idx];
        uint32_t *ptr = (uint32_t *)fb->address;
        int width = (int)fb->width;
        int height = (int)fb->height;
        uint64_t pixels = (uint64_t)width * height;
        
        for (uint64_t i = 0; i < pixels; i++) {
            ptr[i] = 0x00000000;
        }
    }
}

void init_fb() {
    while (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count == 0);

    fb_count = (int)framebuffer_request.response->framebuffer_count;
    framebuffers = framebuffer_request.response->framebuffers;
    
    struct limine_framebuffer *framebuffer = framebuffers[0];
    
    fb_width = (int)framebuffer->width;
    fb_height = (int)framebuffer->height;
    fb_pitch = (int)(framebuffer->pitch / 4);
    fb_ptr = (uint32_t *)framebuffer->address;

    clear_screen();
}

void draw_char(int x, int y, char c, uint32_t color) {
    if (!is_printable_char(c)) return;

    const uint8_t *char_bitmap = font8x16[(int)c - 32];

    for (int fb_idx = 0; fb_idx < fb_count; fb_idx++) {
        struct limine_framebuffer *fb = framebuffers[fb_idx];
        uint32_t *ptr = (uint32_t *)fb->address;
        int width = (int)fb->width;
        int height = (int)fb->height;
        int pitch = (int)(fb->pitch / 4);
        
        for (int row = 0; row < FONT_HEIGHT; row++) {
            uint8_t byte = char_bitmap[row];
            for (int col = 0; col < FONT_WIDTH; col++) {
                if (byte & (0x80 >> col)) {
                    int px = x + col;
                    int py = y + row;
                    if (px >= 0 && px < width && py >= 0 && py < height) {
                        ptr[py * pitch + px] = color;
                    }
                }
            }
        }
    }
}

void draw_rect(int x, int y, int width, int height, uint32_t color) {
    for (int fb_idx = 0; fb_idx < fb_count; fb_idx++) {
        struct limine_framebuffer *fb = framebuffers[fb_idx];
        uint32_t *ptr = (uint32_t *)fb->address;
        int fb_width_local = (int)fb->width;
        int fb_height_local = (int)fb->height;
        int pitch = (int)(fb->pitch / 4);
        
        for (int py = y; py < y + height && py < fb_height_local; py++) {
            for (int px = x; px < x + width && px < fb_width_local; px++) {
                if (px >= 0 && py >= 0) {
                    ptr[py * pitch + px] = color;
                }
            }
        }
    }
}

static void scroll_screen(void) {
    for (int fb_idx = 0; fb_idx < fb_count; fb_idx++) {
        struct limine_framebuffer *fb = framebuffers[fb_idx];
        uint32_t *ptr = (uint32_t *)fb->address;
        int width = (int)fb->width;
        int height = (int)fb->height;
        int pitch = (int)(fb->pitch / 4);
        
        for (int y = 0; y < height - FONT_HEIGHT; y++) {
            memmove(&ptr[y * pitch], 
                    &ptr[(y + FONT_HEIGHT) * pitch], 
                    width * sizeof(uint32_t));
        }
        
        for (int y = height - FONT_HEIGHT; y < height; y++) {
            for (int x = 0; x < width; x++) {
                ptr[y * pitch + x] = 0x00000000;
            }
        }
    }
    
    cursor_position_y -= FONT_HEIGHT;
}

void fb_print(const char *str, uint32_t color) {
    if (!str) return;
    
    while (*str) {
        if (*str == '\n') {
            cursor_position_x = 0;
            cursor_position_y += FONT_HEIGHT;
            
            if (cursor_position_y >= fb_height - FONT_HEIGHT) {
                scroll_screen();
            }
        } else if (*str == '\r') {
            cursor_position_x = 0;
        } else if (*str == '\t') {
            int spaces = 4 - (cursor_position_x / FONT_WIDTH) % 4;
            for (int i = 0; i < spaces; i++) {
                draw_char(cursor_position_x, cursor_position_y, ' ', color);
                cursor_position_x += FONT_WIDTH;
                
                if (cursor_position_x >= fb_width - FONT_WIDTH) {
                    cursor_position_x = 0;
                    cursor_position_y += FONT_HEIGHT;
                    
                    if (cursor_position_y >= fb_height - FONT_HEIGHT) {
                        scroll_screen();
                    }
                }
            }
        } else {
            draw_char(cursor_position_x, cursor_position_y, *str, color);
            cursor_position_x += FONT_WIDTH;
            
            if (cursor_position_x >= fb_width - FONT_WIDTH) {
                cursor_position_x = 0;
                cursor_position_y += FONT_HEIGHT;
                
                if (cursor_position_y >= fb_height - FONT_HEIGHT) {
                    scroll_screen();
                }
            }
        }
        str++;
    }
}

void fb_printf(const char *format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    fb_print(buffer, 0xFFFFFFFF);
}

void move_cursor_to(uint16_t x, uint16_t y) {
    if (x < fb_width) {
        cursor_position_x = x;
    }
    if (y < fb_height) {
        cursor_position_y = y;
    }
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