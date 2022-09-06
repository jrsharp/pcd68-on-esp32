#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <CPU.h>
#include <KCTL.h>
#include <Screen_EPD.h>
#include <TDA.h>

#include "text_demo.h"

#define CYCLE_FACTOR 1000
#define INPUT_FACTOR 20

u8* systemRom;                   // ROM
u8* systemRam;                   // RAM
CPU* pcdCpu;                     // CPU
TDA* textDisplayAdapter;         // Graphics adapter
KCTL* keyboardController;        // Keyboard controller
Screen* pcdScreen;               // Screen instance
u32 keydownDebounceMs = 0;       // debounce period (in ms) for keyboard input
i64 interruptDebounceClocks = 0; // debounce period (in clocks) for keyboard input interrupt
i64 lastClock = 0;
u16 keyCode = 0;
u16 mod = 0;

extern "C" {
    void app_main();
}

bool handleEvents(u16* kc) {
    return true;
}

// Main loop
bool mainLoop() {
    bool exit = false, clearKbdInt = false;

    //std::cout << ".";

    // Process input and update screen
    i64 clocks = pcdCpu->getClock();
    if (clocks % CYCLE_FACTOR == 0) {
        // Process input only a fraction
        if (clocks % (CYCLE_FACTOR * INPUT_FACTOR) == 0) {
            exit = !handleEvents(&keyCode);

            if (keyCode > 0) {
                keyboardController->update(keyCode, mod);
                textDisplayAdapter->update();
                pcdScreen->refresh();
                keyCode = 0;
                mod = 0;
                clearKbdInt = true;
            }
        }

        if (clocks % (CYCLE_FACTOR * 10)) {
            textDisplayAdapter->update();
            pcdScreen->refresh();
        }
    }

    //std::cout << "\n\nBefore Instruction: \n\n" << std::endl;
    //pcdCpu->printState();

    // Advance CPU
    pcdCpu->execute();

    if (clearKbdInt) {
        keyboardController->clear();
        clearKbdInt = false;
    }

    return exit;
}

void app_main(void) {

    systemRom = (u8*)malloc(CPU::ROM_SIZE);
    systemRam = (u8*)malloc(CPU::RAM_SIZE);

    // Load binary
    memcpy(systemRom, text_demo_bin, text_demo_bin_len);

    // Start with a CPU
    pcdCpu = new CPU();

    // Set up peripherals
    pcdScreen = new Screen(Screen::BASE_ADDR, sizeof(Screen::Registers) + sizeof(Screen::framebufferMem));
    textDisplayAdapter = new TDA(pcdCpu, pcdScreen, TDA::BASE_ADDR, sizeof(TDA::textMapMem) + sizeof(TDA::Registers));
    keyboardController = new KCTL(pcdCpu, KCTL::BASE_ADDR, sizeof(KCTL::Registers));

    // Attach to CPU
    pcdCpu->attachPeripheral(pcdScreen);
    pcdCpu->attachPeripheral(textDisplayAdapter);
    pcdCpu->attachPeripheral(keyboardController);

    // Any that require init()
    int result = pcdScreen->init();

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
