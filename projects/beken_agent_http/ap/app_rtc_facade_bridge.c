#include "app_rtc_facade_bridge.h"

#include "ai_rtc_facade.h"
#include "app_event.h"
#include "audio_engine.h"
#include "entity_log.h"

#include <string.h>

static int s_rtc_facade_initialized;
static int s_rtc_session_active;
static int s_rtc_remote_joined;
static App_Rtc_Facade_Bridge_Token_Request_Cb s_token_request_cb;
static void *s_token_request_user;

static Ai_Rtc_Facade_Audio_Format_t map_audio_format(audio_enc_type_t audio_type)
{
    switch (audio_type)
    {
        case AUDIO_ENC_TYPE_G722:
            return AI_RTC_FACADE_AUDIO_FORMAT_G722;
        case AUDIO_ENC_TYPE_OPUS:
            return AI_RTC_FACADE_AUDIO_FORMAT_OPUS;
        case AUDIO_ENC_TYPE_PCM:
        default:
            return AI_RTC_FACADE_AUDIO_FORMAT_PCM16;
    }
}

static void on_rtc_state(Ai_Rtc_Facade_Event_t event,
                         Ai_Rtc_Facade_State_t state,
                         int detail,
                         void *user)
{
    (void)user;
    ENTITY_LOGI("[RTC_FACADE] event=%d state=%d detail=%d\r\n", event, state, detail);

    switch (event)
    {
        case AI_RTC_FACADE_EVENT_JOINED:
            s_rtc_session_active = 1;
            break;
        case AI_RTC_FACADE_EVENT_RECONNECTING:
            app_event_send_msg(APP_EVT_RTC_CONNECTION_LOST, 0);
            break;
        case AI_RTC_FACADE_EVENT_REJOINED:
            s_rtc_session_active = 1;
            app_event_send_msg(APP_EVT_RTC_REJOIN_SUCCESS, 0);
            break;
        case AI_RTC_FACADE_EVENT_REMOTE_USER_JOINED:
            s_rtc_remote_joined = 1;
            app_event_send_msg(APP_EVT_AGENT_JOINED, 0);
            break;
        case AI_RTC_FACADE_EVENT_REMOTE_USER_OFFLINE:
            s_rtc_remote_joined = 0;
            app_event_send_msg(APP_EVT_AGENT_OFFLINE, 0);
            break;
        case AI_RTC_FACADE_EVENT_TOKEN_WILL_EXPIRE:
            if (s_token_request_cb != NULL)
            {
                int renew_ret = s_token_request_cb(s_token_request_user);
                ENTITY_LOGI("[RTC_FACADE] token renew requested ret=%d\r\n", renew_ret);
                if (renew_ret != 0)
                {
                    app_event_send_msg(APP_EVT_RTC_CONNECTION_LOST, 0);
                }
            }
            else
            {
                ENTITY_LOGW("[RTC_FACADE] token renew callback missing\r\n");
                app_event_send_msg(APP_EVT_RTC_CONNECTION_LOST, 0);
            }
            break;
        case AI_RTC_FACADE_EVENT_TOKEN_EXPIRED:
            s_rtc_session_active = 0;
            app_event_send_msg(APP_EVT_RTC_CONNECTION_LOST, 0);
            break;
        case AI_RTC_FACADE_EVENT_FAILED:
            if (s_rtc_session_active || s_rtc_remote_joined)
            {
                app_event_send_msg(APP_EVT_RTC_CONNECTION_LOST, 0);
            }
            else
            {
                app_event_send_msg(APP_EVT_AGENT_START_FAIL, 0);
            }
            s_rtc_session_active = 0;
            s_rtc_remote_joined = 0;
            break;
        case AI_RTC_FACADE_EVENT_STOPPED:
            s_rtc_session_active = 0;
            s_rtc_remote_joined = 0;
            break;
        default:
            break;
    }
}

static int on_rtc_audio_rx(const Ai_Rtc_Facade_Audio_Frame_t *frame, void *user)
{
    static uint32_t s_rx_frame_cnt;
    int ret;

    (void)user;
    if (frame == NULL || frame->data == NULL || frame->len == 0u)
    {
        return AI_RTC_FACADE_ERR_INVALID_ARG;
    }

    s_rx_frame_cnt++;
    if (s_rx_frame_cnt == 1u || (s_rx_frame_cnt % 100u) == 0u)
    {
        ENTITY_LOGI("[RTC_FACADE_RX] frame=%u size=%u format=%d\r\n",
                    (unsigned)s_rx_frame_cnt,
                    (unsigned)frame->len,
                    frame->format);
    }

    ret = audio_engine_write_data(frame->data, (uint32_t)frame->len, 0);
    return (ret < 0) ? AI_RTC_FACADE_ERR_INTERNAL : AI_RTC_FACADE_OK;
}

static int on_rtc_datastream_rx(const Ai_Rtc_Facade_Datastream_Message_t *message, void *user)
{
    (void)user;
    if (message == NULL)
    {
        return AI_RTC_FACADE_ERR_INVALID_ARG;
    }

    ENTITY_LOGI("[RTC_FACADE_DS] stream=%d uid=%u len=%u ts=%u\r\n",
                message->stream_id,
                (unsigned)message->sender_uid,
                (unsigned)message->len,
                (unsigned)message->sent_ts);
    return AI_RTC_FACADE_OK;
}

