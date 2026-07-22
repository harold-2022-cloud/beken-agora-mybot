//entity_mqtt_import_interface.c
#include "entity_mqtt_import_interface.h"

#include "entity_iot_func.h"
#include "entity_log.h"
#include "entity_mqtt_cmd_parse.h"
#include "entity_mqtt_event_respone_parse.h"
#include "entity_product_hooks.h"

static entity_ai_token_result_cb_t s_ai_token_result_cb;
static void *s_ai_token_result_user;

void Entity_Mqtt_Ai_Token_Result_Callback_Set(entity_ai_token_result_cb_t cb, void *user)
{
    s_ai_token_result_cb = cb;
    s_ai_token_result_user = user;
}

static void Entity_Mqtt_Event_Respone_Time_Parse_Import_Callback(unsigned int stamp, int zone_offset, char *sys_tz_str)
{
    ENTITY_LOGI("%s stamp=%u zone_offset=%d tz=%s\r\n",
                __FUNCTION__,
                stamp,
                zone_offset,
                (sys_tz_str != 0) ? sys_tz_str : "null");
}

static void Entity_Mqtt_Event_Respone_Model_Parse_Import_Callback(cJSON *iot_properties)
{
    (void)iot_properties;
    ENTITY_LOGI("%s\r\n", __FUNCTION__);
}

static void Entity_Mqtt_Event_Respone_Bind_Parse_Import_Callback(unsigned char bind_state)
{
    ENTITY_LOGI("%s bind_state=%u\r\n", __FUNCTION__, bind_state);
}

static void Entity_Mqtt_Event_Respone_Agora_Agent_Nfc_Parse_Import_Callback(int result,
                                                                            const char *rtcToken,
                                                                            const char *channelName,
                                                                            const char *appId,
                                                                            int uid)
{
    (void)result;
    (void)rtcToken;
    (void)channelName;
    (void)appId;
    (void)uid;
}

static void Entity_Mqtt_Event_Respone_Agora_Agent_Device_Access_Parse_Import_Callback(int result,
                                                                                      const char *rtcToken,
                                                                                      const char *channelName,
                                                                                      const char *appId,
                                                                                      int uid)
{
    entity_ai_token_result_t entity_result = {
        .result = result,
        .rtc_token = rtcToken,
        .channel_name = channelName,
        .app_id = appId,
        .uid = uid,
    };

    if (s_ai_token_result_cb != 0)
    {
        s_ai_token_result_cb(&entity_result, s_ai_token_result_user);
        return;
    }

    ENTITY_LOGW("[AI_TOKEN_CB] no product callback, result=%d uid=%d\r\n", result, uid);
}

static void Entity_Mqtt_Cmd_Reset_Parse_Import_Callback(unsigned char need_clear)
{
    (void)need_clear;
    ENTITY_LOGI("%s\r\n", __FUNCTION__);
    if (Entity_Sleep_Ms != 0)
    {
        Entity_Sleep_Ms(200);
    }
    if (Entity_System_Reset != 0)
    {
        Entity_System_Reset();
    }
}

static void Entity_Mqtt_Cmd_Ota_Parse_Import_Callback(char *firmware_url)
{
    ENTITY_LOGI("%s url=%s\r\n", __FUNCTION__, (firmware_url != 0) ? firmware_url : "null");
}

static void Entity_Mqtt_Cmd_Clean_Data_Parse_Import_Callback(void)
{
    ENTITY_LOGI("%s\r\n", __FUNCTION__);
}

static void Entity_Mqtt_Cmd_Property_Set_Parse_Import_Callback(Dp_Obj_Collect_t *dp_obj_collect)
{
    ENTITY_LOGI("%s\r\n", __FUNCTION__);
    Entity_Product_Hooks_On_Dp_Received(dp_obj_collect);
}

void Entity_Mqtt_Import_Callback_Init(void)
{
    Entity_Mqtt_Event_Respone_Parse_Cbs_t event_cbs = {
        .Event_Time_Parse_Cb = Entity_Mqtt_Event_Respone_Time_Parse_Import_Callback,
        .Event_Model_Parse_Cb = Entity_Mqtt_Event_Respone_Model_Parse_Import_Callback,
        .Event_Bind_Parse_Cb = Entity_Mqtt_Event_Respone_Bind_Parse_Import_Callback,
        .Event_Agora_Agent_Nfc_Parse_Cb = Entity_Mqtt_Event_Respone_Agora_Agent_Nfc_Parse_Import_Callback,
        .Event_Agora_Agent_Device_Access_Parse_Cb = Entity_Mqtt_Event_Respone_Agora_Agent_Device_Access_Parse_Import_Callback,
    };
    Entity_Mqtt_Event_Respone_Parse_Cbs_Init(&event_cbs);

    Entity_Mqtt_Cmd_Parse_Cbs_t cmd_cbs = {
        .Cmd_Reset_Parse_Cb = Entity_Mqtt_Cmd_Reset_Parse_Import_Callback,
        .Cmd_Ota_Parse_Cb = Entity_Mqtt_Cmd_Ota_Parse_Import_Callback,
        .Cmd_Clean_Data_Parse_Cb = Entity_Mqtt_Cmd_Clean_Data_Parse_Import_Callback,
        .Cmd_Property_Set_Parse_Cb = Entity_Mqtt_Cmd_Property_Set_Parse_Import_Callback,
    };
    Entity_Mqtt_Cmd_Parse_Cbs_Init(&cmd_cbs);
}
