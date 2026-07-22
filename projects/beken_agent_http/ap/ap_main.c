#include "bk_private/bk_init.h"
#include <components/system.h>
#include <os/os.h>
#include <driver/gpio.h>
#include "gpio_driver.h"
#include <media_service.h>
#include "sys_driver.h"
#include "sys_hal.h"
#include <modules/pm.h>

#if CONFIG_BK_NETWORK_TRANSFER
#include "network_transfer.h"
#endif

#if CONFIG_BK_AUDIO_ENGINE
#include "audio_engine.h"
#endif

#if CONFIG_BK_VIDEO_ENGINE
#include "video_engine.h"
#endif

#if CONFIG_APP_EVT
#include "app_event.h"
#endif

#include "bk_factory_config.h"

#if CONFIG_MOTOR
#include "motor.h"
#endif

#if CONFIG_NET_PAN
#include "bluetooth_storage.h"
#endif

/* Entity product components */
// #include "key_app_server.h" /* removed: replaced by key_app_service.h (bk_key_app component) */
#include "entity_iot_process.h"
#include "chip_bk7258.h"
#include "entity_iot_func.h"
#include <key_app_service.h>   /* provides bk_key_service_init(), bk_key_register_wakeup_source() */
#include "usr_key_cfg.h"       /* provides POWER_ON_GPIO_PIN */
/* LONG_RRESS_TIMR is defined in key_adapter.h (pulled in by key_app_service.h); define fallback here */
#ifndef LONG_RRESS_TIMR
#define LONG_RRESS_TIMR     3000
#endif

#if CONFIG_ENABLE_AGORA_DATASTREAM
#include "app_agora_session_process.h"
#endif
#include "app_rtc_facade_bridge.h"

#if CONFIG_EXT_LED_BLINK
extern void Led_Blink_Init(void);
#endif

#define TAG "ap_main"
#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)

#ifdef CONFIG_LDO3V3_ENABLE
#ifndef LDO3V3_CTRL_GPIO
#ifdef CONFIG_LDO3V3_CTRL_GPIO
#define LDO3V3_CTRL_GPIO    CONFIG_LDO3V3_CTRL_GPIO
#else
#define LDO3V3_CTRL_GPIO    GPIO_52
#endif
#endif
#endif

extern uint32_t bk_misc_get_ap_reset_reason(void);
extern uint32_t bk_misc_get_cp_reset_reason(void);
extern int cli_ota_init(void);

static const uint32_t s_user_value2 = 10;
#if CONFIG_NET_PAN
static const bt_user_storage_t s_bt_factory_storage = {0};
#endif

const struct factory_config_t s_user_config[] = {
    {"user_key1", (void *)"user_value1", 11, BK_FALSE, 0},
    {"user_key2", (void *)&s_user_value2, 4, BK_TRUE, 4},
#if CONFIG_NET_PAN
    {BT_STORAGE_KEY, (void *)&s_bt_factory_storage, sizeof(s_bt_factory_storage), BK_TRUE, sizeof(s_bt_factory_storage)},
#endif
};

static bk_err_t app_force_ldo_gpio_close(void)
{
    sys_hal_set_ana_reg18_value(0);
    sys_hal_set_ana_reg19_value(0);
    sys_hal_set_ana_reg20_value(0);
    sys_hal_set_ana_reg21_value(0);
    sys_hal_set_ana_reg27_value(0);
    sys_drv_aud_aud_en(0);
    sys_drv_aud_audbias_en(0);
    sys_drv_apll_en(0);
    gpio_dev_unmap(GPIO_50);
    gpio_dev_unmap(GPIO_52);
    gpio_dev_unmap(GPIO_10);
    gpio_dev_unmap(GPIO_11);
    gpio_dev_unmap(GPIO_0);
    gpio_dev_unmap(GPIO_1);
    gpio_dev_unmap(GPIO_9);
    return 0;
}

