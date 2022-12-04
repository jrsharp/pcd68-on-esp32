#include "BLE_KCTL.h"

SemaphoreHandle_t* BLE_KCTL::semaphore = nullptr;

BLE_KCTL::BLE_KCTL(CPU* cpu, uint32_t start, uint32_t size, bool* dataAvailableFlag) :
    KCTL(cpu, start, size) {
    this->dataAvailableFlag = dataAvailableFlag;
}

int BLE_KCTL::init() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );
    ESP_LOGI(TAG, "setting hid gap, mode:%d", HID_HOST_MODE);
    ESP_ERROR_CHECK( esp_hid_gap_init(HID_HOST_MODE) );
    ESP_ERROR_CHECK( esp_hid_ble_gap_adv_init(0x03c1, "pcd68") );
    ESP_ERROR_CHECK( esp_ble_gattc_register_callback(esp_hidh_gattc_event_handler) );

    esp_hidh_config_t config = {
        .callback = BLE_KCTL::hidCallback,
        .event_stack_size = 4096,
        .callback_arg = this,
    };

    ESP_ERROR_CHECK( esp_hidh_init(&config) );

    return 0;
}

void BLE_KCTL::reset() {
    this->registers.status = Status::CONNECTED;
}

void BLE_KCTL::update() {
    /*
    registers.keycode = keycode;
    registers.mod = mod;
    */
    this->cpu->setIPL(KBD_INT_LEVEL);
}

void BLE_KCTL::clear() {
    this->cpu->setIPL(0x00);
}

u8 BLE_KCTL::read8(u32 addr) {
    if (addr >= BASE_ADDR && addr < BASE_ADDR + sizeof(registers)) {
        return get8((u8*)&registers, addr - BASE_ADDR);
    }
    return 0;
}

u16 BLE_KCTL::read16(u32 addr) {
    if (addr >= BASE_ADDR && addr < BASE_ADDR + sizeof(registers)) {
        return get16((u8*)&registers, addr - BASE_ADDR);
    }
    return 0;
}

void BLE_KCTL::write8(u32 addr, u8 val) {
    if (addr >= BASE_ADDR && addr < BASE_ADDR + sizeof(registers)) {
        set8((u8*)&registers, addr - BASE_ADDR, val);
    }
}

void BLE_KCTL::write16(u32 addr, u16 val) {
    if (addr >= BASE_ADDR && addr < BASE_ADDR + sizeof(registers)) {
        set16((u8*)&registers, addr - BASE_ADDR, val);
    }
}

void BLE_KCTL::hidCallback(void* handler_args, esp_event_base_t base, int32_t id, void* event_data) {
    BLE_KCTL* self = (BLE_KCTL*)handler_args;
    esp_hidh_event_t event = (esp_hidh_event_t)id;
    esp_hidh_event_data_t* param = (esp_hidh_event_data_t*)event_data;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    switch (event) {
    case ESP_HIDH_OPEN_EVENT: {
        if (param->open.status == ESP_OK) {
            const uint8_t *bda = esp_hidh_dev_bda_get(param->open.dev);
            ESP_LOGI(TAG, ESP_BD_ADDR_STR " OPEN: %s", ESP_BD_ADDR_HEX(bda), esp_hidh_dev_name_get(param->open.dev));
            esp_hidh_dev_dump(param->open.dev, stdout);
        } else {
            ESP_LOGE(TAG, " OPEN failed!");
        }
        break;
    }
    case ESP_HIDH_BATTERY_EVENT: {
        const uint8_t *bda = esp_hidh_dev_bda_get(param->battery.dev);
        ESP_LOGI(TAG, ESP_BD_ADDR_STR " BATTERY: %d%%", ESP_BD_ADDR_HEX(bda), param->battery.level);
        break;
    }
    case ESP_HIDH_INPUT_EVENT: {
        const uint8_t *bda = esp_hidh_dev_bda_get(param->input.dev);
        ESP_LOGI(TAG, ESP_BD_ADDR_STR " INPUT: %8s, MAP: %2u, ID: %3u, Len: %d, Data:", ESP_BD_ADDR_HEX(bda), esp_hid_usage_str(param->input.usage), param->input.map_index, param->input.report_id, param->input.length);
        ESP_LOG_BUFFER_HEX(TAG, param->input.data, param->input.length);
        u8 mod = param->input.data[0];
        u8 key1 = param->input.data[2];
        u8 key2 = param->input.data[3];
        u8 key3 = param->input.data[4];
        u8 key4 = param->input.data[5];
        u8 key5 = param->input.data[6];
        u8 key6 = param->input.data[7];
        if (self->registers.pendingReportCount < REPORT_STACK_SIZE) {
            KCTL::KeyReport report = { mod, { key1, key2, key3, key4, key5, key6, 0x00 } };
            self->registers.reportStack[self->registers.pendingReportCount++] = report;
            self->update();
            if (self->dataAvailableFlag != nullptr) {
                *(self->dataAvailableFlag) = true;
            }
        }

        /*
        if (BLE_KCTL::semaphore != nullptr) {
            xSemaphoreGiveFromISR(BLE_KCTL::semaphore, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
        */
        break;
    }
    case ESP_HIDH_CLOSE_EVENT: {
        const uint8_t *bda = esp_hidh_dev_bda_get(param->close.dev);
        ESP_LOGI(TAG, ESP_BD_ADDR_STR " CLOSE: %s", ESP_BD_ADDR_HEX(bda), esp_hidh_dev_name_get(param->close.dev));
        break;
    }
    default:
        ESP_LOGI(TAG, "EVENT: %d", event);
        break;
    }
}

void BLE_KCTL::hidTask(void* pvParameters) {
    size_t results_len = 0;
    esp_hid_scan_result_t *results = NULL;
    ESP_LOGI(TAG, "SCAN...");
    //start scan for HID devices
    esp_hid_scan(5, &results_len, &results);
    ESP_LOGI(TAG, "SCAN: %u results", results_len);
    if (results_len) {
        esp_hid_scan_result_t *r = results;
        esp_hid_scan_result_t *cr = NULL;
        while (r) {
            printf("  %s: " ESP_BD_ADDR_STR ", ", (r->transport == ESP_HID_TRANSPORT_BLE) ? "BLE" : "BT ", ESP_BD_ADDR_HEX(r->bda));
            printf("RSSI: %d, ", r->rssi);
            printf("USAGE: %s, ", esp_hid_usage_str(r->usage));
            if (r->transport == ESP_HID_TRANSPORT_BLE) {
                cr = r;
                printf("APPEARANCE: 0x%04x, ", r->ble.appearance);
                printf("ADDR_TYPE: '%s', ", ble_addr_type_str(r->ble.addr_type));
            }
            printf("NAME: %s ", r->name ? r->name : "");
            printf("\n");
            r = r->next;
        }
        if (cr) {
            //open the last result
            esp_hidh_dev_open(cr->bda, cr->transport, cr->ble.addr_type);
        }
        //free the results
        esp_hid_scan_results_free(results);
    }
    vTaskDelete(NULL);
}
