//entity_iot_process.c
#include "entity_iot_process.h"

#include "entity_interface.h"

#include "entity_log.h"
#include "entity_iot_func.h"
#include "entity_iot_cloud.h"
#include "entity_os_system.h"
#include "entity_product_hooks.h"
#include "entity_mqtt_import_interface.h"
#include "bsp_system.h"
#include "components/system.h"
#include "os/os.h"

#include <stdio.h>
#include <string.h>

//#include "led_blink.h"
#include <key_app_service.h> /* key_event_t enums (VOLUME_UP/DOWN/SHUT_DOWN/...), bk_key_register_event_handler */
#include "bat_monitor.h"
#include "app_event.h"

#include "audio_engine.h"
#include "app_agora_session_process.h"
#include "app_rtc_facade_bridge.h"
#include "device_api_client.h"
#include "http_wifi_provision_server.h"
#if CONFIG_DUAL_SCREEN_AVI_PLAYER
#include "bk_dual_screen_avi_player.h"
#include "bk_partition.h"
#endif

#define POWER_OFF_PRESS_SECOND          6   //长按6秒关机
#define DEVICE_RESET_MULTI_COUNT        3   //设备重置，多击次数为3

static unsigned char Flag_First=true;

typedef enum
{
    ENTITY_PRODUCT_WORK_VOLUME_REPORT = 1,
    ENTITY_PRODUCT_WORK_CONFIG_NET,
    ENTITY_PRODUCT_WORK_HTTP_BIND,
    ENTITY_PRODUCT_WORK_AI_START,
    ENTITY_PRODUCT_WORK_AI_STOP,
} Entity_Product_Work_Type_e;

typedef struct
{
    Entity_Product_Work_Type_e type;
    uint32_t value;
} Entity_Product_Work_Msg_t;

static beken_queue_t s_entity_product_work_queue = NULL;
static beken_thread_t s_entity_product_work_thread = NULL;
static int s_entity_product_worker_started = 0;
static char s_http_conversation_id[96];
static int s_http_binding_done;
static int s_http_binding_in_progress;
static volatile uint32_t s_config_net_pending_need_clear;

//DP名称序号与Dp_Identifier_Array保持一致
typedef enum 
{
    DEV_DP_SWITCH_INDEX,
    DEV_DP_BATTERY_PERCENTAGE_INDEX,
    DEV_DP_VOLUME_SET_INDEX,
    DEV_DP_CHARGE_STATUS_INDEX,
}Dp_Identifier_Index_e;

//DP名称
#if CONFIG_HTTP_AGENT_ENABLE_MQTT
static Dev_Dp_Info_t Dev_Dp_Info_Array[]=
{
    {DPID_SWITCH, "switch"},
    {DPID_BATTERY_PERCENTAGE, "battery_percentage"},
    {DPID_VOLUME_SET, "volume_set"},
    {DPID_CHARGE_STATUS, "charge_status"},
};
#endif

static unsigned char Switch_Status;

void set_agora_config(const char *rtcToken, const char *channelName, const char *appId, int uid);
static void set_agora_config_with_user_account(const char *rtcToken,
                                               const char *channelName,
                                               const char *appId,
                                               int uid,
                                               const char *user_account);
static int Product_Http_Ai_Start(void);
static void Product_Http_Ai_Stop(void);
static void Product_Config_Net_Run(uint32_t need_clear);
static void Product_Http_Binding_Reset(void);
static void Product_Http_Binding_Request_Cancel(void);
static int Product_Http_Binding_Start(void);


/**
*@名称 		Dev_Dp_Obj_Type_Bool_Handle
*@功能 		bool类型的DP点设置处理
*@参数 		unsigned char dpid, unsigned char value
*@返回值 	void
*@使用说明	
*/
void Dev_Dp_Obj_Type_Bool_Handle(unsigned char dpid, unsigned char value)
{
    switch (dpid)
    {
    case DPID_SWITCH:
        Switch_Status = !Switch_Status;
        DP_Switch_Status_Report(Switch_Status);
        break;
    default:
        break;
    }
}

/**
*@名称 		Dev_Dp_Obj_Type_Enum_Handle
*@功能 		枚举类型的DP点设置处理
*@参数 		unsigned char dpid, unsigned char value
*@返回值 	void
*@使用说明	
*/
void Dev_Dp_Obj_Type_Enum_Handle(unsigned char dpid, unsigned char value)
{
    switch (dpid)
    {
    case DPID_CHARGE_STATUS:
        //
        break;
    default:
        break;
    }
}

