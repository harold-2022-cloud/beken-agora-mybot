#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <components/system.h>
#include <os/os.h>
#include <os/mem.h>
#include <os/str.h>
#include "network_transfer.h"
#include "cli.h"
#if CONFIG_VOLC_RTC_EN
#include "bk_volc_api.h"
#elif CONFIG_AGORA_IOT_SDK
#include "ai_rtc_facade.h"
#include <driver/h264_types.h>
#endif
#if CONFIG_BK_AUDIO_ENGINE
#include "audio_engine.h"
#endif
#if CONFIG_BK_VIDEO_ENGINE
#include "video_engine.h"
#endif

#define TAG "ntwk_trans"
#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

/**
 * @brief 网络传输模块全局上下文实例
 */
static ntwk_trans_ctx_t g_ntwk_trans_ctx = {0};

#if CONFIG_AGORA_IOT_SDK
static Ai_Rtc_Facade_Audio_Format_t ntwk_trans_agora_audio_format(audio_enc_type_t audio_type)
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

static int ntwk_trans_agora_facade_audio_send(uint8_t *data_ptr, size_t data_len, audio_enc_type_t audio_type)
{
    static uint32_t s_tx_frame_cnt;
    Ai_Rtc_Facade_Audio_Frame_t frame = {
        .data = data_ptr,
        .len = data_len,
        .format = ntwk_trans_agora_audio_format(audio_type),
        .sample_rate_hz = 16000,
        .channels = 1,
        .duration_ms = 20,
        .timestamp_ms = 0,
    };

    int ret = Ai_Rtc_Facade_Send_Audio(&frame);
    if (ret == AI_RTC_FACADE_OK)
    {
        s_tx_frame_cnt++;
        if (s_tx_frame_cnt == 1u || (s_tx_frame_cnt % 100u) == 0u)
        {
            LOGI("[RTC_FACADE_TX] frame=%u size=%u format=%d\n",
                 (unsigned)s_tx_frame_cnt,
                 (unsigned)data_len,
                 frame.format);
        }
    }
    return (ret == AI_RTC_FACADE_OK) ? (int)data_len : ret;
}

static int ntwk_trans_agora_facade_video_send(frame_buffer_t *frame)
{
    Ai_Rtc_Facade_Video_Frame_t video;
    int ret;

    if (frame == NULL || frame->frame == NULL || frame->length == 0)
    {
        return AI_RTC_FACADE_ERR_INVALID_ARG;
    }

    memset(&video, 0, sizeof(video));
    video.data = (const uint8_t *)frame->frame;
    video.len = (size_t)frame->length;
    video.width = frame->width;
    video.height = frame->height;
    video.frame_rate_hz = 0;
    video.timestamp_ms = frame->timestamp;

    if (frame->fmt == PIXEL_FMT_H264)
    {
        video.format = AI_RTC_FACADE_VIDEO_FORMAT_H264;
        if ((frame->h264_type & (1 << H264_NAL_I_FRAME)) != 0)
        {
            video.flags |= AI_RTC_FACADE_VIDEO_FLAG_KEYFRAME;
        }
    }
    else if (frame->fmt == PIXEL_FMT_JPEG)
    {
        video.format = AI_RTC_FACADE_VIDEO_FORMAT_JPEG;
        video.flags |= AI_RTC_FACADE_VIDEO_FLAG_KEYFRAME;
    }
    else
    {
        return AI_RTC_FACADE_ERR_UNSUPPORTED;
    }

    ret = Ai_Rtc_Facade_Send_Video(&video);
    return (ret == AI_RTC_FACADE_OK) ? (int)frame->length : ret;
}

static bk_err_t ntwk_trans_agora_facade_start(void *user_data)
{
    (void)user_data;
    return BK_OK;
}

static bk_err_t ntwk_trans_agora_facade_stop(void *user_data)
{
    (void)user_data;
    return (Ai_Rtc_Facade_Stop() == AI_RTC_FACADE_OK) ? BK_OK : BK_FAIL;
}

static bk_err_t ntwk_trans_agora_facade_pre_config(void *user_data)
{
    (void)user_data;
    return BK_OK;
}
#endif

int ntwk_trans_update(void *user_data, void *update_info)
{
    int ret = 0;

    if (!g_ntwk_trans_ctx.initialized) {
        LOGW("Network transfer not initialized\n");
        return -1;
    }
    if (g_ntwk_trans_ctx.update_cb) {
        ret = g_ntwk_trans_ctx.update_cb(user_data, update_info);
    }
    if (ret != 0) {
        LOGE("Failed to update network transfer\n");
        return ret;
    }
    LOGI("Network transfer updated\n");
    return 0;
}
/**
 * @brief 启动网络传输
 * @param user_data 用户数据指针，传递给启动回调函数
 * @return int 0表示成功，负数表示失败
 */
