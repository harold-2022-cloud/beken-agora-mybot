//bsp_ble.h
#pragma once

void Bsp_Ble_Init(void);
int Bsp_Ble_Gatt_Notify_Send(unsigned char conn_index, unsigned char *send_data, unsigned short len);
int Bsp_Ble_Gatt_Indicate_Send(unsigned char conn_index, unsigned char *send_data, unsigned short len);
int Bsp_Ble_Set_Adv_Data(unsigned char *adv_data, unsigned char adv_len);
int Bsp_Ble_Set_Scan_Rsp_Data(unsigned char *scan_rsp_data, unsigned char scan_rsp_len);
int Bsp_Ble_Adv_Start(void);
int Bsp_Ble_Adv_Stop(void);
void Bsp_Ble_Get_Local_Addr(unsigned char *mac_addr);
int Bsp_Ble_Disconnect(unsigned char conn_index);
void Bsp_Ble_Gatts_Disable(void);

int Bsp_Ble_Set_Device_Name(const char *name);