/**
*@名称 		Dev_Dp_Obj_Type_Int_Handle
*@功能 		整数类型的DP点设置处理
*@参数 		unsigned char dpid, unsigned char value
*@返回值 	void
*@使用说明	
*/
void Dev_Dp_Obj_Type_Int_Handle(unsigned char dpid, int value)
{
    switch (dpid)
    {
        case DPID_VOLUME_SET:
            if(value >= 0 && value <= 10)
            {
                audio_engine_volume_set_abs(value);
                DP_Volume_Set_Report(value);
            }
            break;  
        default:
            break;    
    }
}

/**
*@名称 		Dev_Dp_Obj_Type_Raw_Handle
*@功能 		字节流类型的DP点设置处理
*@参数 		unsigned char dpid, unsigned char *data, unsigned short length
*@返回值 	void
*@使用说明	
*/
void Dev_Dp_Obj_Type_Raw_Handle(unsigned char dpid, unsigned char *data, unsigned short length)
{
    
}

/**
*@名称 		Dev_Dp_Obj_Type_Str_Handle
*@功能 	    字符串类型的DP点设置处理
*@参数 		unsigned char dpid, const char *value
*@返回值 	void
*@使用说明	
*/
void Dev_Dp_Obj_Type_Str_Handle(unsigned char dpid, const char *value)
{
}

/**
*@名称 		DP_Switch_Status_Report
*@功能 		开关状态上报
*@参数 		unsigned char status
*@返回值 	void
*@使用说明	
*/
void DP_Switch_Status_Report(unsigned char status)
{
#if CONFIG_HTTP_AGENT_ENABLE_MQTT
    ENTITY_LOGD("%s\r\n", __FUNCTION__);
    unsigned char dp_num=1;
    Dp_Obj_Collect_t *dp_obj_collect = Entity_Dp_Obj_Collect_Mem_Malloc(dp_num);
    if(dp_obj_collect == NULL)
    {
        return;
    }
    Entity_Dp_Bool_Frame_Load(dp_obj_collect, Dev_Dp_Info_Array[DEV_DP_SWITCH_INDEX].Dpid, Dev_Dp_Info_Array[DEV_DP_SWITCH_INDEX].Identifier, status);
    Entity_Dp_Collect_Report(dp_obj_collect);
    Entity_Dp_Obj_Collect_Mem_Free(dp_obj_collect);
#else
    (void)status;
#endif
}

/**
*@名称 		DP_Charge_Status_Report
*@功能 		充电状态上报
*@参数 		unsigned char status
*@返回值 	void
*@使用说明	
*/
void DP_Charge_Status_Report(unsigned char status)
{
#if CONFIG_HTTP_AGENT_ENABLE_MQTT
    ENTITY_LOGD("%s\r\n", __FUNCTION__);
    unsigned char dp_num=1;
    Dp_Obj_Collect_t *dp_obj_collect = Entity_Dp_Obj_Collect_Mem_Malloc(dp_num);
    if(dp_obj_collect == NULL)
    {
        return;
    }
    Entity_Dp_Bool_Frame_Load(dp_obj_collect, Dev_Dp_Info_Array[DEV_DP_CHARGE_STATUS_INDEX].Dpid, Dev_Dp_Info_Array[DEV_DP_CHARGE_STATUS_INDEX].Identifier, status);
    Entity_Dp_Collect_Report(dp_obj_collect);
    Entity_Dp_Obj_Collect_Mem_Free(dp_obj_collect);
#else
    (void)status;
#endif
}

/**
*@名称 		DP_Battery_Percent_Report
*@功能 		电池电量上报
*@参数 		unsigned char percent
*@返回值 	void
*@使用说明	
*/
void DP_Battery_Percent_Report(unsigned char percent)
{
#if CONFIG_HTTP_AGENT_ENABLE_MQTT
    ENTITY_LOGD("%s\r\n", __FUNCTION__);
    unsigned char dp_num=1;
    Dp_Obj_Collect_t *dp_obj_collect = Entity_Dp_Obj_Collect_Mem_Malloc(dp_num);
    if(dp_obj_collect == NULL)
    {
        return;
    }
    Entity_Dp_Int_Frame_Load(dp_obj_collect, Dev_Dp_Info_Array[DEV_DP_BATTERY_PERCENTAGE_INDEX].Dpid, Dev_Dp_Info_Array[DEV_DP_BATTERY_PERCENTAGE_INDEX].Identifier, percent);
    Entity_Dp_Collect_Report(dp_obj_collect);
    Entity_Dp_Obj_Collect_Mem_Free(dp_obj_collect);
#else
    (void)percent;
#endif
}

