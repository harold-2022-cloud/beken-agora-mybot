#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifndef POWER_ON_GPIO_PIN
#define POWER_ON_GPIO_PIN   GPIO_12
#endif

#define KEY_DEFAULT_CONFIG_TABLE \
{ \
    { \
        .gpio_id = GPIO_13, \
        .active_level = LOW_LEVEL_TRIGGER, \
        .short_event = VOLUME_UP, \
        .double_event = EVENT_NONE, \
        .long_event = CONFIG_NETWORK \
    }, \
    { \
        .gpio_id = POWER_ON_GPIO_PIN, \
        .active_level = LOW_LEVEL_TRIGGER, \
        .short_event = AI_AGENT_CONFIG, \
        .long_event = SHUT_DOWN \
    }, \
    { \
        .gpio_id = GPIO_8, \
        .active_level = LOW_LEVEL_TRIGGER, \
        .short_event = VOLUME_DOWN, \
        .double_event = VOLUME_DOWN \
    }, \
}

#ifdef __cplusplus
}
#endif