int App_Rtc_Facade_Bridge_Init(void)
{
    const Ai_Rtc_Facade_Config_t config = {
        .join_timeout_ms = 30000,
        .enable_audio = true,
        .enable_video = false,
    };
    const Ai_Rtc_Facade_Callbacks_t callbacks = {
        .on_state = on_rtc_state,
        .on_audio_rx = on_rtc_audio_rx,
        .on_video_rx = NULL,
        .on_datastream_rx = on_rtc_datastream_rx,
        .user = NULL,
    };

    if (s_rtc_facade_initialized)
    {
        return AI_RTC_FACADE_OK;
    }

    int ret = Ai_Rtc_Facade_Init(&config, &callbacks);
    if (ret == AI_RTC_FACADE_OK)
    {
        s_rtc_facade_initialized = 1;
        ENTITY_LOGI("[RTC_FACADE] bridge initialized\r\n");
    }
    else
    {
        ENTITY_LOGE("[RTC_FACADE] bridge init failed ret=%d\r\n", ret);
    }
    return ret;
}

void App_Rtc_Facade_Bridge_Set_Token_Request_Callback(App_Rtc_Facade_Bridge_Token_Request_Cb cb,
                                                      void *user)
{
    s_token_request_cb = cb;
    s_token_request_user = user;
}

int App_Rtc_Facade_Bridge_On_Token_Result(const char *rtc_token,
                                          const char *channel_name,
                                          const char *app_id,
                                          int uid)
{
    return App_Rtc_Facade_Bridge_On_Token_Result_With_User_Account(rtc_token,
                                                                   channel_name,
                                                                   app_id,
                                                                   uid,
                                                                   NULL);
}

int App_Rtc_Facade_Bridge_On_Token_Result_With_User_Account(const char *rtc_token,
                                                            const char *channel_name,
                                                            const char *app_id,
                                                            int uid,
                                                            const char *user_account)
{
    Ai_Rtc_Facade_Token_Result_t result;
    int ret;

    if (!s_rtc_facade_initialized)
    {
        ret = App_Rtc_Facade_Bridge_Init();
        if (ret != AI_RTC_FACADE_OK)
        {
            app_event_send_msg(APP_EVT_AGENT_START_FAIL, 0);
            return ret;
        }
    }

    memset(&result, 0, sizeof(result));
    result.result = 0;
    result.rtc_token = rtc_token;
    result.channel_name = channel_name;
    result.app_id = app_id;
    result.uid = uid;
    result.user_account = (user_account != NULL && user_account[0] != '\0') ? user_account : NULL;
    result.enable_audio_ai_qos = true;

    ENTITY_LOGI("[RTC_FACADE] token result options ai_qos=%d user_account=%s\r\n",
                result.enable_audio_ai_qos ? 1 : 0,
                result.user_account != NULL ? result.user_account : "NULL");

    s_rtc_session_active = 1;
    ret = Ai_Rtc_Facade_On_Token_Result(&result);
    if (ret != AI_RTC_FACADE_OK)
    {
        ENTITY_LOGE("[RTC_FACADE] token result failed ret=%d\r\n", ret);
        s_rtc_session_active = 0;
        app_event_send_msg(APP_EVT_AGENT_START_FAIL, 0);
    }
    return ret;
}

int App_Rtc_Facade_Bridge_Stop(void)
{
    int ret;

    if (!s_rtc_facade_initialized)
    {
        s_rtc_session_active = 0;
        s_rtc_remote_joined = 0;
        return AI_RTC_FACADE_OK;
    }

    ret = Ai_Rtc_Facade_Stop();
    s_rtc_session_active = 0;
    s_rtc_remote_joined = 0;
    return ret;
}

int App_Rtc_Facade_Bridge_Stop_And_Notify(void)
{
    int ret = App_Rtc_Facade_Bridge_Stop();
    app_event_send_msg(APP_EVT_AGORA_SESSION_STOP, 0);
    return ret;
}

unsigned char App_Rtc_Facade_Bridge_Is_Active(void)
{
    return (unsigned char)(s_rtc_session_active || Ai_Rtc_Facade_Is_Joined());
}

int App_Rtc_Facade_Bridge_Send_Audio(const uint8_t *data,
                                     size_t len,
                                     audio_enc_type_t audio_type)
{
    static uint32_t s_tx_frame_cnt;
    Ai_Rtc_Facade_Audio_Frame_t frame;
    int ret;

    if (data == NULL || len == 0u)
    {
        return AI_RTC_FACADE_ERR_INVALID_ARG;
    }

    memset(&frame, 0, sizeof(frame));
    frame.data = data;
    frame.len = len;
    frame.format = map_audio_format(audio_type);
    frame.sample_rate_hz = 16000;
    frame.channels = 1;
    frame.duration_ms = 20;

    ret = Ai_Rtc_Facade_Send_Audio(&frame);
    if (ret == AI_RTC_FACADE_OK)
    {
        s_tx_frame_cnt++;
        if (s_tx_frame_cnt == 1u || (s_tx_frame_cnt % 100u) == 0u)
        {
            ENTITY_LOGI("[RTC_FACADE_TX] frame=%u size=%u format=%d\r\n",
                        (unsigned)s_tx_frame_cnt,
                        (unsigned)len,
                        frame.format);
        }
    }
    return ret;
}

int App_Rtc_Facade_Bridge_Audio_Read_Callback(unsigned char *data, unsigned int len, void *user)
{
    int ret;

    (void)user;
    if (data == NULL || len == 0u)
    {
        return AI_RTC_FACADE_ERR_INVALID_ARG;
    }

    ret = App_Rtc_Facade_Bridge_Send_Audio(data, len, audio_engine_get_encoder_type());
    return (ret == AI_RTC_FACADE_OK) ? (int)len : ret;
}