/**
*@名称 		DP_Volume_Set_Report
*@功能 		音量上报
*@参数 		unsigned char level
*@返回值 	void
*@使用说明	
*/
void DP_Volume_Set_Report(unsigned char level)
{
#if CONFIG_HTTP_AGENT_ENABLE_MQTT
    ENTITY_LOGD("%s\r\n", __FUNCTION__);
    unsigned char dp_num=1;
    Dp_Obj_Collect_t *dp_obj_collect = Entity_Dp_Obj_Collect_Mem_Malloc(dp_num);
    if(dp_obj_collect == NULL)
    {
        return;
    }
    Entity_Dp_Int_Frame_Load(dp_obj_collect, Dev_Dp_Info_Array[DEV_DP_VOLUME_SET_INDEX].Dpid, Dev_Dp_Info_Array[DEV_DP_VOLUME_SET_INDEX].Identifier, level);
    Entity_Dp_Collect_Report(dp_obj_collect);
    Entity_Dp_Obj_Collect_Mem_Free(dp_obj_collect);
#else
    (void)level;
#endif
}

/**
*@名称 		Device_Power_On_Report_Info
*@功能 		设备上电时上报相关DP点
*@参数 		void
*@返回值 	void
*@使用说明	
*/
void Device_Power_On_Report_Info(void)
{
#if CONFIG_HTTP_AGENT_ENABLE_MQTT
    ENTITY_LOGD("%s\r\n", __FUNCTION__);
    unsigned char dp_num=4;
    Dp_Obj_Collect_t *dp_obj_collect = Entity_Dp_Obj_Collect_Mem_Malloc(dp_num);
    if(dp_obj_collect == NULL)
    {
        return;
    }
    unsigned char charge_Status = battery_if_is_charging();
    unsigned char battery_percent;
    battery_get_charge_level(&battery_percent);
    unsigned char volume_level = audio_engine_volume_get_level();
    Entity_Dp_Bool_Frame_Load(dp_obj_collect, Dev_Dp_Info_Array[DEV_DP_SWITCH_INDEX].Dpid, Dev_Dp_Info_Array[DEV_DP_SWITCH_INDEX].Identifier, Switch_Status);
    Entity_Dp_Bool_Frame_Load(dp_obj_collect, Dev_Dp_Info_Array[DEV_DP_CHARGE_STATUS_INDEX].Dpid, Dev_Dp_Info_Array[DEV_DP_CHARGE_STATUS_INDEX].Identifier, charge_Status);
    Entity_Dp_Int_Frame_Load(dp_obj_collect, Dev_Dp_Info_Array[DEV_DP_BATTERY_PERCENTAGE_INDEX].Dpid, Dev_Dp_Info_Array[DEV_DP_BATTERY_PERCENTAGE_INDEX].Identifier, battery_percent);
    Entity_Dp_Int_Frame_Load(dp_obj_collect, Dev_Dp_Info_Array[DEV_DP_VOLUME_SET_INDEX].Dpid, Dev_Dp_Info_Array[DEV_DP_VOLUME_SET_INDEX].Identifier, volume_level);
    Entity_Dp_Collect_Report(dp_obj_collect);
    Entity_Dp_Obj_Collect_Mem_Free(dp_obj_collect);
#endif
}

