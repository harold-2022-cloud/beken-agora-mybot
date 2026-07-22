//entity_interface.c
#include "entity_interface.h"

#include "entity_log.h"
#include "entity_mqtt_import_interface.h"
#include "entity_periph_import_interface.h"
#include "entity_system_import_interface.h"

void Entity_Iot_Interface_Init(void)
{
    static unsigned char Flag_Init = 0;
    if (Flag_Init == 0)
    {
        Flag_Init = 1;
        Entity_System_All_Func_Import_Interface_Init();
        Entity_Periph_All_Import_Interface_Init();
#if CONFIG_HTTP_AGENT_ENABLE_MQTT
        Entity_Mqtt_Import_Callback_Init();
#else
        ENTITY_LOGI("[HTTP_AGENT][MQTT_DISABLED] skip Entity_Mqtt_Import_Callback_Init\r\n");
#endif
    }
}
