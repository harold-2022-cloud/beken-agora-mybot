#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int Http_Wifi_Provision_Server_Start(uint8_t reset_binding);
void Http_Wifi_Provision_Server_Stop(void);
int Http_Wifi_Provision_Server_Is_Running(void);
void Http_Wifi_Provision_Server_On_Sta_Connected(void);

#ifdef __cplusplus
}
#endif