/**
*@名称 		Dev_Dp_Obj_Callback
*@功能 	    DP点设置处理回调
*@参数 		Dp_Obj_Collect_t *Dp_Obj_Collect
*@返回值 	void
*@使用说明	
*/
void Dev_Dp_Obj_Callback(Dp_Obj_Collect_t *Dp_Obj_Collect)
{
    if(!Dp_Obj_Collect)
        return;

    for(int i=0; i<Dp_Obj_Collect->Dp_Num; i++)
    {
        Dp_Obj_t *dp_obj = &Dp_Obj_Collect->Dp_Objs[i];
        switch (dp_obj->Type)
        {
            case PROP_BOOL:
                ENTITY_LOGD("-- PROP_BOOL --:dpid:%d value:%d\r\n", dp_obj->Dpid, dp_obj->Value.Dp_Bool);
                Dev_Dp_Obj_Type_Bool_Handle(dp_obj->Dpid, dp_obj->Value.Dp_Bool);
                break;
            case PROP_INT:
                ENTITY_LOGD("-- PROP_INT --:dpid:%d value:%d\r\n", dp_obj->Dpid, dp_obj->Value.Dp_Int);
                Dev_Dp_Obj_Type_Int_Handle(dp_obj->Dpid, dp_obj->Value.Dp_Int);
                break;
            case PROP_ENUM:
                ENTITY_LOGD("-- PROP_ENUM --:dpid:%d value:%d\r\n", dp_obj->Dpid, dp_obj->Value.Dp_Enum);
                Dev_Dp_Obj_Type_Enum_Handle(dp_obj->Dpid, dp_obj->Value.Dp_Enum);
                break;
            case PROP_TEXT:
                ENTITY_LOGD("-- PROP_TEXT --:dpid:%d value:%s length:%d\r\n", dp_obj->Dpid, dp_obj->Value.Dp_Str, strlen(dp_obj->Value.Dp_Str));
                Dev_Dp_Obj_Type_Str_Handle(dp_obj->Dpid, dp_obj->Value.Dp_Str);
                break;
            case PROP_RAW:
                ENTITY_LOGD("-- PROP_RAW --:pdid:%d %d\r\n", dp_obj->Dpid, dp_obj->Len);
                Dev_Dp_Obj_Type_Raw_Handle(dp_obj->Dpid, (unsigned char*)dp_obj->Value.Dp_Raw, dp_obj->Len);
                break;
            default:
                ENTITY_LOGD( "-- PROP_Others --:dpid:%d value:%d\r\n", dp_obj->Dpid, dp_obj->Type);
                break;
        }
    }   
}

/**
*@名称 		Dev_Reset_Report
*@功能 		设备手动重置上报
*@参数 		unsigned char clearData
*@返回值 	void
*@使用说明	
*/
void Dev_Reset_Report(unsigned char clearData)
{
#if CONFIG_HTTP_AGENT_ENABLE_MQTT
    ENTITY_LOGD("%s\r\n", __FUNCTION__);
    Entity_Manual_Reset_Export_Interface(clearData);
#else
    (void)clearData;
    ENTITY_LOGI("[HTTP_AGENT][MQTT_DISABLED] skip reset report\r\n");
#endif
}

static int Entity_Product_Work_Send(Entity_Product_Work_Type_e type, uint32_t value)
{
    Entity_Product_Work_Msg_t msg = {
        .type = type,
        .value = value,
    };

    if (!s_entity_product_worker_started || s_entity_product_work_queue == NULL)
    {
        ENTITY_LOGW("[PRODUCT_WORK] drop type=%d value=%u: worker not ready\r\n", type, value);
        return -1;
    }

    if (type == ENTITY_PRODUCT_WORK_CONFIG_NET)
    {
        Product_Http_Binding_Request_Cancel();
    }

    int ret = rtos_push_to_queue(&s_entity_product_work_queue, &msg, BEKEN_NO_WAIT);
    if (ret != kNoErr)
    {
        ENTITY_LOGE("[PRODUCT_WORK] enqueue failed type=%d value=%u ret=%d\r\n", type, value, ret);
        if (type == ENTITY_PRODUCT_WORK_CONFIG_NET)
        {
            s_config_net_pending_need_clear = value ? 1u : 2u;
            ENTITY_LOGW("[PRODUCT_WORK] preserve pending config-net need_clear=%u\r\n", value);
        }
    }

    return ret;
}

