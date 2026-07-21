#include "i2c_bus.h"

#include "driver/i2c_master.h"
#include "esp_log.h"

static const char *TAG = "I2C_BUS";

#define I2C_SCL_GPIO GPIO_NUM_22
#define I2C_SDA_GPIO GPIO_NUM_27
#define I2C_PROBE_TIMEOUT_MS 20

static i2c_master_bus_handle_t s_bus = NULL;
static uint8_t s_found[I2C_BUS_MAX_DEVICES];
static uint8_t s_found_count = 0;

esp_err_t i2c_bus_init(void)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = -1,
        .sda_io_num = I2C_SDA_GPIO,
        .scl_io_num = I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t err = i2c_new_master_bus(&bus_config, &s_bus);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "i2c_new_master_bus failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "I2C bus initialized (SCL=%d, SDA=%d)", I2C_SCL_GPIO, I2C_SDA_GPIO);
    return ESP_OK;
}

esp_err_t i2c_bus_scan(void)
{
    if (s_bus == NULL)
        return ESP_ERR_INVALID_STATE;

    uint8_t count = 0;

    /* 0x00-0x02 et 0x78-0x7F sont réservés (adresses spéciales du
     * protocole I2C), pas de périphérique utilisateur possible là. */
    for (uint8_t addr = 0x03; addr <= 0x77 && count < I2C_BUS_MAX_DEVICES; addr++)
    {
        esp_err_t err = i2c_master_probe(s_bus, addr, I2C_PROBE_TIMEOUT_MS);
        if (err == ESP_OK)
        {
            s_found[count++] = addr;
        }
    }

    s_found_count = count;
    ESP_LOGI(TAG, "Scan done: %u device(s) found", (unsigned)s_found_count);
    return ESP_OK;
}

uint8_t i2c_bus_get_count(void)
{
    return s_found_count;
}

uint8_t i2c_bus_get_address(uint8_t idx)
{
    if (idx >= s_found_count)
        return 0xFF;

    return s_found[idx];
}
