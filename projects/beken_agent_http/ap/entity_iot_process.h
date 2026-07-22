//entity_iot_process.h
#pragma once

#include "entity_mqtt_export_interface.h"
#include "entity_mqtt_dev_dp.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>




/* module version*/
#define DEV_VERSION             "1.0.0" //20260117
//#define DEV_VERSION             "1.0.1" //20260209
/* mcu version */
#define DEV_MCU_VERSION         DEV_VERSION


/*
 * Public POC note:
 * MQTT is disabled by default in this project. Keep these values as placeholders
 * unless you explicitly enable CONFIG_HTTP_AGENT_ENABLE_MQTT for your own cloud.
 */
#define TRIPLE_TEST



#define PRODUCT_KEY     "REPLACE_WITH_PRODUCT_KEY"
#define PRODUCT_SECRET  "REPLACE_WITH_PRODUCT_SECRET"


//AI
#ifdef TRIPLE_TEST
#define AI_TRIPLE_USE   1
#if (AI_TRIPLE_USE == 1)//
#define UUID_DEFAULT    "REPLACE_WITH_DEVICE_UUID"
#define SECRET_DEFAULT  "REPLACE_WITH_DEVICE_SECRET"
#define MAC_DEFAULT     "REPLACE_WITH_DEVICE_MAC"
#elif (AI_TRIPLE_USE == 2)//
#define UUID_DEFAULT    "REPLACE_WITH_DEVICE_UUID_2"
#define SECRET_DEFAULT  "REPLACE_WITH_DEVICE_SECRET_2"
#define MAC_DEFAULT     "REPLACE_WITH_DEVICE_MAC_2"
#endif
#endif


//DPID即bssid，需要根据设备实际物模型来填充，不同的DPID对应不同的功能。
typedef enum
{
    DPID_SWITCH             =1,        //开关 switch bool
    DPID_BATTERY_PERCENTAGE =2,       //电量 battery_percentage int
    DPID_VOLUME_SET         =3,       //音量 volume_set int
    DPID_CHARGE_STATUS      =4,       //充电状态 charge_status enum
}Device_Dpid_e;

typedef struct 
{
    unsigned char Dpid;
    const char *Identifier;
}Dev_Dp_Info_t;


//启动 Entity IOT SDK任务
void Entity_Iot_Sdk_Task_Start(void);

//  DP点设置处理回调
void Dev_Dp_Obj_Callback(Dp_Obj_Collect_t *Dp_Obj_Collect);

void DP_Charge_Status_Report(unsigned char status);

void DP_Battery_Percent_Report(unsigned char percent);

void DP_Volume_Set_Report(unsigned char level);

//开关状态上报
void DP_Switch_Status_Report(unsigned char status);

//设备手动重置上报
void Dev_Reset_Report(unsigned char clearData);

//设备上电时上报相关DP点
void Device_Power_On_Report_Info(void);

//设备状态回调
void App_Dev_Status_Callback(unsigned char status);

void DP_Volume_Set_Report(unsigned char level);