static void Entity_Product_Worker_Task(void *arg)
{
    (void)arg;
    Entity_Product_Work_Msg_t msg;

    while (1)
    {
        if (s_config_net_pending_need_clear != 0)
        {
            uint32_t need_clear = (s_config_net_pending_need_clear == 1u) ? 1u : 0u;
            s_config_net_pending_need_clear = 0;
            Product_Config_Net_Run(need_clear);
            continue;
        }

        if (rtos_pop_from_queue(&s_entity_product_work_queue, &msg, BEKEN_WAIT_FOREVER) != kNoErr)
        {
            continue;
        }

        switch (msg.type)
        {
            case ENTITY_PRODUCT_WORK_VOLUME_REPORT:
                ENTITY_LOGI("[PRODUCT_WORK] volume report level=%u\r\n", msg.value);
#if CONFIG_HTTP_AGENT_ENABLE_MQTT
                DP_Volume_Set_Report((unsigned char)msg.value);
#else
                ENTITY_LOGI("[HTTP_AGENT][MQTT_DISABLED] skip volume MQTT report\r\n");
#endif
                break;
            case ENTITY_PRODUCT_WORK_CONFIG_NET:
                Product_Config_Net_Run(msg.value);
                break;
            case ENTITY_PRODUCT_WORK_HTTP_BIND:
                ENTITY_LOGI("[PRODUCT_WORK] HTTP device binding start\r\n");
                Product_Http_Binding_Start();
                break;
            case ENTITY_PRODUCT_WORK_AI_START:
                ENTITY_LOGI("[PRODUCT_WORK] ai start via HTTP\r\n");
                if (Product_Http_Ai_Start() == 0)
                {
#if CONFIG_DUAL_SCREEN_AVI_PLAYER
                    bk_dual_screen_avi_player_start(PATH_SD_FILE("genie_eye.avi"));
#endif
                }
                break;
            case ENTITY_PRODUCT_WORK_AI_STOP:
                ENTITY_LOGI("[PRODUCT_WORK] ai stop\r\n");
#if CONFIG_ENABLE_AGORA_DATASTREAM
                Agora_Session_Timer_Stop();
#endif
                Product_Http_Ai_Stop();
                App_Rtc_Facade_Bridge_Stop_And_Notify();
#if CONFIG_DUAL_SCREEN_AVI_PLAYER
                bk_dual_screen_avi_player_stop();
#endif
                break;
            default:
                ENTITY_LOGW("[PRODUCT_WORK] unknown type=%d value=%u\r\n", msg.type, msg.value);
                break;
        }
    }
}

static void Entity_Product_Worker_Start(void)
{
    if (s_entity_product_worker_started)
    {
        return;
    }

    int ret = rtos_init_queue(&s_entity_product_work_queue,
                              "entity_product_work",
                              sizeof(Entity_Product_Work_Msg_t),
                              8);
    if (ret != kNoErr)
    {
        ENTITY_LOGE("[PRODUCT_WORK] queue init failed ret=%d\r\n", ret);
        return;
    }

    ret = rtos_create_thread(&s_entity_product_work_thread,
                             4,
                             "entity_product_work",
                             Entity_Product_Worker_Task,
                             6144,
                             NULL);
    if (ret != kNoErr)
    {
        ENTITY_LOGE("[PRODUCT_WORK] thread create failed ret=%d\r\n", ret);
        rtos_deinit_queue(&s_entity_product_work_queue);
        s_entity_product_work_queue = NULL;
        return;
    }

    s_entity_product_worker_started = 1;
    ENTITY_LOGI("[PRODUCT_WORK] worker started\r\n");
}


/**
*@名称 		App_Key_Event_Process
*@功能 		
*@参数 		unsigned int event, unsigned int user_data
*@返回值 	void
*@使用说明	
*/
static void App_Key_Event_Process(uint8_t event)
{
    uint32_t run_time = Bsp_Get_Run_Time_Ms();
    ENTITY_LOGD("rtos_get_time()=%d\r\n",run_time);
    if(run_time > (8000))
    {
        Flag_First = false;
    }
    switch (event)
    {
        case VOLUME_DOWN:
        {
            audio_engine_volume_decrease();
            unsigned char volume_level = audio_engine_volume_get_level();
            Entity_Product_Work_Send(ENTITY_PRODUCT_WORK_VOLUME_REPORT, volume_level);
            break;
        }
        case VOLUME_UP:
        {
            audio_engine_volume_increase();
            unsigned char volume_level = audio_engine_volume_get_level();
            Entity_Product_Work_Send(ENTITY_PRODUCT_WORK_VOLUME_REPORT, volume_level);
            break;
        }
        case SHUT_DOWN://关机
        {
            if (run_time >= 9000)
            {
                //extern void Led_Blink_Uninit(void);
                //Led_Blink_Uninit();//清除灯光效果
                ENTITY_LOGW(" power_off\r\n");
                ENTITY_LOGW(" ************TODO:Just force deep sleep for Demo!\r\n");
                app_event_send_msg(APP_EVT_SHUT_DOWN, 0);
                Bsp_Sleep_Ms(1800);
                bk_reboot_ex(RESET_SOURCE_FORCE_DEEPSLEEP);
            }
            break;
        }

        case CONFIG_NETWORK:
        {
            app_event_send_msg(APP_EVT_NETWORK_PROVISIONING, 0);
            Entity_Product_Work_Send(ENTITY_PRODUCT_WORK_CONFIG_NET, 1);
            break;
        }
        case AI_AGENT_CONFIG:
        {
            ENTITY_LOGI("[RTC] GPIO_12 pressed: session_active=%d t=%ums\r\n",
                     get_flag_agora_session_start(), run_time);
            if(!get_flag_agora_session_start())
            {
                Entity_Product_Work_Send(ENTITY_PRODUCT_WORK_AI_START, 0);
            }
            else
            {
                Entity_Product_Work_Send(ENTITY_PRODUCT_WORK_AI_STOP, 0);
            }

            break;
        }
        // 其他事件处理...
        default:
            break;
    }
}


