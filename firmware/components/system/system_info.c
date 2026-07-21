#include "system_info.h"

#include "config.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"

static uint64_t s_boot_time_us = 0;

void system_init(void)
{
    s_boot_time_us = esp_timer_get_time();
}

uint32_t system_get_uptime(void)
{
    return (uint32_t)((esp_timer_get_time() - s_boot_time_us) / 1000000ULL);
}

uint32_t system_get_free_heap(void)
{
    return heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
}

uint32_t system_get_minimum_free_heap(void)
{
    return heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);
}

const char *system_get_version(void)
{
    return SENTINELOS_VERSION;
}