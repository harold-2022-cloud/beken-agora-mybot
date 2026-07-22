//app_agora_session_process.c
#include <stdbool.h>
#include "app_agora_session_process.h"
#include "app_rtc_facade_bridge.h"

/**
 * @名称    get_flag_agora_session_start
 * @功能    查询 Agora 会话是否正在运行（兼容旧版 app_event.h 中的同名函数）
 * @返回值  1-会话中  0-未启动
 */
unsigned char get_flag_agora_session_start(void)
{
    return App_Rtc_Facade_Bridge_Is_Active();
}

#if CONFIG_ENABLE_AGORA_DATASTREAM

#include "entity_log.h"
#include "bsp_system.h"
#include "app_event.h"

#define SESSION_AUTO_STOP_TIME_PERIOD   180000

static void *Auto_Stop_Session_Timer;

/**
 * @名称    Session_Timeout_Callback
 * @功能    对话超时定时器回调，发送会话停止事件并停止网络传输
 */
static void Session_Timeout_Callback(void *arg)
{
    ENTITY_LOGI("[session] %s\r\n", __func__);
    App_Rtc_Facade_Bridge_Stop();
    app_event_send_msg(APP_EVT_AGORA_SESSION_STOP, 0);
}

/**
 * @名称    Agora_Session_Timer_Reload
 * @功能    定时器重新计数（会话保活）
 */
void Agora_Session_Timer_Reload(void)
{
    ENTITY_LOGI("[session] %s session timeout check start\r\n", __func__);
    if(Auto_Stop_Session_Timer == NULL)
    {
        Auto_Stop_Session_Timer = Bsp_Timer_Create(0, SESSION_AUTO_STOP_TIME_PERIOD, Session_Timeout_Callback, NULL);
        Bsp_Timer_Start(0, Auto_Stop_Session_Timer);
    }
    else
    {
        Bsp_Timer_Reload(0, Auto_Stop_Session_Timer);
    }
}

/**
 * @名称    Agora_Session_Timer_Stop
 * @功能    对话超时定时器停止
 */
void Agora_Session_Timer_Stop(void)
{
    ENTITY_LOGI("[session] %s session timeout check stop\r\n", __func__);
    if (Auto_Stop_Session_Timer != NULL)
    {
        Bsp_Timer_Stop(0, Auto_Stop_Session_Timer);
    }
}

/**
 * @名称    Session_App_Event_Handler
 * @功能    监听网络传输事件，用于管理会话超时定时器
 *          - AGENT_JOINED : 云端 Agent 就绪，启动超时倒计时
 *          - AGENT_OFFLINE / AGORA_SESSION_STOP : 会话结束，停止倒计时
 */
static void Session_App_Event_Handler(app_evt_msg_t *msg, void *user_data)
{
    switch(msg->event)
    {
        case APP_EVT_AGENT_JOINED:
            /* Agent 上线 → 等待用户说话，开始超时倒计时 */
            Agora_Session_Timer_Reload();
            break;
        case APP_EVT_AGENT_OFFLINE:
        case APP_EVT_AGORA_SESSION_STOP:
            /* 会话已结束，停止倒计时；確保 RTC 已關閉（幂等，若已停止則無操作） */
            Agora_Session_Timer_Stop();
            App_Rtc_Facade_Bridge_Stop();
            break;
        default:
            break;
    }
}

/**
 * @名称    Session_Mute_Data_Check
 * @功能    云端下发静音包数据检查
 * @参数    unsigned char *pdata, unsigned int data_len
 * @返回值  int  1-是静音  0-非静音  -1-未到检测周期
 * @使用说明 由外部音频接收路径调用（如 entity_iot_process.c）以实现保活
 */
int Session_Mute_Data_Check(unsigned char *pdata, unsigned int data_len)
{
    #define MUTE_CHECK_PERIOD       (48/2)
    static unsigned int count=0;
    static unsigned char flag_mute=0;
    static unsigned int mute_count=0;
    ++count;
    if(count < MUTE_CHECK_PERIOD)
        return -1;
    count = 0;
    unsigned char flag_mute_data=0;
    unsigned char mute_array[8]={0x01, 0x00, 0xfe, 0xff, 0xfd, 0xff, 0xff, 0xff};
    unsigned int pattern_len = sizeof(mute_array);
    unsigned int end = data_len - pattern_len - 1;
    for(int i=0; i<end; i++)
    {
        if(memcmp(pdata+i, mute_array, pattern_len) == 0)
        {
            flag_mute_data = 1;
            break;
        }
    }
    if(flag_mute == 0)//当前不是静音状态
    {
        if(flag_mute_data)//检测到静音数据
        {
            if(++mute_count >= 2)
            {
                mute_count = 0;
                flag_mute = 1;//静音状态
                ENTITY_LOGI("[session] %s, is mute state\r\n", __func__);
            }
        }
        else //检测到非静音数据（AI 正在说话）→ 保活
        {
            mute_count = 0;
            Agora_Session_Timer_Reload();
        }
    }
    else //当前是静音状态
    {
        if(!flag_mute_data)//检测到非静音数据
        {
            if(++mute_count >= 4)
            {
                mute_count = 0;
                flag_mute = 0;//非静音状态
                ENTITY_LOGI("[session] %s, not mute state\r\n", __func__);
                Agora_Session_Timer_Reload();
            }
        }
        else //检测到静音数据
        {
            mute_count = 0;
        }
    }
    return 0;
}

/**
 * @名称    App_Agora_Session_Process_Init
 * @功能    会话处理初始化
 *          使用 network_transfer 事件系统替代旧 Agora DataStream 回调
 */
void App_Agora_Session_Process_Init(void)
{
    Auto_Stop_Session_Timer = Bsp_Timer_Create(0, SESSION_AUTO_STOP_TIME_PERIOD, Session_Timeout_Callback, NULL);

    /* 注册 app 事件监听：Agent 上线/下线 → 管理会话定时器 */
    app_event_register_handler(APP_EVT_AGENT_JOINED,        Session_App_Event_Handler, NULL);
    app_event_register_handler(APP_EVT_AGENT_OFFLINE,       Session_App_Event_Handler, NULL);
    app_event_register_handler(APP_EVT_AGORA_SESSION_STOP,  Session_App_Event_Handler, NULL);
}

#endif /* CONFIG_ENABLE_AGORA_DATASTREAM */
