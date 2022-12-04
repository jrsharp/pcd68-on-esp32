#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"

#include "PCD68_CPU.h"
#include "KCTL.h"
#include "BLE_KCTL.h"
#include "Screen_EPD.h"
#include "TDA.h"

#include "text_demo.h"

#define CYCLE_FACTOR 1000
#define INPUT_FACTOR 20

u8* systemRom;                   // ROM
u8* systemRam;                   // RAM
CPU* pcdCpu;                     // CPU
TDA* textDisplayAdapter;         // Graphics adapter
BLE_KCTL* keyboardController;    // Keyboard controller
Screen* pcdScreen;               // Screen instance
u32 keydownDebounceMs = 0;       // debounce period (in ms) for keyboard input
i64 interruptDebounceClocks = 0; // debounce period (in clocks) for keyboard input interrupt
i64 lastClock = 0;
bool dataAvailable = false;      // Clear int

extern "C" {
    void app_main();
}

// Main loop
bool mainLoop() {
    // Process input and update screen
    i64 clocks = pcdCpu->getClock();
    if (clocks % CYCLE_FACTOR == 0) {
        if (clocks % (CYCLE_FACTOR * 10)) {
            textDisplayAdapter->update();
            pcdScreen->refresh();
        }
    }

    //std::cout << "\n\nBefore Instruction: \n\n" << std::endl;
    //pcdCpu->printState();

    // Advance CPU
    pcdCpu->execute();

    if (dataAvailable) {
        keyboardController->clear();
        dataAvailable = false;
    }

    return false;
}

void app_main(void) {

    // Allocate ROM + RAM:
    systemRom = (u8*)malloc(CPU::ROM_SIZE);
    systemRam = (u8*)malloc(CPU::RAM_SIZE);

    // Load binary
    memcpy(systemRom, text_demo_bin, text_demo_bin_len);

    // Start with a CPU
    pcdCpu = new CPU();

    // Set up peripherals
    pcdScreen = new Screen_EPD(Screen::BASE_ADDR, sizeof(Screen::Registers) + sizeof(Screen::framebufferMem));
    textDisplayAdapter = new TDA(pcdCpu, pcdScreen, TDA::BASE_ADDR, sizeof(TDA::textMapMem) + sizeof(TDA::Registers));
    keyboardController = new BLE_KCTL(pcdCpu, KCTL::BASE_ADDR, sizeof(KCTL::Registers), &dataAvailable);

    // Attach to CPU
    pcdCpu->attachPeripheral(pcdScreen);
    pcdCpu->attachPeripheral(textDisplayAdapter);
    pcdCpu->attachPeripheral(keyboardController);

    // Any that require init()
    int result = pcdScreen->init();
    result = keyboardController->init();

    // Turn on front-light:
    led_strip_t* strip = led_strip_init(0, GPIO_NUM_14, 4);
    strip->set_pixel(strip, 0, 200, 200, 160);
    strip->set_pixel(strip, 1, 200, 200, 160);
    strip->set_pixel(strip, 2, 200, 200, 160);
    strip->set_pixel(strip, 3, 200, 200, 160);
    //strip->set_pixel(strip, 0, 0, 0, 0);
    //strip->set_pixel(strip, 1, 0, 0, 0);
    //strip->set_pixel(strip, 2, 0, 0, 0);
    //strip->set_pixel(strip, 3, 0, 0, 0);
    strip->refresh(strip, 100);

    // Spin up HID handler task
    xTaskCreate(&BLE_KCTL::hidTask, "HID Task", 6 * 1024, NULL, 2, NULL);

    // And/or reset()
    keyboardController->reset();
    textDisplayAdapter->reset();

    // And then proceed to reset/start CPU:

    pcdCpu->debugger.enableLogging();
    pcdCpu->reset();
    // Clear all interrupts:
    pcdCpu->setIPL(0x00);

    //pcdCpu->debugger.watchpoints.addAt(0x103a);
    //pcdCpu->debugger.watchpoints.addAt(0x2002);

    // Initial screen:
    textDisplayAdapter->update();
    pcdScreen->refresh();

    while (!mainLoop()) {}

    std::cout << "Clocks: " << std::dec << pcdCpu->getClock() << std::endl;
}
