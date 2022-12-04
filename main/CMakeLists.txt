set(srcs
    "BLE_KCTL.cpp"
    "Screen_EPD.cpp"
    "esp_hid_gap.c"
    "pcd68-on-esp32.cpp")

idf_component_register(SRCS ${srcs}
                        INCLUDE_DIRS "."
                        REQUIRES "pcd68")