/**
*@名称 		App_Key_Extern_Event_Process
*@功能 		
*@参数 		unsigned int event, unsigned int user_data
*@返回值 	void
*@使用说明	
*/
static void App_Key_Extern_Event_Process(uint8_t event)
{
    switch (event)
    {
        case CONFIG_NETWORK://设备重置，重新配网
        {
            app_event_send_msg(APP_EVT_NETWORK_PROVISIONING, 0);
            Entity_Product_Work_Send(ENTITY_PRODUCT_WORK_CONFIG_NET, 1);
            break;
        }
        case AI_AGENT_CONFIG:
        {
            if(!get_flag_agora_session_start())
                Entity_Product_Work_Send(ENTITY_PRODUCT_WORK_AI_START, 0);
            else
            {
                Entity_Product_Work_Send(ENTITY_PRODUCT_WORK_AI_STOP, 0);
            }
            break;
        }
        // 其他事件处理...
        default:
            break;
    }
}


/**
*@名称 		App_Dev_Status_Callback
*@功能 		设备状态回调
*@参数 		unsigned char status
*@返回值 	void
*@使用说明	
*/
void App_Dev_Status_Callback(unsigned char status)
{
    ENTITY_LOGI("======================%s status:%d====================\r\n", __FUNCTION__, status);
    switch(status)
    {
        case DEV_UNPROVISION_STATE://配网中，WIFI还未启动连接
            app_event_send_msg(APP_EVT_NETWORK_PROVISIONING, 0);
            Entity_Product_Work_Send(ENTITY_PRODUCT_WORK_CONFIG_NET, 0);
            break;
        case DEV_WIFI_CONNECTED_STATE://WIFI获取IP成功
            app_event_send_msg(APP_EVT_RECONNECT_NETWORK_SUCCESS, 0);
            Http_Wifi_Provision_Server_On_Sta_Connected();
            Entity_Product_Work_Send(ENTITY_PRODUCT_WORK_HTTP_BIND, 0);
            //bk_sconf_trans_start();
            break;
        case DEV_WIFI_CLOUD_CONNECT_STATE:
            Device_Power_On_Report_Info();
            break;
        case DEV_WIFI_CONNECTING_STATE:
            app_event_send_msg(APP_EVT_RECONNECT_NETWORK, 0);
            break;
        case DEV_WIFI_DISCONNECT_STATE://WIFI断开连接
            app_event_send_msg(APP_EVT_RECONNECT_NETWORK_FAIL, 0);
            Entity_Device_Access_Stop_Export_Interface();
            break;
        case DEV_PROVISION_SUCCESS_STATE:
            app_event_send_msg(APP_EVT_NETWORK_PROVISIONING_SUCCESS, 0);
            break;
        case DEV_OTA_START_STATE:
            app_event_send_msg(APP_EVT_OTA_START, 0);
            break;
        case DEV_OTA_SUCCESS_STATE:
            app_event_send_msg(APP_EVT_OTA_SUCCESS, 0);
            break;
        case DEV_OTA_FAILD_STATE:
            app_event_send_msg(APP_EVT_OTA_FAIL, 0);
            break;
        case DEV_ENTER_FACTORY_TEST_STATE:
            app_event_send_msg(APP_EVT_ENTER_FACTORY_TEST, 0);
            break;
        case DEV_EXIT_FACTORY_TEST_STATE:
            app_event_send_msg(APP_EVT_EXIT_FACTORY_TEST, 0);
            break;
        default:
            break;
    }
}

