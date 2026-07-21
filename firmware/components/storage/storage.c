#include "storage.h"

#include <string.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "STORAGE";

#define STORAGE_NAMESPACE "sentinel"

esp_err_t storage_init(void)
{
    esp_err_t err = nvs_flash_init();

    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "Storage initialized");
    }

    return err;
}

esp_err_t storage_set_string(const char *key,
                             const char *value)
{
    nvs_handle_t handle;

    esp_err_t err =
        nvs_open(STORAGE_NAMESPACE,
                 NVS_READWRITE,
                 &handle);

    if (err != ESP_OK)
        return err;

    err = nvs_set_str(handle, key, value);

    if (err == ESP_OK)
        err = nvs_commit(handle);

    nvs_close(handle);

    return err;
}

esp_err_t storage_get_string(const char *key,
                             char *buffer,
                             size_t size)
{
    nvs_handle_t handle;

    esp_err_t err =
        nvs_open(STORAGE_NAMESPACE,
                 NVS_READONLY,
                 &handle);

    if (err != ESP_OK)
        return err;

    size_t required = size;

    err = nvs_get_str(handle,
                      key,
                      buffer,
                      &required);

    nvs_close(handle);

    return err;
}

esp_err_t storage_set_u32(const char *key,
                          uint32_t value)
{
    nvs_handle_t handle;

    esp_err_t err =
        nvs_open(STORAGE_NAMESPACE,
                 NVS_READWRITE,
                 &handle);

    if (err != ESP_OK)
        return err;

    err = nvs_set_u32(handle,
                      key,
                      value);

    if (err == ESP_OK)
        err = nvs_commit(handle);

    nvs_close(handle);

    return err;
}

esp_err_t storage_get_u32(const char *key,
                          uint32_t *value)
{
    nvs_handle_t handle;

    esp_err_t err =
        nvs_open(STORAGE_NAMESPACE,
                 NVS_READONLY,
                 &handle);

    if (err != ESP_OK)
        return err;

    err = nvs_get_u32(handle,
                      key,
                      value);

    nvs_close(handle);

    return err;
}

esp_err_t storage_delete(const char *key)
{
    nvs_handle_t handle;

    esp_err_t err =
        nvs_open(STORAGE_NAMESPACE,
                 NVS_READWRITE,
                 &handle);

    if (err != ESP_OK)
        return err;

    err = nvs_erase_key(handle,
                        key);

    if (err == ESP_OK)
        err = nvs_commit(handle);

    nvs_close(handle);

    return err;
}

esp_err_t storage_erase(void)
{
    nvs_handle_t handle;

    esp_err_t err =
        nvs_open(STORAGE_NAMESPACE,
                 NVS_READWRITE,
                 &handle);

    if (err != ESP_OK)
        return err;

    err = nvs_erase_all(handle);

    if (err == ESP_OK)
        err = nvs_commit(handle);

    nvs_close(handle);

    return err;
}