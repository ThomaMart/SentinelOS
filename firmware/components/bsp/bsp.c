#include "bsp.h"

#include "lcd.h"
#include "touch.h"

#include <esp_check.h>
#include <esp_log.h>
#include <esp_lvgl_port.h>

static const char *TAG = "BSP";

/*=========================================================
 * Private state
 *========================================================*/

static lv_display_t *s_display = NULL;

static void touch_indev_event_cb(lv_event_t *e)
{
    lv_indev_t *indev = lv_event_get_indev(e);
    lv_point_t point;
    lv_indev_get_point(indev, &point);
    ESP_LOGI(TAG, "Touch pressed: x=%ld y=%ld", (long)point.x, (long)point.y);
}

/*=========================================================
 * Public API
 *========================================================*/

esp_err_t bsp_init(void)
{
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_handle_t panel = NULL;
    esp_lcd_touch_handle_t touch = NULL;

    ESP_RETURN_ON_ERROR(
        app_lcd_init(&io, &panel),
        TAG,
        "LCD initialization failed"
    );

    s_display = app_lvgl_init(io, panel);

    if (s_display == NULL) {
        ESP_LOGE(TAG, "LVGL initialization failed");
        return ESP_FAIL;
    }

    ESP_RETURN_ON_ERROR(
        touch_init(&touch),
        TAG,
        "Touch initialization failed"
    );

    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = s_display,
        .handle = touch,
    };

    lv_indev_t *touch_indev = lvgl_port_add_touch(&touch_cfg);
    if (touch_indev == NULL) {
        ESP_LOGE(TAG, "Failed to register touch with LVGL");
        return ESP_FAIL;
    }

    lv_indev_add_event_cb(touch_indev, touch_indev_event_cb, LV_EVENT_PRESSED, NULL);

    ESP_RETURN_ON_ERROR(
        lcd_display_brightness_init(),
        TAG,
        "Backlight PWM initialization failed"
    );

    ESP_RETURN_ON_ERROR(
        lcd_display_backlight_on(),
        TAG,
        "Backlight initialization failed"
    );

    ESP_LOGI(TAG, "Board initialized");

    return ESP_OK;
}

lv_display_t *bsp_display_get(void)
{
    return s_display;
}

esp_err_t bsp_backlight_on(void)
{
    return lcd_display_backlight_on();
}