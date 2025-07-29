#include <io.h>
#include <timer.h>
#include <pc_speaker.h>

void play_sound(uint32_t Freq) {
    uint32_t Div;
    uint32_t tmp;

    // Set the pit to desired frequency
    Div = 1193180 / Freq;
    outb(0x43, 0xb6);
    outb(0x42, (uint8_t) (Div) );
    outb(0x42, (uint8_t) (Div >> 8));

    // Play the sound using the pc speaker
    tmp = inb(0x61);
    if (tmp != (tmp | 3)) {
        outb(0x61, tmp | 3);
    }
}

// Tell the pc speaker to sybau
void nosound() {
    uint8_t tmp = inb(0x61) & 0xFC;

    outb(0x61, tmp);
}

void beep(uint32_t Freq, uint32_t duration) {
    play_sound(Freq);

    timer_wait(duration);

    nosound();
}