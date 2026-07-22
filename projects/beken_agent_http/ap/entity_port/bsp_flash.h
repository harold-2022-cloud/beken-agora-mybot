//bsp_flash.h
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TRIPLE_DATA_KEY         "triple_data"
#define CONFIG_NET_DATA_KEY     "config_net_data"
#define THING_MODEL_DATA_KEY    "thing_model_data"

int Bsp_Flash_Save_Key_Value(const char *key, unsigned char *pdata, unsigned int len);
int Bsp_Flash_Read_Key_Value(const char *key, unsigned char *pdata, unsigned int len);
int Bsp_Flash_Delete_Key(const char *key);
int Bsp_Flash_Reset_Env_To_Default(void);

int Bsp_Ota_Flash_Init(void);
int Bsp_Ota_Flash_Process_Data(unsigned char *buf, uint16_t len, uint32_t total);
int Bsp_Ota_Flash_Check_Crc(uint32_t in_crc);
int Bsp_Ota_Flash_Deinit(void);
int Bsp_Ota_Flash_Complete(void);

#ifdef __cplusplus
}
#endif