#if CONFIG_HTTP_AGENT_ENABLE_MQTT
static void Entity_Ai_Token_Result_Callback(const entity_ai_token_result_t *result, void *user)
{
    (void)user;

    if (result == NULL)
    {
        app_event_send_msg(APP_EVT_AGENT_START_FAIL, 0);
        return;
    }

    if (result->result == 0)
    {
        set_agora_config(result->rtc_token, result->channel_name, result->app_id, result->uid);
        return;
    }

    ENTITY_LOGW("[RTC] Device access token failed, result=%d uid=%d\r\n", result->result, result->uid);
    app_event_send_msg(APP_EVT_AGENT_START_FAIL, 0);
}
#endif

static int request_rtc_token_renew(void *user)
{
    (void)user;
    ENTITY_LOGW("[HTTP_DEVICE_API] RTC token renew not wired in W20 POC\r\n");
    return -1;
}

static int Product_Http_Binding_Start(void)
{
    int ret = 0;

    if (s_http_binding_done)
    {
        return 0;
    }

    if (s_http_binding_in_progress)
    {
        ENTITY_LOGI("[PRODUCT_WORK] HTTP binding already in progress\r\n");
        return 0;
    }

    Device_Api_Binding_Clear_Cancel();
    s_http_binding_in_progress = 1;
    ret = Device_Api_Binding_Run();
    s_http_binding_in_progress = 0;

    if (ret == 0)
    {
        s_http_binding_done = 1;
        ENTITY_LOGI("[PRODUCT_WORK] HTTP device binding accepted\r\n");
        Http_Wifi_Provision_Server_Stop();
        return 0;
    }

    ENTITY_LOGW("[PRODUCT_WORK] HTTP device binding failed ret=%d\r\n", ret);
    return ret;
}

static void Product_Config_Net_Run(uint32_t need_clear)
{
    ENTITY_LOGI("[PRODUCT_WORK] enter AP web config need_clear=%u\r\n", need_clear);
    if (need_clear)
    {
        Product_Http_Binding_Reset();
        (void)Device_Api_Clear_Device_Token();
    }
    if (Http_Wifi_Provision_Server_Start((uint8_t)need_clear) != 0)
    {
        app_event_send_msg(APP_EVT_RECONNECT_NETWORK_FAIL, 0);
    }
}

static void Product_Http_Binding_Request_Cancel(void)
{
    Device_Api_Binding_Cancel();
    ENTITY_LOGI("[PRODUCT_WORK] HTTP binding cancel requested\r\n");
}

static void Product_Http_Binding_Reset(void)
{
    s_http_binding_done = 0;
    s_http_binding_in_progress = 0;
    ENTITY_LOGI("[PRODUCT_WORK] HTTP binding state reset\r\n");
}

static int Product_Http_Ai_Start(void)
{
    Device_Api_Conversation_Start_Result_t result;
    int ret = Product_Http_Binding_Start();

    if (ret != 0)
    {
        ENTITY_LOGE("[PRODUCT_WORK] HTTP AI start blocked by binding ret=%d\r\n", ret);
        app_event_send_msg(APP_EVT_AGENT_START_FAIL, 0);
        return ret;
    }

    ret = Device_Api_Conversation_Start(&result);

    if (ret != 0)
    {
        ENTITY_LOGE("[PRODUCT_WORK] HTTP AI start failed ret=%d\r\n", ret);
        app_event_send_msg(APP_EVT_AGENT_START_FAIL, 0);
        return ret;
    }

    snprintf(s_http_conversation_id, sizeof(s_http_conversation_id), "%s", result.conversation_id);
    ENTITY_LOGI("[PRODUCT_WORK] HTTP AI start joined conversation=%s channel=%s uid=%d user_account=%s\r\n",
                s_http_conversation_id,
                result.channel,
                result.uid,
                result.user_account);

    set_agora_config_with_user_account(result.token,
                                       result.channel,
                                       result.app_id,
                                       result.uid,
                                       result.user_account);
    return 0;
}

static void Product_Http_Ai_Stop(void)
{
    if (s_http_conversation_id[0] != '\0')
    {
        (void)Device_Api_Conversation_Stop(s_http_conversation_id);
        s_http_conversation_id[0] = '\0';
    }
}


/**
*@名称 		set_agora_config
*@功能 		接收云端返回的Agora凭证并启动会话
*@参数 		rtcToken, channelName, appId, uid
*@返回值 	void
*@使用说明	新系统由 RTC facade 接收凭证并触发会话启动
*/
void set_agora_config(const char *rtcToken, const char *channelName, const char *appId, int uid)
{
    set_agora_config_with_user_account(rtcToken, channelName, appId, uid, NULL);
}

