#include "display.h"
#include "config.h"
#include "wifi.h"

#include "bsp.h"
#include "diag.h"
#include "i2c_bus.h"
#include "radar.h"
#include "rogue_ap.h"
#include "sdcard.h"
#include "rwr.h"
#include "system_info.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <esp_check.h>
#include <esp_efuse.h>
#include <esp_log.h>
#include <esp_lvgl_port.h>
#include <esp_secure_boot.h>
#include <esp_task_wdt.h>
#include <lvgl.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *TAG = "DISPLAY";

#define LVGL_LOCK_TIMEOUT_MS 0
#define CONSOLE_MAX_CHARS 2000

/*=========================================================
 * Terminal / security console theme
 *========================================================*/

#define COLOR_BG        lv_color_hex(0x0A0F0A)
#define COLOR_PANEL     lv_color_hex(0x0F1A0F)
#define COLOR_BORDER    lv_color_hex(0x1F5C33)
#define COLOR_GREEN     lv_color_hex(0x33FF66)
#define COLOR_GREEN_DIM lv_color_hex(0x2A8C4A)
#define COLOR_AMBER     lv_color_hex(0xFFAA00)
#define COLOR_RED       lv_color_hex(0xFF4444)

/*=========================================================
 * Private state
 *========================================================*/

static TaskHandle_t s_display_task = NULL;
static void display_task(void *arg);

static display_ota_check_cb_t s_ota_check_cb = NULL;
static display_ota_update_cb_t s_ota_update_cb = NULL;
static bool s_ota_available = false;

static lv_obj_t *lbl_wifi = NULL;
static lv_obj_t *lbl_heap = NULL;
static lv_obj_t *lbl_uptime = NULL;

static lv_obj_t *lbl_ota_status = NULL;
static lv_obj_t *bar_ota_progress = NULL;
static lv_obj_t *btn_ota_check = NULL;
static lv_obj_t *lbl_btn_ota = NULL;

static lv_obj_t *lbl_secure_boot = NULL;
static lv_obj_t *lbl_flash_enc = NULL;

static lv_obj_t *bar_radar_level = NULL;
static lv_obj_t *lbl_radar_status = NULL;
static lv_obj_t *lbl_radar_samples = NULL;

static lv_obj_t *lbl_rwr_status = NULL;
static lv_obj_t *lbl_rwr_counts = NULL;
static lv_obj_t *lbl_rwr_attacker = NULL;

static lv_obj_t *lbl_i2c_status = NULL;
static lv_obj_t *ta_i2c_list = NULL;

static lv_obj_t *lbl_rogue_ap_status = NULL;
static lv_obj_t *ta_rogue_ap_list = NULL;

static lv_obj_t *lbl_sdcard_status = NULL;
static lv_obj_t *lbl_sdcard_cap = NULL;

static lv_obj_t *ta_console = NULL;

/*=========================================================
 * Style helpers
 *========================================================*/

