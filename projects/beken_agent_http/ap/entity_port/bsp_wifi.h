//bsp_wifi.h
#pragma once

#include <stdint.h>

void Bsp_Wifi_Init(uint8_t mode);
int Bsp_Wifi_Sta_Mode_Config(char *ssid, char *passwd, uint8_t auth_mode);
int Bsp_Wifi_Sta_Scan_Start(void *callback);
void *Bsp_Wifi_Get_Scan_Results(void);
int Bsp_Wifi_Copy_Scan_Results(void *result, unsigned int len);
void Bsp_Wifi_Get_Macaddr(uint8_t *mac_addr);
int Bsp_Wifi_Sta_Conncet(void);
int Bsp_Wifi_Sta_Disconnect(void);
void Bsp_Wifi_Sta_Auto_Reconnect_Enable(uint8_t enable);
void Bsp_Register_Wifi_Event_App_Callback(void *cb);
void Bsp_Print_Scan_Result(void *res);
int Bsp_Wifi_Ap_Start(void);
int Bsp_Wifi_Ap_Stop(void);
int Bsp_Wifi_Ap_Mode_Config(char *ap_ssid, char *ap_key, const char *local_ip, const char *gw_ip, const char *ip_mask);
int Bsp_Wifi_Load_Signal_Level_Quality(uint8_t *level, uint8_t *quality);