static void set_agora_config_with_user_account(const char *rtcToken,
                                               const char *channelName,
                                               const char *appId,
                                               int uid,
                                               const char *user_account)
{
    ENTITY_LOGI("[RTC] set_agora_config: channel=%s appId=%s uid=%d token_len=%d\r\n",
             channelName ? channelName : "NULL",
             appId ? appId : "NULL",
             uid,
             rtcToken ? (int)strlen(rtcToken) : 0);

    if (!channelName || strlen(channelName) == 0) {
        ENTITY_LOGW("[RTC] WARNING: channelName is empty, cannot start agora session\r\n");
        app_event_send_msg(APP_EVT_AGENT_START_FAIL, 0);
        return;
    }

    ENTITY_LOGI("[RTC] >>> Ai_Rtc_Facade_On_Token_Result channel=%s user_account=%s\r\n",
                channelName,
                (user_account != NULL && user_account[0] != '\0') ? user_account : "NULL");
    if (App_Rtc_Facade_Bridge_On_Token_Result_With_User_Account(rtcToken,
                                                                channelName,
                                                                appId,
                                                                uid,
                                                                user_account) != 0)
    {
        app_event_send_msg(APP_EVT_AGENT_START_FAIL, 0);
    }
}

/**
*@名称 		Entity_Iot_Sdk_Task
*@功能 		Entity IOT SDK任务
*@参数 		void
*@返回值 	void
*@使用说明	
*/
void Entity_Iot_Sdk_Task(void *arg)
{
    static const Entity_Product_Hooks_t s_entity_product_hooks = {
        .on_dev_status_change = App_Dev_Status_Callback,
        .on_dp_obj_received = Dev_Dp_Obj_Callback,
    };

    ENTITY_LOGI("%s\r\n", __FUNCTION__);

    Entity_Product_Hooks_Register(&s_entity_product_hooks);
#if CONFIG_HTTP_AGENT_ENABLE_MQTT
    Entity_Mqtt_Ai_Token_Result_Callback_Set(Entity_Ai_Token_Result_Callback, NULL);
#else
    ENTITY_LOGI("[HTTP_AGENT][MQTT_DISABLED] skip MQTT AI token callback registration\r\n");
#endif
    App_Rtc_Facade_Bridge_Set_Token_Request_Callback(request_rtc_token_renew, NULL);
    Entity_Iot_Interface_Init();
#ifdef TRIPLE_TEST
    ENTITY_LOGI("-------------------------triple test!------------------\r\n", __FUNCTION__);
    Entity_Triple_Info_t test_tripl={0};
    strcpy(test_tripl.Uuid, UUID_DEFAULT);
    strcpy(test_tripl.Secret, SECRET_DEFAULT);
    strcpy(test_tripl.Mac, MAC_DEFAULT);
    Entity_Iot_System_Init(PRODUCT_KEY, PRODUCT_SECRET, DEV_VERSION, DEV_MCU_VERSION, &test_tripl);
#else
    Entity_Iot_System_Init(PRODUCT_KEY, PRODUCT_SECRET, DEV_VERSION, DEV_MCU_VERSION, NULL);
#endif
    ENTITY_LOGI("After Entity_Iot_System_Init\r\n");
    Entity_Product_Worker_Start();
    ENTITY_LOGI("After Entity_Product_Worker_Start\r\n");
    bk_key_register_event_handler(App_Key_Event_Process);//注册按键事件处理函数（替代默认的handle_system_event）
    ENTITY_LOGI("After bk_key_register_event_handler\r\n");
    //Register_Key_Event_Extern_Callback(App_Key_Extern_Event_Process);
    while(1)
    {
        Bsp_Sleep_Ms(10000);
        Bsp_Debug_Heap_Info();
    }
    rtos_delete_thread(NULL);
}

/**
*@名称 		Entity_Iot_Sdk_Task_Start
*@功能 		启动 Entity IOT SDK任务
*@参数 		void
*@返回值 	void
*@使用说明	
*/
void Entity_Iot_Sdk_Task_Start(void)
{
    int ret;
    void *config_thread_handle = NULL;
    ENTITY_LOGI("=================build data:%s time:%s===================\r\n", __DATE__, __TIME__);
    ret = Bsp_Pthread_Create(&config_thread_handle, "wifi_config_network", 4096, 4, Entity_Iot_Sdk_Task, NULL);
    if (ret != 0)
    {
        ENTITY_LOGE("wifi config network task fail: %d\r\n", ret);
    }
}
  