int ntwk_trans_start(void *user_data)
{
    int ret = 0;
    if (!g_ntwk_trans_ctx.initialized) {
        LOGW("Network transfer not initialized\n");
        return -1;
    }

    if (g_ntwk_trans_ctx.is_started) {
        LOGW("Network transfer already started\n");
        return 0;
    }
    
    // 调用启动回调函数
    if (g_ntwk_trans_ctx.start_cb) {
        ret = g_ntwk_trans_ctx.start_cb(user_data);
    }
    if (ret != 0) {
        LOGE("Failed to start network transfer\n");
        return ret; 
    }

    g_ntwk_trans_ctx.is_started = true;
    LOGI("Network transfer started\n");
    
    return 0;
}
/**
 * @brief 停止网络传输
 * @param user_data 用户数据指针，传递给停止回调函数
 * @return int 0表示成功，负数表示失败
 */
int ntwk_trans_stop(void *user_data)
{
    int ret = 0;

    if (!g_ntwk_trans_ctx.initialized) {
        LOGW("Network transfer not initialized\n");
        return -1;
    }

    if (!g_ntwk_trans_ctx.is_started) {
        LOGW("Network transfer not started\n");
        return -1;
    }

    // 调用停止回调函数
    if (g_ntwk_trans_ctx.stop_cb) {
        ret = g_ntwk_trans_ctx.stop_cb(user_data);
    }

    if (ret != 0) {
        LOGE("Failed to stop network transfer\n");
        return ret;
    }

    g_ntwk_trans_ctx.is_started = false;
    LOGI("Network transfer stopped\n");
    
    return 0;
}
/**
 * @brief 初始化网络传输模块
 * @return int 0表示成功，负数表示失败
 */
int ntwk_trans_init(void)
{
    int ret = 0;

    if (g_ntwk_trans_ctx.initialized) {
        LOGW("Network transfer already initialized\n");
        return 0;
    }
    
    // 根据配置选择RTC后端并设置相应的回调函数
    #if CONFIG_VOLC_RTC_EN
    g_ntwk_trans_ctx.audio_tx_cb = bk_byte_rtc_audio_data_send;
    g_ntwk_trans_ctx.video_tx_cb = bk_byte_rtc_video_data_send;  // 设置底层视频发送回调
    g_ntwk_trans_ctx.start_cb = bk_byte_start;
    g_ntwk_trans_ctx.stop_cb = bk_byte_stop;
    g_ntwk_trans_ctx.pre_config_cb = bk_byte_pre_config;
    g_ntwk_trans_ctx.update_cb = bk_byte_update_agent;
    g_ntwk_trans_ctx.network_type = NETWORK_TYPE_VOLC_RTC;
    #elif CONFIG_AGORA_IOT_SDK
    // Agora RTC is driven by the SDK RTC facade; keep this API as the audio-engine bridge.
    g_ntwk_trans_ctx.audio_tx_cb = ntwk_trans_agora_facade_audio_send;
    g_ntwk_trans_ctx.video_tx_cb = ntwk_trans_agora_facade_video_send;
    g_ntwk_trans_ctx.start_cb = ntwk_trans_agora_facade_start;
    g_ntwk_trans_ctx.stop_cb = ntwk_trans_agora_facade_stop;
    g_ntwk_trans_ctx.pre_config_cb = ntwk_trans_agora_facade_pre_config;
    g_ntwk_trans_ctx.update_cb = NULL;
    g_ntwk_trans_ctx.network_type = NETWORK_TYPE_AGORA_RTC;
    #endif

    // 执行预配置回调
    if (g_ntwk_trans_ctx.pre_config_cb) {
        ret = g_ntwk_trans_ctx.pre_config_cb(g_ntwk_trans_ctx.user_data);
    }

    if (ret != 0) {
        LOGE("Failed to initialize network transfer\n");
        return ret;
    }
    
    g_ntwk_trans_ctx.initialized = true;
    LOGI("Network transfer initialized with network type: %d\n", g_ntwk_trans_ctx.network_type);
    
    return 0;
}

/**
 * @brief 反初始化网络传输模块
 * @return int 0表示成功，负数表示失败
 */
int ntwk_trans_deinit(void)
{
    if (!g_ntwk_trans_ctx.initialized) {
        LOGW("Network transfer not initialized\n");
        return 0;
    }

    if (g_ntwk_trans_ctx.is_started && g_ntwk_trans_ctx.stop_cb) {
        int ret = g_ntwk_trans_ctx.stop_cb(g_ntwk_trans_ctx.user_data);
        if (ret != 0)
            LOGW("stop_cb failed during deinit (ret=%d), proceeding\n", ret);
    }

    // 清空全局上下文
    os_memset(&g_ntwk_trans_ctx, 0, sizeof(ntwk_trans_ctx_t));
    LOGI("Network transfer deinitialized\n");

    return 0;
}

