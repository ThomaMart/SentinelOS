cmake_minimum_required(VERSION 3.16)

set(BSP_VENDOR_DIR
    ${CMAKE_CURRENT_LIST_DIR}/../../third_party/esp32-2432S028R/main
)

idf_component_register(

    SRCS
        "bsp.c"
        ${BSP_VENDOR_DIR}/lcd.c
        ${BSP_VENDOR_DIR}/touch.c

    INCLUDE_DIRS
        "."
        ${BSP_VENDOR_DIR}

    REQUIRES
        esp_lcd
        esp_driver_gpio
        esp_driver_spi
        esp_driver_ledc
        lvgl
        espressif__esp_lvgl_port
        espressif__esp_lcd_touch
)