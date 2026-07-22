#pragma once

#include <stdint.h>

/* Stub: volume APIs are handled by ext_key_driver in the SMP framework */
void volume_set_abs(uint8_t level, uint8_t has_precision);
uint32_t volume_get_current(void);
uint32_t volume_get_level_count(void);
