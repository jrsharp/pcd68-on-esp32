#include <stdio.h>

#include <gdeh042Z21.h>
#include <gdew042t2.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <CPU.h>
#include <KCTL.h>
#include <Screen_SDL.h>
#include <TDA.h>

EpdSpi io;
Gdew042t2 display(io);
//Gdeh042Z21 display(io);

u8 systemRom[CPU::ROM_SIZE];     // ROM
u8 systemRam[CPU::RAM_SIZE];     // RAM
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

void app_main(void) {

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

}