static void style_screen_root(lv_obj_t *obj)
{
    lv_obj_set_style_bg_color(obj, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(obj, COLOR_GREEN, 0);
    lv_obj_set_style_text_font(obj, &lv_font_unscii_8, 0);
}

/*=========================================================
 * Navigation : écran menu + pages plein écran (remplace
 * l'ancienne tab bar, à l'étroit une fois 6 fonctions ajoutées)
 *========================================================*/

typedef enum
{
    PAGE_MENU = 0,
    PAGE_OTA,
    PAGE_SEC,
    PAGE_RADAR,
    PAGE_RWR,
    PAGE_I2C,
    PAGE_ROGUE_AP,
    PAGE_SDCARD,
    PAGE_LOG,
    PAGE_COUNT,
} page_id_t;

static lv_obj_t *s_pages[PAGE_COUNT] = {0};

static void show_page(page_id_t id)
{
    lvgl_port_lock(LVGL_LOCK_TIMEOUT_MS);
    for (int i = 0; i < PAGE_COUNT; i++)
    {
        if (s_pages[i] == NULL)
            continue;

        if (i == (int)id)
            lv_obj_remove_flag(s_pages[i], LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(s_pages[i], LV_OBJ_FLAG_HIDDEN);
    }
    lvgl_port_unlock();
}

static void menu_button_cb(lv_event_t *e)
{
    page_id_t target = (page_id_t)(intptr_t)lv_event_get_user_data(e);
    show_page(target);
}

static void back_button_cb(lv_event_t *e)
{
    (void)e;
    show_page(PAGE_MENU);
}

static void add_back_button(lv_obj_t *page)
{
    lv_obj_t *btn = lv_button_create(page);
    lv_obj_set_size(btn, 64, 36);
    lv_obj_set_style_bg_color(btn, COLOR_PANEL, 0);
    lv_obj_set_style_border_color(btn, COLOR_GREEN_DIM, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_ext_click_area(btn, 10);
    lv_obj_add_event_cb(btn, back_button_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, "< BACK");
    lv_obj_set_style_text_color(lbl, COLOR_GREEN, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_unscii_8, 0);
    lv_obj_center(lbl);
}

static lv_obj_t *make_tab_page(lv_obj_t *parent)
{
    lv_obj_t *tab = lv_obj_create(parent);
    lv_obj_set_size(tab, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(tab, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(tab, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(tab, 0, 0);
    lv_obj_set_style_pad_all(tab, 6, 0);
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(tab, 3, 0);
    lv_obj_remove_flag(tab, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(tab, LV_OBJ_FLAG_HIDDEN);
    return tab;
}

static void clip_to_parent_width(lv_obj_t *lbl)
{
    lv_obj_set_width(lbl, lv_pct(100));
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_MODE_CLIP);
}

static lv_obj_t *make_row_label_small(lv_obj_t *parent, const char *text)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, COLOR_GREEN_DIM, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_unscii_8, 0);
    clip_to_parent_width(lbl);
    return lbl;
}

static lv_obj_t *make_section_title(lv_obj_t *parent, const char *text)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, COLOR_AMBER, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    return lbl;
}

/*=========================================================
 * Console tab
 *========================================================*/

static void console_log(const char *text)
{
    if (ta_console == NULL || text == NULL)
        return;

    lvgl_port_lock(LVGL_LOCK_TIMEOUT_MS);

    const char *current = lv_textarea_get_text(ta_console);
    if (current != NULL && strlen(current) > CONSOLE_MAX_CHARS)
    {
        lv_textarea_set_text(ta_console, "");
    }

    lv_textarea_add_text(ta_console, "> ");
    lv_textarea_add_text(ta_console, text);
    lv_textarea_add_text(ta_console, "\n");

    lvgl_port_unlock();
}

static lv_obj_t *build_console_tab(lv_obj_t *parent)
{
    lv_obj_t *tab = make_tab_page(parent);
    add_back_button(tab);

    ta_console = lv_textarea_create(tab);
    lv_obj_set_width(ta_console, lv_pct(100));
    lv_obj_set_flex_grow(ta_console, 1);
    lv_textarea_set_cursor_click_pos(ta_console, false);
    lv_obj_set_style_bg_color(ta_console, COLOR_PANEL, 0);
    lv_obj_set_style_border_color(ta_console, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(ta_console, 1, 0);
    lv_obj_set_style_text_color(ta_console, COLOR_GREEN, 0);
    lv_obj_set_style_text_font(ta_console, &lv_font_unscii_8, 0);
    lv_textarea_set_text(ta_console, "");
    lv_textarea_add_text(ta_console, "SentinelOS console ready\n");

    return tab;
}

/*=========================================================
 * Dashboard tab
 *========================================================*/

static const struct
{
    const char *label;
    page_id_t target;
} MENU_ITEMS[] = {
    {"OTA UPDATE", PAGE_OTA},
    {"SECURITY", PAGE_SEC},
    {"RADAR (CSI)", PAGE_RADAR},
    {"RWR (DEAUTH WATCH)", PAGE_RWR},
    {"I2C SCAN", PAGE_I2C},
    {"WIFI SCAN (ROGUE AP)", PAGE_ROGUE_AP},
    {"SD CARD", PAGE_SDCARD},
    {"LOG", PAGE_LOG},
};

static lv_obj_t *build_menu_page(lv_obj_t *parent)
{
    lv_obj_t *page = make_tab_page(parent);
    lv_obj_remove_flag(page, LV_OBJ_FLAG_HIDDEN);
    /* Espacement resserré : 5 boutons (40px, agrandis pour le tactile) +
     * 4 lignes de statut dépassent légèrement les 320px de haut -- en plus
     * du resserrage, scroll vertical activé en filet de sécurité plutôt
     * que de rétrécir les boutons (leur taille corrige un vrai problème
     * de précision tactile). */
    lv_obj_set_style_pad_row(page, 2, 0);
    lv_obj_add_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(page, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(page, LV_SCROLLBAR_MODE_ON);
    lv_obj_set_style_bg_color(page, COLOR_GREEN, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, LV_PART_SCROLLBAR);
    lv_obj_set_style_width(page, 8, LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(page, 4, LV_PART_SCROLLBAR);

    lv_obj_t *title = lv_label_create(page);
    lv_label_set_text_fmt(title, "%s v%s", SENTINELOS_NAME, SENTINELOS_VERSION);
    lv_obj_set_style_text_color(title, COLOR_GREEN, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    clip_to_parent_width(title);

    lbl_wifi = make_row_label_small(page, "WIFI   : ---");
    lbl_heap = make_row_label_small(page, "HEAP   : ---- KB");
    lbl_uptime = make_row_label_small(page, "UPTIME : 00:00");

    for (size_t i = 0; i < sizeof(MENU_ITEMS) / sizeof(MENU_ITEMS[0]); i++)
    {
        lv_obj_t *btn = lv_button_create(page);
        lv_obj_set_size(btn, lv_pct(100), 40);
        lv_obj_set_style_margin_top(btn, 5, 0);
        lv_obj_set_style_bg_color(btn, COLOR_PANEL, 0);
        lv_obj_set_style_border_color(btn, COLOR_GREEN, 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        /* Écran résistif peu précis (variance de tap observée ~25-40px) --
         * élargit la zone cliquable au-delà du visuel du bouton pour
         * absorber cette imprécision sans agrandir le bouton lui-même. */
        lv_obj_set_ext_click_area(btn, 10);
        lv_obj_add_event_cb(btn, menu_button_cb, LV_EVENT_CLICKED,
                            (void *)(intptr_t)MENU_ITEMS[i].target);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, MENU_ITEMS[i].label);
        lv_obj_set_style_text_color(lbl, COLOR_GREEN, 0);
        lv_obj_set_style_text_font(lbl, &lv_font_unscii_8, 0);
        lv_obj_center(lbl);
    }

    return page;
}

/*=========================================================
 * OTA tab
 *========================================================*/

static void ota_check_task(void *arg)
{
    (void)arg;

    if (s_ota_check_cb != NULL)
    {
        s_ota_check_cb();
    }

    vTaskDelete(NULL);
}

static void ota_update_task(void *arg)
{
    (void)arg;

    if (s_ota_update_cb != NULL)
    {
        s_ota_update_cb();
    }

    vTaskDelete(NULL);
}

static void ota_check_button_cb(lv_event_t *e)
{
    (void)e;

    ESP_LOGI(TAG, "OTA button tapped, available=%d", s_ota_available);

    if (s_ota_available)
    {
        xTaskCreate(ota_update_task, "ota_update_ui", 4096, NULL, 5, NULL);
    }
    else
    {
        xTaskCreate(ota_check_task, "ota_check_ui", 4096, NULL, 5, NULL);
    }
}

static lv_obj_t *build_ota_tab(lv_obj_t *parent)
{
    lv_obj_t *tab = make_tab_page(parent);
    add_back_button(tab);

    make_section_title(tab, "OTA UPDATE");

    lv_obj_t *lbl_ver = lv_label_create(tab);
    lv_label_set_text_fmt(lbl_ver, "Current version: %s", SENTINELOS_VERSION);
    lv_obj_set_style_text_color(lbl_ver, COLOR_GREEN, 0);
    lv_obj_set_style_text_font(lbl_ver, &lv_font_unscii_8, 0);
    lv_obj_set_style_margin_top(lbl_ver, 6, 0);

    bar_ota_progress = lv_bar_create(tab);
    lv_obj_set_size(bar_ota_progress, lv_pct(100), 16);
    lv_bar_set_range(bar_ota_progress, 0, 100);
    lv_bar_set_value(bar_ota_progress, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar_ota_progress, COLOR_PANEL, LV_PART_MAIN);
    lv_obj_set_style_border_color(bar_ota_progress, COLOR_BORDER, LV_PART_MAIN);
    lv_obj_set_style_border_width(bar_ota_progress, 1, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar_ota_progress, COLOR_GREEN, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar_ota_progress, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_margin_top(bar_ota_progress, 8, 0);

    lbl_ota_status = make_row_label_small(tab, "Status: idle");
    lv_obj_set_style_margin_top(lbl_ota_status, 8, 0);

    btn_ota_check = lv_button_create(tab);
    lv_obj_set_style_margin_top(btn_ota_check, 20, 0);
    lv_obj_set_size(btn_ota_check, lv_pct(100), 44);
    lv_obj_set_style_bg_color(btn_ota_check, COLOR_PANEL, 0);
    lv_obj_set_style_border_color(btn_ota_check, COLOR_GREEN, 0);
    lv_obj_set_style_border_width(btn_ota_check, 1, 0);
    lv_obj_add_event_cb(btn_ota_check, ota_check_button_cb, LV_EVENT_CLICKED, NULL);

    lbl_btn_ota = lv_label_create(btn_ota_check);
    lv_label_set_text(lbl_btn_ota, "CHECK FOR UPDATE");
    lv_obj_set_style_text_color(lbl_btn_ota, COLOR_GREEN, 0);
    lv_obj_set_style_text_font(lbl_btn_ota, &lv_font_unscii_8, 0);
    lv_obj_center(lbl_btn_ota);

    return tab;
}

/*=========================================================
 * Security tab
 *========================================================*/

static lv_obj_t *build_security_tab(lv_obj_t *parent)
{
    lv_obj_t *tab = make_tab_page(parent);
    add_back_button(tab);
    lv_obj_set_flex_align(tab, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(tab, 10, 0);

    lv_obj_t *title = make_section_title(tab, "SECURITY");
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

    bool secure_boot = esp_secure_boot_enabled();
    bool flash_enc = esp_efuse_is_flash_encryption_enabled();

    lbl_secure_boot = lv_label_create(tab);
    lv_label_set_text_fmt(lbl_secure_boot, "Secure Boot\n%s",
                          secure_boot ? "ENABLED (RSA-PSS)" : "disabled");
    lv_obj_set_style_text_color(lbl_secure_boot,
                                secure_boot ? COLOR_GREEN : COLOR_RED, 0);
    lv_obj_set_style_text_font(lbl_secure_boot, &lv_font_unscii_8, 0);
    clip_to_parent_width(lbl_secure_boot);
    lv_obj_set_style_text_align(lbl_secure_boot, LV_TEXT_ALIGN_CENTER, 0);

    lbl_flash_enc = lv_label_create(tab);
    lv_label_set_text_fmt(lbl_flash_enc, "Flash Encrypt\n%s",
                          flash_enc ? "ENABLED" : "disabled");
    lv_obj_set_style_text_color(lbl_flash_enc,
                                flash_enc ? COLOR_GREEN : COLOR_AMBER, 0);
    lv_obj_set_style_text_font(lbl_flash_enc, &lv_font_unscii_8, 0);
    clip_to_parent_width(lbl_flash_enc);
    lv_obj_set_style_text_align(lbl_flash_enc, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *lbl1 = make_row_label_small(tab, "OTA Integrity\nSHA-256");
    lv_obj_t *lbl2 = make_row_label_small(tab, "OTA Authenticity\nECDSA P-256");
    lv_obj_t *lbl3 = make_row_label_small(tab, "UART Protocol\nCRC16 framed");
    lv_obj_set_style_text_align(lbl1, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_align(lbl2, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_align(lbl3, LV_TEXT_ALIGN_CENTER, 0);

    bool was_crash = diag_last_reset_was_crash();
    lv_obj_t *lbl4 = lv_label_create(tab);
    lv_label_set_text_fmt(lbl4, "Last Reset\n%s (crashes: %lu)",
                          diag_get_last_reset_reason_str(),
                          (unsigned long)diag_get_crash_count());
    lv_obj_set_style_text_color(lbl4, was_crash ? COLOR_AMBER : COLOR_GREEN_DIM, 0);
    lv_obj_set_style_text_font(lbl4, &lv_font_unscii_8, 0);
    clip_to_parent_width(lbl4);
    lv_obj_set_style_text_align(lbl4, LV_TEXT_ALIGN_CENTER, 0);

    bool has_coredump = diag_has_coredump();
    lv_obj_t *lbl5 = lv_label_create(tab);
    lv_label_set_text_fmt(lbl5, "Core Dump\n%s",
                          has_coredump ? "PRESENT (flash)" : "none");
    lv_obj_set_style_text_color(lbl5, has_coredump ? COLOR_AMBER : COLOR_GREEN_DIM, 0);
    lv_obj_set_style_text_font(lbl5, &lv_font_unscii_8, 0);
    clip_to_parent_width(lbl5);
    lv_obj_set_style_text_align(lbl5, LV_TEXT_ALIGN_CENTER, 0);

    /* Filet de sécurité si le contenu dépasse les 320px de haut une fois
     * cette ligne ajoutée (même leçon que la page menu). */
    lv_obj_add_flag(tab, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(tab, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(tab, LV_SCROLLBAR_MODE_ON);
    lv_obj_set_style_bg_color(tab, COLOR_GREEN, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(tab, LV_OPA_COVER, LV_PART_SCROLLBAR);
    lv_obj_set_style_width(tab, 8, LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(tab, 4, LV_PART_SCROLLBAR);

    return tab;
}

/*=========================================================
 * Radar tab (Wi-Fi CSI presence/motion sensing)
 *========================================================*/

static lv_obj_t *build_radar_tab(lv_obj_t *parent)
{
    lv_obj_t *tab = make_tab_page(parent);
    add_back_button(tab);
    lv_obj_set_flex_align(tab, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *title = make_section_title(tab, "WIFI RADAR (CSI)");
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *lbl_desc = lv_label_create(tab);
    lv_label_set_text(lbl_desc, "Ambient signal variance");
    lv_obj_set_style_text_color(lbl_desc, COLOR_GREEN_DIM, 0);
    lv_obj_set_style_text_font(lbl_desc, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_align(lbl_desc, LV_TEXT_ALIGN_CENTER, 0);

    bar_radar_level = lv_bar_create(tab);
    lv_obj_set_size(bar_radar_level, lv_pct(100), 16);
    lv_bar_set_range(bar_radar_level, 0, 100);
    lv_bar_set_value(bar_radar_level, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar_radar_level, COLOR_PANEL, LV_PART_MAIN);
    lv_obj_set_style_border_color(bar_radar_level, COLOR_BORDER, LV_PART_MAIN);
    lv_obj_set_style_border_width(bar_radar_level, 1, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar_radar_level, COLOR_GREEN, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar_radar_level, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_margin_top(bar_radar_level, 8, 0);

    lbl_radar_status = lv_label_create(tab);
    lv_label_set_text(lbl_radar_status, "CLEAR");
    lv_obj_set_style_text_color(lbl_radar_status, COLOR_GREEN, 0);
    lv_obj_set_style_text_font(lbl_radar_status, &lv_font_unscii_16, 0);
    lv_obj_set_style_text_align(lbl_radar_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_margin_top(lbl_radar_status, 10, 0);

    lbl_radar_samples = make_row_label_small(tab, "Samples: 0");
    lv_obj_set_style_text_align(lbl_radar_samples, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_margin_top(lbl_radar_samples, 8, 0);

    return tab;
}

/*=========================================================
 * RWR tab (Wi-Fi deauth/disassoc attack detection)
 *========================================================*/

static lv_obj_t *build_rwr_tab(lv_obj_t *parent)
{
    lv_obj_t *tab = make_tab_page(parent);
    add_back_button(tab);
    lv_obj_set_flex_align(tab, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *title = make_section_title(tab, "WIFI RWR");
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *lbl_desc = lv_label_create(tab);
    lv_label_set_text(lbl_desc, "Deauth/disassoc watch");
    lv_obj_set_style_text_color(lbl_desc, COLOR_GREEN_DIM, 0);
    lv_obj_set_style_text_font(lbl_desc, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_align(lbl_desc, LV_TEXT_ALIGN_CENTER, 0);

    lbl_rwr_status = lv_label_create(tab);
    lv_label_set_text(lbl_rwr_status, "CLEAR");
    lv_obj_set_style_text_color(lbl_rwr_status, COLOR_GREEN, 0);
    lv_obj_set_style_text_font(lbl_rwr_status, &lv_font_unscii_16, 0);
    lv_obj_set_style_text_align(lbl_rwr_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_margin_top(lbl_rwr_status, 10, 0);

    lbl_rwr_counts = make_row_label_small(tab, "Deauth: 0 / Mgmt: 0");
    lv_obj_set_style_text_align(lbl_rwr_counts, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_margin_top(lbl_rwr_counts, 8, 0);

    lbl_rwr_attacker = make_row_label_small(tab, "Last: --:--:--:--:--:--");
    lv_obj_set_style_text_align(lbl_rwr_attacker, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_margin_top(lbl_rwr_attacker, 4, 0);

    return tab;
}

/*=========================================================
 * I2C tab (bus scan)
 *========================================================*/

static void i2c_scan_task(void *arg)
{
    (void)arg;

    i2c_bus_scan();

    uint8_t count = i2c_bus_get_count();

    lvgl_port_lock(LVGL_LOCK_TIMEOUT_MS);

    if (lbl_i2c_status != NULL)
    {
        lv_label_set_text_fmt(lbl_i2c_status, "Status: %u device(s) found", (unsigned)count);
    }

    if (ta_i2c_list != NULL)
    {
        lv_textarea_set_text(ta_i2c_list, "");
        if (count == 0)
        {
            lv_textarea_add_text(ta_i2c_list, "(no device on bus)\n");
        }
        else
        {
            for (uint8_t i = 0; i < count; i++)
            {
                char line[32];
                snprintf(line, sizeof(line), "0x%02X\n", i2c_bus_get_address(i));
                lv_textarea_add_text(ta_i2c_list, line);
            }
        }
    }

    lvgl_port_unlock();

    vTaskDelete(NULL);
}

static void i2c_scan_button_cb(lv_event_t *e)
{
    (void)e;

    if (lbl_i2c_status != NULL)
    {
        lvgl_port_lock(LVGL_LOCK_TIMEOUT_MS);
        lv_label_set_text(lbl_i2c_status, "Status: scanning...");
        lvgl_port_unlock();
    }

    xTaskCreate(i2c_scan_task, "i2c_scan_ui", 4096, NULL, 5, NULL);
}

static lv_obj_t *build_i2c_tab(lv_obj_t *parent)
{
    lv_obj_t *tab = make_tab_page(parent);
    add_back_button(tab);

    make_section_title(tab, "I2C BUS SCAN");

    lv_obj_t *lbl_desc = lv_label_create(tab);
    lv_label_set_text(lbl_desc, "Header pins: SCL=22 SDA=27");
    lv_obj_set_style_text_color(lbl_desc, COLOR_GREEN_DIM, 0);
    lv_obj_set_style_text_font(lbl_desc, &lv_font_unscii_8, 0);
    lv_obj_set_style_margin_top(lbl_desc, 6, 0);

    lbl_i2c_status = make_row_label_small(tab, "Status: idle");
    lv_obj_set_style_margin_top(lbl_i2c_status, 6, 0);

    ta_i2c_list = lv_textarea_create(tab);
    lv_obj_set_width(ta_i2c_list, lv_pct(100));
    lv_obj_set_flex_grow(ta_i2c_list, 1);
    lv_textarea_set_cursor_click_pos(ta_i2c_list, false);
    lv_obj_set_style_bg_color(ta_i2c_list, COLOR_PANEL, 0);
    lv_obj_set_style_border_color(ta_i2c_list, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(ta_i2c_list, 1, 0);
    lv_obj_set_style_text_color(ta_i2c_list, COLOR_GREEN, 0);
    lv_obj_set_style_text_font(ta_i2c_list, &lv_font_unscii_8, 0);
    lv_obj_set_style_margin_top(ta_i2c_list, 8, 0);
    lv_textarea_set_text(ta_i2c_list, "");

    lv_obj_t *btn = lv_button_create(tab);
    lv_obj_set_size(btn, lv_pct(100), 40);
    lv_obj_set_style_margin_top(btn, 8, 0);
    lv_obj_set_style_bg_color(btn, COLOR_PANEL, 0);
    lv_obj_set_style_border_color(btn, COLOR_GREEN, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_ext_click_area(btn, 10);
    lv_obj_add_event_cb(btn, i2c_scan_button_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_btn = lv_label_create(btn);
    lv_label_set_text(lbl_btn, "SCAN");
    lv_obj_set_style_text_color(lbl_btn, COLOR_GREEN, 0);
    lv_obj_set_style_text_font(lbl_btn, &lv_font_unscii_8, 0);
    lv_obj_center(lbl_btn);

    return tab;
}

/*=========================================================
 * Rogue AP / evil twin detector tab
 *========================================================*/

static void rogue_ap_scan_task(void *arg)
{
    (void)arg;

    rogue_ap_scan();

    uint8_t count = rogue_ap_get_count();
    uint8_t flagged = rogue_ap_get_flagged_count();

    lvgl_port_lock(LVGL_LOCK_TIMEOUT_MS);

    if (lbl_rogue_ap_status != NULL)
    {
        lv_label_set_text_fmt(lbl_rogue_ap_status, "Status: %u network(s), %u suspect",
                              (unsigned)count, (unsigned)flagged);
        lv_obj_set_style_text_color(lbl_rogue_ap_status, flagged > 0 ? COLOR_AMBER : COLOR_GREEN, 0);
    }

    if (ta_rogue_ap_list != NULL)
    {
        lv_textarea_set_text(ta_rogue_ap_list, "");
        if (count == 0)
        {
            lv_textarea_add_text(ta_rogue_ap_list, "(no network found)\n");
        }
        else
        {
            for (uint8_t i = 0; i < count; i++)
            {
                char line[80];
                if (rogue_ap_is_flagged(i))
                {
                    snprintf(line, sizeof(line), "! %s [%s] %ddBm\n  -> %s\n",
                            rogue_ap_get_ssid(i), rogue_ap_get_bssid_str(i),
                            rogue_ap_get_rssi(i), rogue_ap_get_flag_reason(i));
                }
                else
                {
                    snprintf(line, sizeof(line), "%s [%s] %ddBm\n",
                            rogue_ap_get_ssid(i), rogue_ap_get_bssid_str(i),
                            rogue_ap_get_rssi(i));
                }
                lv_textarea_add_text(ta_rogue_ap_list, line);
            }
        }
    }

    lvgl_port_unlock();

    vTaskDelete(NULL);
}

static void rogue_ap_scan_button_cb(lv_event_t *e)
{
    (void)e;

    if (lbl_rogue_ap_status != NULL)
    {
        lvgl_port_lock(LVGL_LOCK_TIMEOUT_MS);
        lv_label_set_text(lbl_rogue_ap_status, "Status: scanning...");
        lv_obj_set_style_text_color(lbl_rogue_ap_status, COLOR_GREEN_DIM, 0);
        lvgl_port_unlock();
    }

    xTaskCreate(rogue_ap_scan_task, "rogue_ap_scan_ui", 4096, NULL, 5, NULL);
}

static lv_obj_t *build_rogue_ap_tab(lv_obj_t *parent)
{
    lv_obj_t *tab = make_tab_page(parent);
    add_back_button(tab);

    make_section_title(tab, "WIFI SCAN");

    lv_obj_t *lbl_desc = lv_label_create(tab);
    lv_label_set_text(lbl_desc, "! = duplicate SSID / weak security");
    lv_obj_set_style_text_color(lbl_desc, COLOR_GREEN_DIM, 0);
    lv_obj_set_style_text_font(lbl_desc, &lv_font_unscii_8, 0);
    lv_obj_set_style_margin_top(lbl_desc, 6, 0);

    lbl_rogue_ap_status = make_row_label_small(tab, "Status: idle");
    lv_obj_set_style_margin_top(lbl_rogue_ap_status, 6, 0);

    ta_rogue_ap_list = lv_textarea_create(tab);
    lv_obj_set_width(ta_rogue_ap_list, lv_pct(100));
    lv_obj_set_flex_grow(ta_rogue_ap_list, 1);
    lv_textarea_set_cursor_click_pos(ta_rogue_ap_list, false);
    lv_obj_set_style_bg_color(ta_rogue_ap_list, COLOR_PANEL, 0);
    lv_obj_set_style_border_color(ta_rogue_ap_list, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(ta_rogue_ap_list, 1, 0);
    lv_obj_set_style_text_color(ta_rogue_ap_list, COLOR_GREEN, 0);
    lv_obj_set_style_text_font(ta_rogue_ap_list, &lv_font_unscii_8, 0);
    lv_obj_set_style_margin_top(ta_rogue_ap_list, 8, 0);
    lv_textarea_set_text(ta_rogue_ap_list, "");

    lv_obj_t *btn = lv_button_create(tab);
    lv_obj_set_size(btn, lv_pct(100), 40);
    lv_obj_set_style_margin_top(btn, 8, 0);
    lv_obj_set_style_bg_color(btn, COLOR_PANEL, 0);
    lv_obj_set_style_border_color(btn, COLOR_GREEN, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_ext_click_area(btn, 10);
    lv_obj_add_event_cb(btn, rogue_ap_scan_button_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_btn = lv_label_create(btn);
    lv_label_set_text(lbl_btn, "SCAN");
    lv_obj_set_style_text_color(lbl_btn, COLOR_GREEN, 0);
    lv_obj_set_style_text_font(lbl_btn, &lv_font_unscii_8, 0);
    lv_obj_center(lbl_btn);

    return tab;
}

/*=========================================================
 * SD card tab
 *========================================================*/

static lv_obj_t *build_sdcard_tab(lv_obj_t *parent)
{
    lv_obj_t *tab = make_tab_page(parent);
    add_back_button(tab);
    lv_obj_set_flex_align(tab, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(tab, 10, 0);

    lv_obj_t *title = make_section_title(tab, "SD CARD");
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

    lbl_sdcard_status = lv_label_create(tab);
    lv_label_set_text(lbl_sdcard_status, "Status\nchecking...");
    lv_obj_set_style_text_color(lbl_sdcard_status, COLOR_GREEN_DIM, 0);
    lv_obj_set_style_text_font(lbl_sdcard_status, &lv_font_unscii_8, 0);
    clip_to_parent_width(lbl_sdcard_status);
    lv_obj_set_style_text_align(lbl_sdcard_status, LV_TEXT_ALIGN_CENTER, 0);

    lbl_sdcard_cap = lv_label_create(tab);
    lv_label_set_text(lbl_sdcard_cap, "");
    lv_obj_set_style_text_color(lbl_sdcard_cap, COLOR_GREEN_DIM, 0);
    lv_obj_set_style_text_font(lbl_sdcard_cap, &lv_font_unscii_8, 0);
    clip_to_parent_width(lbl_sdcard_cap);
    lv_obj_set_style_text_align(lbl_sdcard_cap, LV_TEXT_ALIGN_CENTER, 0);

    return tab;
}

/*=========================================================
 * Public API
 *========================================================*/

esp_err_t display_init(void)
{
    ESP_RETURN_ON_ERROR(bsp_init(), TAG, "BSP initialization failed");

    lv_display_t *disp = bsp_display_get();
    if (disp == NULL)
    {
        ESP_LOGE(TAG, "BSP did not return a display");
        return ESP_FAIL;
    }

    lvgl_port_lock(LVGL_LOCK_TIMEOUT_MS);

    lv_obj_t *screen = lv_screen_active();
    lv_obj_clean(screen);
    style_screen_root(screen);

    s_pages[PAGE_MENU]  = build_menu_page(screen);
    s_pages[PAGE_OTA]   = build_ota_tab(screen);
    s_pages[PAGE_SEC]   = build_security_tab(screen);
    s_pages[PAGE_RADAR] = build_radar_tab(screen);
    s_pages[PAGE_RWR]   = build_rwr_tab(screen);
    s_pages[PAGE_I2C]   = build_i2c_tab(screen);
    s_pages[PAGE_ROGUE_AP] = build_rogue_ap_tab(screen);
    s_pages[PAGE_SDCARD] = build_sdcard_tab(screen);
    s_pages[PAGE_LOG]   = build_console_tab(screen);

    lvgl_port_unlock();

    ESP_LOGI(TAG, "Display initialized");

    xTaskCreate(display_task, "display", 4096, NULL, 5, &s_display_task);

    return ESP_OK;
}

void display_set_ota_check_callback(display_ota_check_cb_t cb)
{
    s_ota_check_cb = cb;
}

void display_set_ota_update_callback(display_ota_update_cb_t cb)
{
    s_ota_update_cb = cb;
}

void display_set_ota_available(bool available)
{
    s_ota_available = available;

    if (lbl_btn_ota == NULL)
        return;

    lvgl_port_lock(LVGL_LOCK_TIMEOUT_MS);
    lv_label_set_text(lbl_btn_ota, available ? "UPDATE" : "CHECK FOR UPDATE");
    lv_obj_set_style_text_color(lbl_btn_ota, available ? COLOR_AMBER : COLOR_GREEN, 0);
    if (btn_ota_check != NULL)
    {
        lv_obj_set_style_border_color(btn_ota_check, available ? COLOR_AMBER : COLOR_GREEN, 0);
    }
    lvgl_port_unlock();
}

void display_set_heap(size_t heap)
{
    if (lbl_heap == NULL)
        return;

    lvgl_port_lock(LVGL_LOCK_TIMEOUT_MS);
    lv_label_set_text_fmt(lbl_heap, "HEAP   : %u KB", (unsigned int)(heap / 1024));
    lvgl_port_unlock();
}

void display_set_uptime(uint32_t seconds)
{
    if (lbl_uptime == NULL)
        return;

    uint32_t minutes = seconds / 60;
    uint32_t secs = seconds % 60;

    lvgl_port_lock(LVGL_LOCK_TIMEOUT_MS);
    lv_label_set_text_fmt(lbl_uptime, "UPTIME : %02lu:%02lu",
                          (unsigned long)minutes, (unsigned long)secs);
    lvgl_port_unlock();
}

void display_set_status(const char *status)
{
    if (status == NULL)
        return;

    if (lbl_ota_status != NULL)
    {
        lvgl_port_lock(LVGL_LOCK_TIMEOUT_MS);
        lv_label_set_text_fmt(lbl_ota_status, "Status: %s", status);
        lvgl_port_unlock();
    }

    console_log(status);
}

void display_set_ota_progress(int percent)
{
    if (bar_ota_progress == NULL)
        return;

    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    lvgl_port_lock(LVGL_LOCK_TIMEOUT_MS);
    lv_bar_set_value(bar_ota_progress, percent, LV_ANIM_OFF);
    lvgl_port_unlock();
}

void display_set_wifi(bool connected)
{
    if (lbl_wifi == NULL)
        return;

    lvgl_port_lock(LVGL_LOCK_TIMEOUT_MS);
    lv_label_set_text(lbl_wifi, connected ? "WIFI   : ON" : "WIFI   : OFF");
    lv_obj_set_style_text_color(lbl_wifi, connected ? COLOR_GREEN : COLOR_RED, 0);
    lvgl_port_unlock();
}

void display_set_sdcard_status(bool mounted, uint32_t capacity_mb, uint32_t free_mb)
{
    if (lbl_sdcard_status == NULL)
        return;

    lvgl_port_lock(LVGL_LOCK_TIMEOUT_MS);

    lv_label_set_text_fmt(lbl_sdcard_status, "Status\n%s",
                          mounted ? "MOUNTED (FATFS/SPI)" : "no card / mount failed");
    lv_obj_set_style_text_color(lbl_sdcard_status, mounted ? COLOR_GREEN : COLOR_AMBER, 0);

    if (mounted && lbl_sdcard_cap != NULL)
    {
        lv_label_set_text_fmt(lbl_sdcard_cap, "Capacity\n%lu MB total / %lu MB free",
                              (unsigned long)capacity_mb, (unsigned long)free_mb);
    }

    lvgl_port_unlock();
}

static void display_update_radar(void)
{
    if (bar_radar_level == NULL)
        return;

    int level = radar_get_signal_level();
    bool motion = radar_motion_detected();
    uint32_t samples = radar_get_sample_count();

    ESP_LOGI(TAG, "radar: level=%d motion=%d samples=%lu", level, motion, (unsigned long)samples);

    lvgl_port_lock(LVGL_LOCK_TIMEOUT_MS);

    lv_bar_set_value(bar_radar_level, level, LV_ANIM_ON);

    lv_label_set_text(lbl_radar_status, motion ? "MOTION DETECTED" : "CLEAR");
    lv_obj_set_style_text_color(lbl_radar_status, motion ? COLOR_AMBER : COLOR_GREEN, 0);

    lv_label_set_text_fmt(lbl_radar_samples, "Samples: %lu", (unsigned long)samples);

    lvgl_port_unlock();
}

static void display_update_rwr(void)
{
    if (lbl_rwr_status == NULL)
        return;

    bool attack = rwr_attack_detected();
    uint32_t deauth = rwr_get_deauth_count();
    uint32_t total = rwr_get_total_frames();
    const char *last = rwr_get_last_attacker();

    lvgl_port_lock(LVGL_LOCK_TIMEOUT_MS);

    lv_label_set_text(lbl_rwr_status, attack ? "ATTACK DETECTED" : "CLEAR");
    lv_obj_set_style_text_color(lbl_rwr_status, attack ? COLOR_RED : COLOR_GREEN, 0);

    lv_label_set_text_fmt(lbl_rwr_counts, "Deauth: %lu / Mgmt: %lu",
                          (unsigned long)deauth, (unsigned long)total);

    if (last[0] != '\0')
    {
        lv_label_set_text_fmt(lbl_rwr_attacker, "Last: %s", last);
    }

    lvgl_port_unlock();
}

static void display_task(void *arg)
{
    (void)arg;

    /* Watchdog applicatif : cette tâche s'inscrit au Task Watchdog Timer
     * (timeout 5s, voir sdkconfig) et le nourrit à chaque tour de boucle
     * -- si elle se bloque (deadlock LVGL, boucle infinie), le TWDT
     * déclenche un panic + reboot au lieu de laisser l'UI figée
     * indéfiniment. */
    esp_task_wdt_add(NULL);

    while (true)
    {
        display_set_heap(system_get_free_heap());
        display_set_uptime(system_get_uptime());
        display_set_wifi(wifi_is_connected());
        display_update_radar();
        display_update_rwr();

        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
