#include "Screen_EPD.h"
#include <iostream>

EpdSpi Screen_EPD::io;
Gdew042t2 Screen_EPD::display(Screen_EPD::io);

// C'tor
Screen_EPD::Screen_EPD(uint32_t start, uint32_t size) :
    Peripheral(start, size),
    Screen(start, size) {
}

// Init EPD
int Screen_EPD::init() {
    registers.busy = false;
    //framebufferMem = (u8*)malloc((400 * 300));
    if (framebufferMem == nullptr) {
        return -1;
    }
    display.init(false);
    display.fillScreen(EPD_WHITE);
    display.update();
    return 0;
}

// Reset EPD
void Screen_EPD::reset() {
    refreshFlag = true;
    // TODO: actual, soft (hard?) reset of EPD
}

// Refresh screen
int Screen_EPD::refresh() {
    if (refreshFlag) {
        registers.busy = true;
        // copy
        u8 oneBB = 0xFF; // start with white
        for (int i = 0; i < (400 * 300); i++) {
            if (framebufferMem[i] == 0x00) { // black pixel found
                oneBB = (oneBB & ~(1 << (7 - (i % 8))));
            } else if (framebufferMem[i] == 0xFF) { // white pixel found
                oneBB = (oneBB | (1 << (7 - (i % 8))));
            }
            if (i % 8 == 7) { // Filled a byte?
                display._buffer[i / 8] = oneBB;
                oneBB = 0xFF;
            }
        }
        display.update();
        registers.busy = false;
        refreshFlag = false;
    }

    return 0;
}