/**
 * @brief 发送音频数据到网络
 * @param data 音频数据指针
 * @param size 音频数据大小
 * @param audio_type 音频编码类型
 * @return int 0表示成功，负数表示失败
 */
int ntwk_trans_send_audio(const uint8_t *data, size_t size, audio_enc_type_t audio_type)
{
    int ret = 0;

    if (!g_ntwk_trans_ctx.initialized) {
        LOGE("Network transfer not initialized\n");
        return -1;
    }
    
    // 参数校验
    if (!data || size == 0) {
        LOGE("Invalid audio data parameters\n");
        return -2;
    }
    
    if (audio_type == AUDIO_ENC_TYPE_INVALID) {
        LOGE("Invalid audio type\n");
        return -3;
    }

    // 调用音频发送回调函数
    if (g_ntwk_trans_ctx.audio_tx_cb) {
        ret = g_ntwk_trans_ctx.audio_tx_cb((uint8_t *)data, size, audio_type);
    }

    if (ret < 0) {
        //LOGE("Failed to send audio data, ret:%d\n", ret);
        return ret;
    }

    return ret;
}


/**
 * @brief 发送视频数据到网络
 * @param data 视频数据指针
 * @param size 视频数据大小
 * @param video_type 视频类型
 * @return int 0表示成功，负数表示失败
 */
int ntwk_trans_send_video(frame_buffer_t *frame)
{
    int ret = 0;

    if (!g_ntwk_trans_ctx.initialized) {
        LOGE("Network transfer not initialized\n");
        return -1;
    }
    
    // 参数校验
    if (!frame || !frame->frame || frame->length == 0) {
        LOGE("Invalid video frame parameters\n");
        return -2;
    }

    // 调用视频发送回调函数
    if (g_ntwk_trans_ctx.video_tx_cb) {
        ret = g_ntwk_trans_ctx.video_tx_cb(frame);
    } else {
        LOGW("video_tx_cb not set, video data dropped\n");
        return -4;
    }

    if (ret < 0) {
        //LOGE("Failed to send video data, ret:%d\n", ret);
        return ret;
    }

    return ret;
}


/**
 * @brief 获取当前网络传输类型
 * @return network_type_t 网络类型枚举值
 */
network_type_t ntwk_trans_get_network_type(void)
{
    return g_ntwk_trans_ctx.network_type;
}

/**
 * @brief 接收音频数据并写入音频引擎
 * @param data 音频数据指针
 * @param size 音频数据大小
 * @return int 0表示成功，负数表示失败
 */
int ntwk_trans_recv_audio(const uint8_t *data, size_t size)
{
    if (!g_ntwk_trans_ctx.initialized) {
        LOGE("Network transfer not initialized\n");
        return -1;
    }

    /* Drop incoming audio immediately after ntwk_trans_stop() so the speaker
     * goes silent without waiting for the Agora thread to finish cleanup. */
    if (!g_ntwk_trans_ctx.is_started) {
        return 0;
    }

    // 参数校验
    if (!data || size == 0) {
        LOGE("Invalid audio data parameters\n");
        return -2;
    }

    // 如果音频引擎已启用，则将数据写入音频引擎
    #if CONFIG_BK_AUDIO_ENGINE
    static uint32_t s_rx_frame_cnt = 0;
    s_rx_frame_cnt++;
    if (s_rx_frame_cnt == 1 || (s_rx_frame_cnt % 100) == 0) {
        LOGI("[ntwk_rx] frame=%u size=%u\n", s_rx_frame_cnt, (unsigned)size);
    }
    int ret = audio_engine_write_data(data, size, 0);
    if (ret < 0 && (s_rx_frame_cnt == 1 || (s_rx_frame_cnt % 100) == 0)) {
        LOGI("[ntwk_rx] audio_engine_write_data ret=%d\n", ret);
    }
    return ret;
    #else
    LOGE("Audio engine not enabled\n");
    return 0;
    #endif
}
/**
 * @brief 获取音频编码器类型
 * @return audio_enc_type_t 音频编码器类型枚举值
 */
audio_enc_type_t ntwk_trans_get_audio_encoder_type(void)
{
    // 如果音频引擎已启用，则从音频引擎获取编码器类型
    #if CONFIG_BK_AUDIO_ENGINE
    return audio_engine_get_encoder_type();
    #else
    LOGW("Audio engine not enabled\n");
    return AUDIO_ENC_TYPE_INVALID;
    #endif
}
