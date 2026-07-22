//app_agora_session_process.h
#pragma once

#include <common/sys_config.h>

/* 查询当前是否有 RTC facade 会话正在运行 */
unsigned char get_flag_agora_session_start(void);

#if CONFIG_ENABLE_AGORA_DATASTREAM

/* 会话处理初始化（使用 network_transfer 事件系统） */
void App_Agora_Session_Process_Init(void);

/* 对话超时定时器停止 */
void Agora_Session_Timer_Stop(void);

/* 对话超时定时器重新计数（保活） */
void Agora_Session_Timer_Reload(void);

/* 云端下发静音包数据检查；可由外部音频接收路径调用以实现保活 */
int Session_Mute_Data_Check(unsigned char *pdata, unsigned int data_len);

#endif /* CONFIG_ENABLE_AGORA_DATASTREAM */
