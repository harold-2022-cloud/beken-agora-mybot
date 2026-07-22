//entity_mqtt_export_interface.h
#pragma once

#include <stdbool.h>

bool Entity_Device_Access_Export_Interface(void);
void Entity_Device_Access_Stop_Export_Interface(void);
void Entity_Manual_Reset_Export_Interface(unsigned char need_clear);
void Entity_Manual_Config_Net_Export_Interface(unsigned char need_clear);