static void bk_enter_deepsleep(void)
{
#if CONFIG_GSENSOR_ENABLE
    extern int gsensor_enter_sleep_config(void);
    gsensor_enter_sleep_config();
    rtos_delay_milliseconds(10);
#endif
    LOGI("RESET_SOURCE_FORCE_DEEPSLEEP\r\n");
    bk_key_register_wakeup_source();
    app_force_ldo_gpio_close();
    rtos_delay_milliseconds(100);
    bk_pm_ap_sleep_mode_set(PM_MODE_FORCE_DEEP_SLEEP);
    rtos_delay_milliseconds(10);
}

static void bk_wait_power_on(void)
{
    uint32_t press_time = 0;
    uint32_t power_on_gpio = POWER_ON_GPIO_PIN;

    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();
    do {
        if (bk_gpio_get_input(power_on_gpio) == 0) {
            extern void delay_ms(uint32 num);
            delay_ms(500);
            press_time += 500;
            if (bk_gpio_get_input(power_on_gpio) != 0)
                break;
        } else {
            break;
        }
    } while (press_time < LONG_RRESS_TIMR);
    GLOBAL_INT_RESTORE();

    if (press_time < LONG_RRESS_TIMR)
        bk_enter_deepsleep();
}

int main(void)
{
    if (bk_misc_get_ap_reset_reason() != RESET_SOURCE_FORCE_DEEPSLEEP)
    {
        bk_init();
        media_service_init();

#ifdef CONFIG_LDO3V3_ENABLE
        BK_LOG_ON_ERR(gpio_dev_unmap(LDO3V3_CTRL_GPIO));
        bk_gpio_disable_pull(LDO3V3_CTRL_GPIO);
        bk_gpio_enable_output(LDO3V3_CTRL_GPIO);
        bk_gpio_set_output_high(LDO3V3_CTRL_GPIO);
#endif

        if (bk_misc_get_cp_reset_reason() == RESET_SOURCE_DEEPPS_GPIO &&
            (bk_gpio_get_wakeup_gpio_id() == POWER_ON_GPIO_PIN))
        {
#if CONFIG_MOTOR
            motor_open(PWM_MOTOR_CH_3);
#endif
            bk_wait_power_on();
#if CONFIG_MOTOR
            motor_close(PWM_MOTOR_CH_3);
#endif
        }

        bk_regist_factory_user_config((const struct factory_config_t *)&s_user_config,
                                       sizeof(s_user_config) / sizeof(s_user_config[0]));
        bk_factory_init();

#if CONFIG_EXT_LED_BLINK
        Led_Blink_Init();
#endif

        bk_pm_module_vote_cpu_freq(PM_DEV_ID_AUDIO, PM_CPU_FRQ_240M);

#if CONFIG_BK_NETWORK_TRANSFER
        ntwk_trans_init();
#endif

#if CONFIG_APP_EVT
        app_event_init();
#endif

        App_Rtc_Facade_Bridge_Init();

#if CONFIG_BK_AUDIO_ENGINE
        audio_engine_init_with_read_callback(App_Rtc_Facade_Bridge_Audio_Read_Callback, NULL);
#endif

#if CONFIG_BK_VIDEO_ENGINE
        video_engine_init();
#endif

        cli_ota_init();

        bk_key_service_init(); /* was: App_Key_Service_Init() (old ext_key_driver API) */

#if CONFIG_BAT_MONITOR
        extern void battery_monitor_init(void);
        battery_monitor_init();
#endif

#if CONFIG_USBD_MSC
        extern void msc_storage_init(void);
        msc_storage_init();
#endif

        Entity_Chip_Bk7258_Bind();
        const char *entity_missing = NULL;
        if (Entity_Iot_Func_All_Bound(&entity_missing))
        {
            LOGI("Entity BK7258 bind ready\r\n");
        }
        else
        {
            LOGE("Entity BK7258 bind missing: %s\r\n", entity_missing ? entity_missing : "?");
        }

        Entity_Iot_Sdk_Task_Start();

#if CONFIG_ENABLE_AGORA_DATASTREAM
        App_Agora_Session_Process_Init();
#endif
    }
    else
    {
        bk_init();
        bk_enter_deepsleep();
    }

    return 0;
}
