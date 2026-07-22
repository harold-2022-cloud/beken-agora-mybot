//entity_periph_import_interface.c
#include "entity_periph_import_interface.h"

#include "bsp_ble.h"
#include "bsp_flash.h"
#include "bsp_wifi.h"
#include "entity_ble_app.h"
#include "entity_ble_gatt.h"
#include "entity_ble_transfer_protocol.h"
#include "entity_dev_info.h"
#include "entity_http_ota.h"
#include "entity_log.h"
#include "entity_wifi.h"

int Entity_Wifi_Import_Interface_Init(void)
{
    Entity_Wifi_Cbs_t cbs = {
        .Wifi_Init = Bsp_Wifi_Init,
        .Wifi_Sta_Mode_Config = Bsp_Wifi_Sta_Mode_Config,
        .Wifi_Sta_Scan_Start = Bsp_Wifi_Sta_Scan_Start,
        .Wifi_Get_Scan_Results = Bsp_Wifi_Get_Scan_Results,
        .Wifi_Copy_Scan_Results = Bsp_Wifi_Copy_Scan_Results,
        .Wifi_Get_Macaddr = Bsp_Wifi_Get_Macaddr,
        .Wifi_Sta_Conncet = Bsp_Wifi_Sta_Conncet,
        .Wifi_Sta_Disconnect = Bsp_Wifi_Sta_Disconnect,
        .Wifi_Sta_Auto_Reconnect_Enable = Bsp_Wifi_Sta_Auto_Reconnect_Enable,
        .Register_Wifi_Event_App_Cb = Bsp_Register_Wifi_Event_App_Callback,
        .Print_Scan_Result = Bsp_Print_Scan_Result,
        .Wifi_Ap_Start = Bsp_Wifi_Ap_Start,
        .Wifi_Ap_Stop = Bsp_Wifi_Ap_Stop,
        .Wifi_Ap_Mode_Config = Bsp_Wifi_Ap_Mode_Config,
        .Wifi_Load_Signal_Level_Quality = Bsp_Wifi_Load_Signal_Level_Quality,
    };
    Entity_Wifi_Cbs_Init(&cbs);
    return 0;
}

int Entity_Ble_Import_Interface_Init(void)
{
#if CONFIG_HTTP_AGENT_ENABLE_BLE_PROVISIONING
    Entity_Ble_Cbs_t cbs = {
        .Ble_Init = Bsp_Ble_Init,
        .Ble_Gatt_Notify_Send = Bsp_Ble_Gatt_Notify_Send,
        .Ble_Gatt_Indicate_Send = Bsp_Ble_Gatt_Indicate_Send,
        .Ble_Set_Adv_Data = Bsp_Ble_Set_Adv_Data,
        .Ble_Set_Scan_Rsp_Data = Bsp_Ble_Set_Scan_Rsp_Data,
        .Ble_Adv_Start = Bsp_Ble_Adv_Start,
        .Ble_Adv_Stop = Bsp_Ble_Adv_Stop,
        .Ble_Get_Local_Addr = Bsp_Ble_Get_Local_Addr,
        .Ble_Disconnect = Bsp_Ble_Disconnect,
        .Ble_Parser_Data = Entity_Ble_V1_Parser_Data,
        .Ble_Disable = Bsp_Ble_Gatts_Disable,
    };
    Entity_Ble_Cbs_Init(&cbs);
    Entity_Ble_V1_Register_Msg_App_Process_Cb(Entity_Ble_App_Msg_Process);
    Entity_Ble_App_Register_Send_Data_Callback(Entity_Ble_V1_Send_Packet_By_Notify);
#else
    ENTITY_LOGI("[HTTP_AGENT][BLE_DISABLED] skip Entity_Ble_Import_Interface_Init\r\n");
#endif
    return 0;
}

static int Entity_Flash_Save_Triple_Info(unsigned char *pdata, unsigned int len)
{
    return Bsp_Flash_Save_Key_Value(TRIPLE_DATA_KEY, pdata, len);
}

static int Entity_Flash_Read_Triple_Info(unsigned char *pdata, unsigned int len)
{
    return Bsp_Flash_Read_Key_Value(TRIPLE_DATA_KEY, pdata, len);
}

static int Entity_Flash_Save_Config_Net_Info(unsigned char *pdata, unsigned int len)
{
    return Bsp_Flash_Save_Key_Value(CONFIG_NET_DATA_KEY, pdata, len);
}

static int Entity_Flash_Read_Config_Net_Info(unsigned char *pdata, unsigned int len)
{
    return Bsp_Flash_Read_Key_Value(CONFIG_NET_DATA_KEY, pdata, len);
}

static int Entity_Flash_Save_Thing_Model_Info(unsigned char *pdata, unsigned int len)
{
    return Bsp_Flash_Save_Key_Value(THING_MODEL_DATA_KEY, pdata, len);
}

static int Entity_Flash_Read_Thing_Model_Info(unsigned char *pdata, unsigned int len)
{
    return Bsp_Flash_Read_Key_Value(THING_MODEL_DATA_KEY, pdata, len);
}

void Entity_Flash_Import_Interface_Init(void)
{
    Entity_Flash_Cbs_t cbs = {
        .Flash_Write_Triple_Cb = Entity_Flash_Save_Triple_Info,
        .Flash_Read_Triple_Cb = Entity_Flash_Read_Triple_Info,
        .Flash_Write_Config_Net_Cb = Entity_Flash_Save_Config_Net_Info,
        .Flash_Read_Config_Net_Cb = Entity_Flash_Read_Config_Net_Info,
        .Flash_Write_Thing_Model_Info_Cb = Entity_Flash_Save_Thing_Model_Info,
        .Flash_Read_Thing_Model_Info_Cb = Entity_Flash_Read_Thing_Model_Info,
        .Flash_Read_Key_Value_Cb = Bsp_Flash_Read_Key_Value,
    };
    Entity_Flash_Cbs_Init(&cbs);
}

void Entity_Ota_Flash_Import_Interface_Init(void)
{
    Entity_Ota_Flash_Func_t func = {
        .Ota_Flash_Init = Bsp_Ota_Flash_Init,
        .Ota_Flash_Process_Data = Bsp_Ota_Flash_Process_Data,
        .Ota_Flash_Check_Crc = Bsp_Ota_Flash_Check_Crc,
        .Ota_Flash_Deinit = Bsp_Ota_Flash_Deinit,
        .Ota_Flash_Complete = Bsp_Ota_Flash_Complete,
    };
    Entity_Ota_Flash_Func_Init(&func);
}

void Entity_Periph_All_Import_Interface_Init(void)
{
    Entity_Wifi_Import_Interface_Init();
    Entity_Ble_Import_Interface_Init();
    Entity_Flash_Import_Interface_Init();
    Entity_Ota_Flash_Import_Interface_Init();
}
