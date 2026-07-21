#include <stdio.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_system.h>
#include <esp_log.h>

#include <lvgl.h>
#include <esp_lvgl_port.h>

#include "lcd.h"
#include "touch.h"

static const char *TAG = "SentinelOS";

static lv_obj_t *lbl_heap;

static esp_err_t app_ui_create(void)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    lvgl_port_lock(0);

    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "SentinelOS");
    lv_obj_set_style_text_color(title, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

    lv_obj_t *status = lv_label_create(scr);
    lv_label_set_text(status, "FreeRTOS : OK");
    lv_obj_set_style_text_color(status, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(status, LV_ALIGN_TOP_LEFT, 10, 70);

    lv_obj_t *firmware = lv_label_create(scr);
    lv_label_set_text(firmware, "Firmware : v0.1.0");
    lv_obj_set_style_text_color(firmware, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(firmware, LV_ALIGN_TOP_LEFT, 10, 100);

    lbl_heap = lv_label_create(scr);
    lv_label_set_text(lbl_heap, "Heap :");
    lv_obj_set_style_text_color(lbl_heap, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(lbl_heap, LV_ALIGN_TOP_LEFT, 10, 130);

    lvgl_port_unlock();

    return ESP_OK;
}

void app_main(void)
{
    ESP_LOGI(TAG, "1 - Backlight init");
    ESP_ERROR_CHECK(lcd_display_brightness_init());

    ESP_LOGI(TAG, "2 - LCD init");
    ESP_ERROR_CHECK(app_lcd_init(&lcd_io, &lcd_panel));

    ESP_LOGI(TAG, "3 - LVGL init");
    display = app_lvgl_init(lcd_io, lcd_panel);

    if (display == NULL) {
        ESP_LOGE(TAG, "LVGL init failed");
        esp_restart();
    }

    ESP_LOGI(TAG, "4 - Touch init");
    ESP_ERROR_CHECK(touch_init(&tp));

    touch_cfg.disp = display;
    touch_cfg.handle = tp;
    touch_cfg.scale.x = 0;
    touch_cfg.scale.y = 0;

    ESP_LOGI(TAG, "5 - Register touch");
    lvgl_port_add_touch(&touch_cfg);

    ESP_LOGI(TAG, "6 - Backlight");
    ESP_ERROR_CHECK(lcd_display_brightness_set(75));

    ESP_LOGI(TAG, "7 - Rotation");
    ESP_ERROR_CHECK(lcd_display_rotate(display, LV_DISPLAY_ROTATION_0));

    ESP_LOGI(TAG, "8 - UI");
    ESP_ERROR_CHECK(app_ui_create());

    ESP_LOGI(TAG, "9 - Loop");
    esp_lcd_panel_io_handle_t lcd_io;
    esp_lcd_panel_handle_t lcd_panel;

    esp_lcd_touch_handle_t tp;
    lvgl_port_touch_cfg_t touch_cfg;

    lv_display_t *display = NULL;

    ESP_ERROR_CHECK(lcd_display_brightness_init());

    ESP_ERROR_CHECK(app_lcd_init(&lcd_io, &lcd_panel));

    display = app_lvgl_init(lcd_io, lcd_panel);

    if (display == NULL) {
        ESP_LOGE(TAG, "LVGL init failed");
        esp_restart();
    }

    ESP_ERROR_CHECK(touch_init(&tp));

    touch_cfg.disp = display;
    touch_cfg.handle = tp;
    touch_cfg.scale.x = 0;
    touch_cfg.scale.y = 0;

    lvgl_port_add_touch(&touch_cfg);

    ESP_ERROR_CHECK(lcd_display_brightness_set(75));
    ESP_ERROR_CHECK(lcd_display_rotate(display, LV_DISPLAY_ROTATION_0));

    ESP_ERROR_CHECK(app_ui_create());

    char buffer[64];

    while (1)
    {
        snprintf(buffer,
                 sizeof(buffer),
                 "Heap : %u bytes",
                 (unsigned)esp_get_free_heap_size());

        if (lvgl_port_lock(0))
        {
            lv_label_set_text(lbl_heap, buffer);
            lvgl_port_unlock();
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}