#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "PCD68_CPU.h"
#include "KCTL.h"
#include "Peripheral.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_bt_defs.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_hidh.h"
#include "esp_hid_gap.h"
#include <cstring>
#include <stdint.h>

extern u8* systemRam;
static constexpr char* TAG = "BLE_KCTL";

class BLE_KCTL : public KCTL {

public:
    /**
     * Constructor
     *
     * @param cpu ref
     * @param start base address
     * @param size size of memory
     * @param dataAvailableFlag is data available?
     */
    BLE_KCTL(CPU* cpu, uint32_t start, uint32_t size, bool* dataAvailableFlag);

    /**
     * Initializes resources for keyboard controller
     *
     * returns non-zero on error
     */
    int init();

    /**
     * Keyboard controller polls for input and updates registers accordingly
     */
    void update();

    /**
     * Clears all regs
     */
    void clear();

    void reset() override;
    u8 read8(u32 addr) override;
    u16 read16(u32 addr) override;
    void write8(u32 addr, u8 val) override;
    void write16(u32 addr, u16 val) override;

    /** HID event handler */ 
    static void hidCallback(void* handler_args, esp_event_base_t base, int32_t id, void* event_data);

    /** FreeRTOS task function */
    static void hidTask(void* pvParameters);

    /** Keyboard semaphore */
    static SemaphoreHandle_t* semaphore;

private:
    bool* dataAvailableFlag;
};
