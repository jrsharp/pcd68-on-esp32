#pragma once

#include "CPU.h"
#include "Screen.h"
#include "Peripheral.h"
#include <gdeh042Z21.h>
#include <gdew042t2.h>
#include <stdint.h>

class Screen_EPD : public Screen {

public:
    /**
     * Struct for Screen's registers
     */
    struct Registers {
        bool busy;
    };

    /**
     * Constructor
     *
     * @param start base address
     * @param size size of memory
     */
    Screen_EPD(uint32_t start, uint32_t size);

    /**
     * Initialize Screen peripheral
     * (Set up SPI, other hw, etc.)
     */
    virtual int init();

    /**
     * Update / refresh display contents
     */
    virtual int refresh();

    virtual void reset();

private:
    static EpdSpi io;
    static Gdew042t2 display;
};
