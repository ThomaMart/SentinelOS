#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <esp_err.h>

esp_err_t display_init(void);

/**
 * @brief Callbacks for the OTA tab button, registered by app.c (which owns
 * both display and ota) to avoid a circular component dependency.
 *
 * The button starts in "check" mode (calls the check callback, which must
 * NOT auto-download). If display_set_ota_available(true) is called as a
 * result, the button switches to "update" mode (calls the update callback,
 * which actually starts the download) until the next check resets it.
 */
typedef void (*display_ota_check_cb_t)(void);
typedef void (*display_ota_update_cb_t)(void);

void display_set_ota_check_callback(display_ota_check_cb_t cb);
void display_set_ota_update_callback(display_ota_update_cb_t cb);

/** Switches the OTA tab button between "CHECK FOR UPDATE" and "UPDATE". */
void display_set_ota_available(bool available);

void display_set_heap(size_t heap);

void display_set_uptime(uint32_t seconds);

/** Updates the OTA tab status line and appends the message to the console tab. */
void display_set_status(const char *status);

/** Updates the OTA tab progress bar (0-100). */
void display_set_ota_progress(int percent);

void display_set_wifi(bool connected);

/** Met à jour l'onglet SD CARD (statut de montage, capacité/espace libre). */
void display_set_sdcard_status(bool mounted, uint32_t capacity_mb, uint32_t free_mb);
