//entity_mqtt_export_interface.c
#include "entity_mqtt_export_interface.h"

#include "entity_log.h"
#include "entity_mqtt_app.h"
#include "entity_mqtt_event_report.h"
#include "entity_os_system.h"

static const char *Entity_Device_Access_Get_Language(void)
{
    return "ch";
}

bool Entity_Device_Access_Export_Interface(void)
{
    char pending_id[64] = {0};
    uint64_t pending_pub_ms = 0;

    ENTITY_LOGI("[DEVICE_ACCESS] request start\r\n");
    if (!Entity_Mqtt_App_Is_Connected())
    {
        ENTITY_LOGW("[DEVICE_ACCESS] skip: mqtt not connected\r\n");
        return false;
    }

    if (!Entity_Mqtt_App_Prepare_Ai_Publish())
    {
        ENTITY_LOGW("[DEVICE_ACCESS] skip: mqtt not ready for ai publish\r\n");
        return false;
    }

    if (Entity_Mqtt_Get_Token_Pending_Snapshot(pending_id, sizeof(pending_id), &pending_pub_ms))
    {
        ENTITY_LOGW("[DEVICE_ACCESS] skip: token pending id=%s pub_ms=%llu\r\n",
                    pending_id,
                    (unsigned long long)pending_pub_ms);
        return false;
    }

    Mqtt_Event_Agora_Agent_Device_Access_Report(1, NULL, Entity_Device_Access_Get_Language());
    ENTITY_LOGI("[DEVICE_ACCESS] request submitted\r\n");
    return true;
}

void Entity_Device_Access_Stop_Export_Interface(void)
{
}

void Entity_Manual_Reset_Export_Interface(unsigned char need_clear)
{
    Entity_Manual_Reset_Process(need_clear);
}

void Entity_Manual_Config_Net_Export_Interface(unsigned char need_clear)
{
    Entity_Manual_Config_Net_Process(need_clear);
}
