#include "device_api_client.h"

#include "cJSON.h"
#include "entity_network.h"
#include "entity_log.h"
#if CONFIG_HTTP_AGENT_ENABLE_BLE_PROVISIONING
#include "entity_ble_transfer_protocol.h"
#endif
#include "bsp_wifi.h"
#include "bsp_flash.h"
#include "os/os.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef DEVICE_API_DEFAULT_BASE_URL
#define DEVICE_API_DEFAULT_BASE_URL "http://mybot.sg3.agoralab.co/api"
#endif
#ifndef DEVICE_API_DEFAULT_DEVICE_ID
#define DEVICE_API_DEFAULT_DEVICE_ID "AG-BK7258-POC"
#endif
#ifndef DEVICE_API_DEFAULT_DEVICE_TOKEN
#define DEVICE_API_DEFAULT_DEVICE_TOKEN ""
#endif

#define DEVICE_API_HTTP_TIMEOUT_MS 10000U
#define DEVICE_API_URL_MAX 320
#define DEVICE_API_PATH_MAX 192
#define DEVICE_API_HOST_MAX 96
#define DEVICE_API_BODY_MAX 256
#define DEVICE_API_REQUEST_MAX 1536
#define DEVICE_API_RESPONSE_MAX 4096
#define DEVICE_API_PAIR_POLL_MAX_SECONDS 300
#define DEVICE_API_PAIR_DEFAULT_POLL_SECONDS 3
#define DEVICE_API_DEVICE_TOKEN_KEY "http_device_token"

static char s_base_url[160] = DEVICE_API_DEFAULT_BASE_URL;
static char s_device_id[80] = DEVICE_API_DEFAULT_DEVICE_ID;
static char s_device_token[512] = DEVICE_API_DEFAULT_DEVICE_TOKEN;
static char s_last_conversation_id[96];
static char s_http_request[DEVICE_API_REQUEST_MAX];
static char s_http_response[DEVICE_API_RESPONSE_MAX];
static bool s_device_id_explicit;
static bool s_device_token_loaded;

typedef struct
{
    char code[16];
    char pair_token[512];
    int expires_in_seconds;
    int poll_after_seconds;
} Device_Api_Pair_Code_Result_t;

typedef struct
{
    char status[24];
    char device_token[512];
    int poll_after_seconds;
} Device_Api_Binding_Status_Result_t;

static Device_Api_Pair_Code_Result_t s_pending_pair;
static bool s_pending_pair_valid;
static volatile bool s_binding_cancel_requested;

static void Device_Api_Notify_Pair_Code_Ble(const Device_Api_Pair_Code_Result_t *pair)
{
#if CONFIG_HTTP_AGENT_ENABLE_BLE_PROVISIONING
    cJSON *root = NULL;
    cJSON *data = NULL;
    char *json = NULL;
    int ret = -1;

    if (pair == NULL || pair->code[0] == '\0')
    {
        return;
    }

    root = cJSON_CreateObject();
    data = cJSON_CreateObject();
    if (root == NULL || data == NULL)
    {
        cJSON_Delete(root);
        cJSON_Delete(data);
        ENTITY_LOGE("[HTTP_DEVICE_API] pair-code BLE notify json alloc failed\r\n");
        return;
    }

    cJSON_AddStringToObject(root, "type", "thing.network.set.response");
    cJSON_AddNumberToObject(root, "code", 0);
    cJSON_AddStringToObject(data, "pair_code", pair->code);
    cJSON_AddStringToObject(data, "device_id", s_device_id);
    cJSON_AddNumberToObject(data, "expires_in", pair->expires_in_seconds);
    cJSON_AddNumberToObject(data, "poll_after", pair->poll_after_seconds);
    cJSON_AddItemToObject(root, "data", data);
    data = NULL;

    json = cJSON_PrintUnformatted(root);
    if (json != NULL)
    {
        ret = Entity_Ble_V1_Send_Packet_By_Notify((unsigned char *)json, (unsigned short)strlen(json));
        ENTITY_LOGI("[HTTP_DEVICE_API] pair-code BLE notify pair_code=%s device=%s len=%u ret=%d\r\n",
                    pair->code,
                    s_device_id,
                    (unsigned)strlen(json),
                    ret);
        cJSON_free(json);
    }

    cJSON_Delete(root);
#else
    if (pair != NULL && pair->code[0] != '\0')
    {
        ENTITY_LOGI("[HTTP_AGENT][BLE_DISABLED] skip pair-code BLE notify pair_code=%s\r\n", pair->code);
    }
#endif
}

static bool Device_Api_Mac_Is_Usable(const uint8_t mac[6])
{
    bool any_non_zero = false;
    bool any_not_ff = false;

    for (size_t i = 0; i < 6; ++i)
    {
        any_non_zero = any_non_zero || (mac[i] != 0x00);
        any_not_ff = any_not_ff || (mac[i] != 0xff);
    }

    return any_non_zero && any_not_ff;
}

static void Device_Api_Ensure_Device_Id(void)
{
    uint8_t mac[6] = {0};

    if (s_device_id_explicit)
    {
        return;
    }

    Bsp_Wifi_Get_Macaddr(mac);
    if (!Device_Api_Mac_Is_Usable(mac))
    {
        ENTITY_LOGW("[HTTP_DEVICE_API] wifi mac unavailable; keep fallback device=%s\r\n", s_device_id);
        return;
    }

    snprintf(s_device_id,
             sizeof(s_device_id),
             "AG-%02X%02X%02X%02X%02X%02X",
             mac[0],
             mac[1],
             mac[2],
             mac[3],
             mac[4],
             mac[5]);
}

static int Device_Api_Load_Device_Token(void)
{
    int ret = 0;

    if (s_device_token_loaded || s_device_token[0] != '\0')
    {
        return s_device_token[0] != '\0' ? 0 : -1;
    }

    memset(s_device_token, 0, sizeof(s_device_token));
    ret = Bsp_Flash_Read_Key_Value(DEVICE_API_DEVICE_TOKEN_KEY,
                                   (unsigned char *)s_device_token,
                                   sizeof(s_device_token) - 1u);
    s_device_token[sizeof(s_device_token) - 1u] = '\0';
    s_device_token_loaded = true;

    if (ret <= 0 || s_device_token[0] == '\0')
    {
        ENTITY_LOGI("[HTTP_DEVICE_API] no stored device_token\r\n");
        s_device_token[0] = '\0';
        return -1;
    }

    ENTITY_LOGI("[HTTP_DEVICE_API] loaded device_token len=%u\r\n",
                (unsigned)strlen(s_device_token));
    return 0;
}

static int Device_Api_Save_Device_Token(void)
{
    int ret = 0;

    if (s_device_token[0] == '\0')
    {
        return -1;
    }

    ret = Bsp_Flash_Save_Key_Value(DEVICE_API_DEVICE_TOKEN_KEY,
                                   (unsigned char *)s_device_token,
                                   (unsigned int)strlen(s_device_token) + 1u);
    if (ret != 0)
    {
        ENTITY_LOGE("[HTTP_DEVICE_API] save device_token failed ret=%d\r\n", ret);
        return ret;
    }

    ENTITY_LOGI("[HTTP_DEVICE_API] device_token saved len=%u\r\n",
                (unsigned)strlen(s_device_token));
    return 0;
}

static int Device_Api_Build_Url(char *url, size_t url_len, const char *path_fmt)
{
    size_t offset = 0;
    if (url == NULL || path_fmt == NULL)
    {
        return -1;
    }

    if (s_base_url[0] == '\0')
    {
        return -2;
    }

    const int base_has_slash = (s_base_url[strlen(s_base_url) - 1] == '/');
    const int path_has_slash = (path_fmt[0] == '/');
    const char *path_prefix = (base_has_slash || path_has_slash) ? "" : "/";
    int written = snprintf(url, url_len, "%s%s", s_base_url, path_prefix);
    if (written < 0 || (size_t)written >= url_len)
    {
        return -3;
    }
    offset = strlen(url);

    if (base_has_slash && path_has_slash && offset > 0)
    {
        offset--;
    }

    written = snprintf(url + offset, url_len - offset, path_fmt, s_device_id);
    if (written < 0 || (size_t)written >= url_len - offset)
    {
        return -4;
    }

    return 0;
}

static int Device_Api_Parse_Http_Url(const char *url,
                                     char *host,
                                     size_t host_len,
                                     uint16_t *port,
                                     char *path,
                                     size_t path_len)
{
    const char *cursor = NULL;
    const char *path_start = NULL;
    const char *port_start = NULL;
    size_t host_copy_len = 0;

    if (url == NULL || host == NULL || port == NULL || path == NULL)
    {
        return -1;
    }

    if (strncmp(url, "https://", 8) == 0)
    {
        ENTITY_LOGE("[HTTP_DEVICE_API] https URL requires TLS and is not wired in W20 product POC: %s\r\n", url);
        return -2;
    }

    if (strncmp(url, "http://", 7) != 0)
    {
        ENTITY_LOGE("[HTTP_DEVICE_API] unsupported URL scheme: %s\r\n", url);
        return -3;
    }

    cursor = url + 7;
    path_start = strchr(cursor, '/');
    if (path_start == NULL)
    {
        path_start = cursor + strlen(cursor);
    }

    port_start = memchr(cursor, ':', (size_t)(path_start - cursor));
    if (port_start != NULL)
    {
        host_copy_len = (size_t)(port_start - cursor);
        long parsed_port = strtol(port_start + 1, NULL, 10);
        if (parsed_port <= 0 || parsed_port > 65535)
        {
            return -4;
        }
        *port = (uint16_t)parsed_port;
    }
    else
    {
        host_copy_len = (size_t)(path_start - cursor);
        *port = 80;
    }

    if (host_copy_len == 0 || host_copy_len >= host_len)
    {
        return -5;
    }
    memcpy(host, cursor, host_copy_len);
    host[host_copy_len] = '\0';

    if (*path_start == '\0')
    {
        snprintf(path, path_len, "/");
    }
    else
    {
        if (strlen(path_start) >= path_len)
        {
            return -6;
        }
        snprintf(path, path_len, "%s", path_start);
    }

    return 0;
}

static int Device_Api_Read_Http_Response(Entity_Network_t *network,
                                         char *response,
                                         size_t response_len)
{
    int total = 0;
    int last_read_len = 0;

    if (network == NULL || response == NULL || response_len == 0)
    {
        return -1;
    }

    response[0] = '\0';
    while ((size_t)total < response_len - 1)
    {
        int read_len = network->Read(network,
                                     response + total,
                                     (uint32_t)(response_len - 1 - (size_t)total),
                                     total == 0 ? DEVICE_API_HTTP_TIMEOUT_MS : 1000U);
        last_read_len = read_len;
        if (read_len <= 0)
        {
            break;
        }
        total += read_len;
        response[total] = '\0';
    }

    if (total <= 0)
    {
        ENTITY_LOGE("[HTTP_DEVICE_API] read response no data total=%d last_read=%d\r\n",
                    total,
                    last_read_len);
        return -2;
    }

    ENTITY_LOGI("[HTTP_DEVICE_API] read response bytes=%d last_read=%d\r\n",
                total,
                last_read_len);
    return total;
}

static int Device_Api_Extract_Http_Body(char *response,
                                        int response_len,
                                        int *status_code,
                                        char **body)
{
    char *status_cursor = NULL;
    char *headers_end = NULL;

    if (response == NULL || response_len <= 0 || status_code == NULL || body == NULL)
    {
        return -1;
    }

    if (strncmp(response, "HTTP/", 5) != 0)
    {
        return -2;
    }

    status_cursor = strchr(response, ' ');
    if (status_cursor == NULL)
    {
        return -3;
    }
    *status_code = atoi(status_cursor + 1);

    headers_end = strstr(response, "\r\n\r\n");
    if (headers_end == NULL)
    {
        return -4;
    }

    *body = headers_end + 4;
    if (*body >= response + response_len)
    {
        return -5;
    }

    return 0;
}

static int Device_Api_Request_Append(size_t *offset, const char *fmt, ...)
{
    va_list args;
    int written = 0;

    if (offset == NULL || *offset >= sizeof(s_http_request))
    {
        return -1;
    }

    va_start(args, fmt);
    written = vsnprintf(s_http_request + *offset, sizeof(s_http_request) - *offset, fmt, args);
    va_end(args);

    if (written < 0 || (size_t)written >= sizeof(s_http_request) - *offset)
    {
        return -2;
    }

    *offset += (size_t)written;
    return 0;
}

static int Device_Api_Http_Json_Request(const char *method,
                                        const char *url,
                                        const char *auth_scheme,
                                        const char *auth_token,
                                        const char *json_body,
                                        int *status_code,
                                        char **response_body)
{
    char host[DEVICE_API_HOST_MAX];
    char path[DEVICE_API_PATH_MAX];
    uint16_t port = 0;
    Entity_Network_t network;
    size_t offset = 0;
    size_t body_len = json_body != NULL ? strlen(json_body) : 0;

    int ret = Device_Api_Parse_Http_Url(url, host, sizeof(host), &port, path, sizeof(path));
    if (ret != 0)
    {
        return ret;
    }

    s_http_request[0] = '\0';
    ret = Device_Api_Request_Append(&offset,
                                    "%s %s HTTP/1.1\r\n"
                                    "Host: %s\r\n",
                                    method,
                                    path,
                                    host);
    if (ret == 0 && auth_scheme != NULL && auth_scheme[0] != '\0' &&
        auth_token != NULL && auth_token[0] != '\0')
    {
        ret = Device_Api_Request_Append(&offset, "Authorization: %s %s\r\n", auth_scheme, auth_token);
    }
    if (ret == 0)
    {
        ret = Device_Api_Request_Append(&offset,
                                        "Accept: application/json\r\n"
                                        "Connection: close\r\n");
    }
    if (ret == 0 && json_body != NULL)
    {
        ret = Device_Api_Request_Append(&offset,
                                        "Content-Type: application/json; charset=utf-8\r\n"
                                        "Content-Length: %u\r\n"
                                        "\r\n"
                                        "%s",
                                        (unsigned)body_len,
                                        json_body);
    }
    else if (ret == 0)
    {
        ret = Device_Api_Request_Append(&offset, "Content-Length: 0\r\n\r\n");
    }

    if (ret != 0)
    {
        ENTITY_LOGE("[HTTP_DEVICE_API] request too large method=%s url=%s auth_len=%u body_len=%u\r\n",
                    method,
                    url,
                    (unsigned)(auth_token != NULL ? strlen(auth_token) : 0),
                    (unsigned)body_len);
        return -10;
    }

    ENTITY_LOGI("[HTTP_DEVICE_API] request method=%s path=%s body_len=%u request_len=%u auth=%s\r\n",
                method,
                path,
                (unsigned)body_len,
                (unsigned)offset,
                (auth_scheme != NULL && auth_scheme[0] != '\0') ? auth_scheme : "none");

    memset(&network, 0, sizeof(network));
    network.Host = host;
    network.Port = port;
    network.Type = NETWORK_TCP;

    ret = Entity_Network_Init(&network);
    if (ret != 0)
    {
        return -11;
    }

    ret = network.Connect(&network);
    if (ret != 0)
    {
        ENTITY_LOGE("[HTTP_DEVICE_API] connect failed host=%s port=%u ret=%d\r\n", host, port, ret);
        return -12;
    }

    ret = network.Write(&network, s_http_request, (uint32_t)offset, DEVICE_API_HTTP_TIMEOUT_MS);
    ENTITY_LOGI("[HTTP_DEVICE_API] write done bytes=%d expected=%u\r\n",
                ret,
                (unsigned)offset);
    if (ret <= 0)
    {
        ENTITY_LOGE("[HTTP_DEVICE_API] write failed host=%s port=%u ret=%d\r\n", host, port, ret);
        network.Disconnect(&network);
        return -13;
    }
    if ((size_t)ret < offset)
    {
        ENTITY_LOGE("[HTTP_DEVICE_API] partial write bytes=%d expected=%u\r\n",
                    ret,
                    (unsigned)offset);
        network.Disconnect(&network);
        return -16;
    }

    ret = Device_Api_Read_Http_Response(&network, s_http_response, sizeof(s_http_response));
    network.Disconnect(&network);
    if (ret < 0)
    {
        ENTITY_LOGE("[HTTP_DEVICE_API] read failed host=%s port=%u ret=%d\r\n", host, port, ret);
        return -14;
    }

    ret = Device_Api_Extract_Http_Body(s_http_response, ret, status_code, response_body);
    if (ret != 0)
    {
        ENTITY_LOGE("[HTTP_DEVICE_API] response parse failed ret=%d\r\n", ret);
        return -15;
    }

    return 0;
}

static int Device_Api_Http_Post_Json(const char *url,
                                     const char *json_body,
                                     int *status_code,
                                     char **response_body)
{
    return Device_Api_Http_Json_Request("POST",
                                        url,
                                        "Device",
                                        s_device_token,
                                        json_body,
                                        status_code,
                                        response_body);
}

static cJSON *Device_Api_Response_Data(cJSON *root)
{
    cJSON *data = cJSON_GetObjectItem(root, "data");
    return cJSON_IsObject(data) ? data : root;
}

static int Device_Api_Copy_Json_String(cJSON *object,
                                       const char *key,
                                       char *dst,
                                       size_t dst_len)
{
    cJSON *item = cJSON_GetObjectItem(object, key);
    if (!cJSON_IsString(item) || item->valuestring == NULL || item->valuestring[0] == '\0')
    {
        ENTITY_LOGE("[HTTP_DEVICE_API] missing json string field: %s\r\n", key);
        return -1;
    }

    snprintf(dst, dst_len, "%s", item->valuestring);
    if (strlen(item->valuestring) >= dst_len)
    {
        ENTITY_LOGE("[HTTP_DEVICE_API] json string field too long: %s\r\n", key);
        return -2;
    }

    return 0;
}

static int Device_Api_Copy_Json_String_Optional(cJSON *object,
                                                const char *key,
                                                char *dst,
                                                size_t dst_len)
{
    cJSON *item = cJSON_GetObjectItem(object, key);
    if (!cJSON_IsString(item) || item->valuestring == NULL || item->valuestring[0] == '\0')
    {
        if (dst_len > 0)
        {
            dst[0] = '\0';
        }
        return 0;
    }

    snprintf(dst, dst_len, "%s", item->valuestring);
    if (strlen(item->valuestring) >= dst_len)
    {
        ENTITY_LOGE("[HTTP_DEVICE_API] json string field too long: %s\r\n", key);
        return -1;
    }

    return 0;
}

static int Device_Api_Json_Int_Or_Default(cJSON *object, const char *key, int fallback)
{
    cJSON *item = cJSON_GetObjectItem(object, key);
    return cJSON_IsNumber(item) ? item->valueint : fallback;
}

static int Device_Api_Parse_Pair_Code_Response(const char *json,
                                               Device_Api_Pair_Code_Result_t *out)
{
    cJSON *root = NULL;
    cJSON *data = NULL;
    int ret = -1;

    if (json == NULL || out == NULL)
    {
        return -1;
    }

    root = cJSON_Parse(json);
    if (root == NULL)
    {
        ENTITY_LOGE("[HTTP_DEVICE_API] pair-code response is not valid json\r\n");
        return -2;
    }

    memset(out, 0, sizeof(*out));
    data = Device_Api_Response_Data(root);
    if (Device_Api_Copy_Json_String(data, "code", out->code, sizeof(out->code)) != 0 ||
        Device_Api_Copy_Json_String(data, "pair_token", out->pair_token, sizeof(out->pair_token)) != 0)
    {
        ret = -3;
        goto cleanup;
    }

    out->expires_in_seconds = Device_Api_Json_Int_Or_Default(data,
                                                             "expires_in_seconds",
                                                             DEVICE_API_PAIR_POLL_MAX_SECONDS);
    out->poll_after_seconds = Device_Api_Json_Int_Or_Default(data,
                                                             "poll_after_seconds",
                                                             DEVICE_API_PAIR_DEFAULT_POLL_SECONDS);
    if (out->poll_after_seconds <= 0)
    {
        out->poll_after_seconds = DEVICE_API_PAIR_DEFAULT_POLL_SECONDS;
    }
    if (out->expires_in_seconds <= 0)
    {
        out->expires_in_seconds = DEVICE_API_PAIR_POLL_MAX_SECONDS;
    }

    ret = 0;

cleanup:
    cJSON_Delete(root);
    return ret;
}

static int Device_Api_Parse_Binding_Status_Response(const char *json,
                                                    Device_Api_Binding_Status_Result_t *out)
{
    cJSON *root = NULL;
    cJSON *data = NULL;
    int ret = -1;

    if (json == NULL || out == NULL)
    {
        return -1;
    }

    root = cJSON_Parse(json);
    if (root == NULL)
    {
        ENTITY_LOGE("[HTTP_DEVICE_API] binding-status response is not valid json\r\n");
        return -2;
    }

    memset(out, 0, sizeof(*out));
    data = Device_Api_Response_Data(root);
    if (Device_Api_Copy_Json_String(data, "status", out->status, sizeof(out->status)) != 0 ||
        Device_Api_Copy_Json_String_Optional(data, "device_token", out->device_token, sizeof(out->device_token)) != 0)
    {
        ret = -3;
        goto cleanup;
    }

    out->poll_after_seconds = Device_Api_Json_Int_Or_Default(data,
                                                             "poll_after_seconds",
                                                             DEVICE_API_PAIR_DEFAULT_POLL_SECONDS);
    if (out->poll_after_seconds <= 0)
    {
        out->poll_after_seconds = DEVICE_API_PAIR_DEFAULT_POLL_SECONDS;
    }

    ret = 0;

cleanup:
    cJSON_Delete(root);
    return ret;
}

static int Device_Api_Parse_Start_Response(const char *json,
                                           Device_Api_Conversation_Start_Result_t *out)
{
    cJSON *root = NULL;
    cJSON *data = NULL;
    cJSON *rtc = NULL;
    cJSON *uid = NULL;
    int ret = -1;

    if (json == NULL || out == NULL)
    {
        return -1;
    }

    root = cJSON_Parse(json);
    if (root == NULL)
    {
        ENTITY_LOGE("[HTTP_DEVICE_API] start response is not valid json\r\n");
        return -2;
    }

    memset(out, 0, sizeof(*out));
    data = Device_Api_Response_Data(root);
    rtc = cJSON_GetObjectItem(data, "rtc");
    if (!cJSON_IsObject(rtc))
    {
        ENTITY_LOGE("[HTTP_DEVICE_API] missing json object field: rtc\r\n");
        ret = -3;
        goto cleanup;
    }

    if (Device_Api_Copy_Json_String(data, "conversation_id", out->conversation_id, sizeof(out->conversation_id)) != 0 ||
        Device_Api_Copy_Json_String(rtc, "app_id", out->app_id, sizeof(out->app_id)) != 0 ||
        Device_Api_Copy_Json_String(rtc, "channel", out->channel, sizeof(out->channel)) != 0 ||
        Device_Api_Copy_Json_String(rtc, "token", out->token, sizeof(out->token)) != 0)
    {
        ret = -5;
        goto cleanup;
    }

    uid = cJSON_GetObjectItem(rtc, "uid");
    if (cJSON_IsString(uid) && uid->valuestring != NULL && uid->valuestring[0] != '\0')
    {
        char *end = NULL;
        long parsed_uid = strtol(uid->valuestring, &end, 10);
        snprintf(out->user_account, sizeof(out->user_account), "%s", uid->valuestring);
        if (strlen(uid->valuestring) >= sizeof(out->user_account))
        {
            ENTITY_LOGE("[HTTP_DEVICE_API] json string field too long: uid\r\n");
            ret = -6;
            goto cleanup;
        }
        out->uid = (end != uid->valuestring && *end == '\0' && parsed_uid >= 0 && parsed_uid <= INT32_MAX) ? (int)parsed_uid : 0;
    }
    else if (cJSON_IsNumber(uid))
    {
        out->uid = uid->valueint;
        snprintf(out->user_account, sizeof(out->user_account), "%d", out->uid);
    }
    else
    {
        ENTITY_LOGE("[HTTP_DEVICE_API] missing json string/number field: uid\r\n");
        ret = -4;
        goto cleanup;
    }

    snprintf(s_last_conversation_id, sizeof(s_last_conversation_id), "%s", out->conversation_id);
    ret = 0;

cleanup:
    cJSON_Delete(root);
    return ret;
}

void Device_Api_Client_Init(const char *base_url,
                            const char *device_id,
                            const char *device_token)
{
    if (base_url != NULL && base_url[0] != '\0')
    {
        snprintf(s_base_url, sizeof(s_base_url), "%s", base_url);
    }
    if (device_id != NULL && device_id[0] != '\0')
    {
        snprintf(s_device_id, sizeof(s_device_id), "%s", device_id);
        s_device_id_explicit = true;
    }
    if (device_token != NULL && device_token[0] != '\0')
    {
        snprintf(s_device_token, sizeof(s_device_token), "%s", device_token);
        s_device_token_loaded = true;
    }

    Device_Api_Ensure_Device_Id();
    (void)Device_Api_Load_Device_Token();

    ENTITY_LOGI("[HTTP_DEVICE_API] init base=%s device=%s token_len=%u\r\n",
                s_base_url,
                s_device_id,
                (unsigned)strlen(s_device_token));
}

static int Device_Api_Request_Pair_Code(Device_Api_Pair_Code_Result_t *out)
{
    char url[DEVICE_API_URL_MAX];
    char body[DEVICE_API_BODY_MAX];
    char *response_body = NULL;
    int status_code = 0;
    int ret = 0;

    if (out == NULL)
    {
        return -1;
    }

    Device_Api_Ensure_Device_Id();

    ret = Device_Api_Build_Url(url, sizeof(url), "/devices/pair-codes");
    if (ret != 0)
    {
        ENTITY_LOGE("[HTTP_DEVICE_API] build pair-code url failed ret=%d\r\n", ret);
        return -2;
    }

    ret = snprintf(body,
                   sizeof(body),
                   "{\"device_id\":\"%s\","
                   "\"firmware_version\":\"1.0.0\","
                   "\"hardware_model\":\"esp32-s3-box-3\"}",
                   s_device_id);
    if (ret < 0 || (size_t)ret >= sizeof(body))
    {
        return -3;
    }

    ENTITY_LOGI("[HTTP_DEVICE_API] pair-code request device=%s\r\n", s_device_id);
    ret = Device_Api_Http_Json_Request("POST", url, NULL, NULL, body, &status_code, &response_body);
    if (ret != 0)
    {
        ENTITY_LOGE("[HTTP_DEVICE_API] pair-code HTTP failed ret=%d\r\n", ret);
        return -4;
    }

    if (status_code != 200 && status_code != 201)
    {
        ENTITY_LOGE("[HTTP_DEVICE_API] pair-code rejected http_status=%d\r\n", status_code);
        return -5;
    }

    ret = Device_Api_Parse_Pair_Code_Response(response_body, out);
    if (ret != 0)
    {
        ENTITY_LOGE("[HTTP_DEVICE_API] pair-code response parse failed ret=%d\r\n", ret);
        return -6;
    }

    ENTITY_LOGI("[HTTP_DEVICE_API] PAIR_CODE=%s device=%s expires_in=%ds poll_after=%ds\r\n",
                out->code,
                s_device_id,
                out->expires_in_seconds,
                out->poll_after_seconds);
    Device_Api_Notify_Pair_Code_Ble(out);
    return 0;
}

static int Device_Api_Check_Binding_Status(const char *pair_token,
                                           Device_Api_Binding_Status_Result_t *out)
{
    char url[DEVICE_API_URL_MAX];
    char *response_body = NULL;
    int status_code = 0;
    int ret = 0;

    if (pair_token == NULL || pair_token[0] == '\0' || out == NULL)
    {
        return -1;
    }

    ret = Device_Api_Build_Url(url, sizeof(url), "/devices/%s/binding-status");
    if (ret != 0)
    {
        ENTITY_LOGE("[HTTP_DEVICE_API] build binding-status url failed ret=%d\r\n", ret);
        return -2;
    }

    ret = Device_Api_Http_Json_Request("GET", url, "Pair", pair_token, NULL, &status_code, &response_body);
    if (ret != 0)
    {
        ENTITY_LOGE("[HTTP_DEVICE_API] binding-status HTTP failed ret=%d\r\n", ret);
        return -3;
    }

    if (status_code != 200)
    {
        ENTITY_LOGE("[HTTP_DEVICE_API] binding-status rejected http_status=%d\r\n", status_code);
        return -4;
    }

    ret = Device_Api_Parse_Binding_Status_Response(response_body, out);
    if (ret != 0)
    {
        ENTITY_LOGE("[HTTP_DEVICE_API] binding-status response parse failed ret=%d\r\n", ret);
        return -5;
    }

    return 0;
}

int Device_Api_Binding_Begin(void)
{
    int ret = 0;

    s_binding_cancel_requested = false;
    Device_Api_Ensure_Device_Id();
    (void)Device_Api_Load_Device_Token();

    if (s_device_token[0] != '\0')
    {
        s_pending_pair_valid = false;
        memset(&s_pending_pair, 0, sizeof(s_pending_pair));
        ENTITY_LOGI("[HTTP_DEVICE_API] binding begin skip: device_token already configured len=%u\r\n",
                    (unsigned)strlen(s_device_token));
        return 0;
    }

    if (s_pending_pair_valid)
    {
        ENTITY_LOGI("[HTTP_DEVICE_API] binding begin reuse pending PAIR_CODE=%s device=%s\r\n",
                    s_pending_pair.code,
                    s_device_id);
        Device_Api_Notify_Pair_Code_Ble(&s_pending_pair);
        return 0;
    }

    memset(&s_pending_pair, 0, sizeof(s_pending_pair));
    ret = Device_Api_Request_Pair_Code(&s_pending_pair);
    if (ret != 0)
    {
        memset(&s_pending_pair, 0, sizeof(s_pending_pair));
        ENTITY_LOGE("[HTTP_DEVICE_API] binding begin pair-code failed ret=%d\r\n", ret);
        return ret;
    }

    s_pending_pair_valid = true;
    ENTITY_LOGI("[HTTP_DEVICE_API] binding begin pending pair-code ready device=%s\r\n", s_device_id);
    return 0;
}

int Device_Api_Clear_Device_Token(void)
{
    int ret = Bsp_Flash_Delete_Key(DEVICE_API_DEVICE_TOKEN_KEY);

    memset(s_device_token, 0, sizeof(s_device_token));
    s_device_token_loaded = false;
    s_pending_pair_valid = false;
    memset(&s_pending_pair, 0, sizeof(s_pending_pair));

    ENTITY_LOGI("[HTTP_DEVICE_API] clear device_token ret=%d\r\n", ret);
    return ret;
}

int Device_Api_Get_Pending_Pair_Code(char *pair_code,
                                     size_t pair_code_len,
                                     char *device_id,
                                     size_t device_id_len,
                                     int *expires_in_seconds,
                                     int *poll_after_seconds,
                                     int *has_device_token)
{
    Device_Api_Ensure_Device_Id();
    (void)Device_Api_Load_Device_Token();

    if (has_device_token != NULL)
    {
        *has_device_token = (s_device_token[0] != '\0') ? 1 : 0;
    }
    if (device_id != NULL && device_id_len > 0)
    {
        snprintf(device_id, device_id_len, "%s", s_device_id);
    }
    if (pair_code != NULL && pair_code_len > 0)
    {
        pair_code[0] = '\0';
        if (s_pending_pair_valid)
        {
            snprintf(pair_code, pair_code_len, "%s", s_pending_pair.code);
        }
    }
    if (expires_in_seconds != NULL)
    {
        *expires_in_seconds = s_pending_pair_valid ? s_pending_pair.expires_in_seconds : 0;
    }
    if (poll_after_seconds != NULL)
    {
        *poll_after_seconds = s_pending_pair_valid ? s_pending_pair.poll_after_seconds : DEVICE_API_PAIR_DEFAULT_POLL_SECONDS;
    }

    return s_pending_pair_valid ? 0 : -1;
}

void Device_Api_Binding_Cancel(void)
{
    s_binding_cancel_requested = true;
}

void Device_Api_Binding_Clear_Cancel(void)
{
    s_binding_cancel_requested = false;
    ENTITY_LOGI("[HTTP_DEVICE_API] binding clear stale cancel\r\n");
}

int Device_Api_Binding_Run(void)
{
    Device_Api_Pair_Code_Result_t pair;
    int waited_seconds = 0;
    int ret = 0;

    Device_Api_Ensure_Device_Id();
    (void)Device_Api_Load_Device_Token();

    if (s_device_token[0] != '\0')
    {
        ENTITY_LOGI("[HTTP_DEVICE_API] binding skip: device_token already configured len=%u\r\n",
                    (unsigned)strlen(s_device_token));
        return 0;
    }

    if (s_pending_pair_valid)
    {
        pair = s_pending_pair;
        ENTITY_LOGI("[HTTP_DEVICE_API] binding pending PAIR_CODE=%s device=%s\r\n",
                    pair.code,
                    s_device_id);
    }
    else
    {
        ret = Device_Api_Request_Pair_Code(&pair);
        if (ret != 0)
        {
            return ret;
        }
        s_pending_pair = pair;
        s_pending_pair_valid = true;
    }

    while (waited_seconds < pair.expires_in_seconds)
    {
        Device_Api_Binding_Status_Result_t status;
        int delay_seconds = pair.poll_after_seconds;

        if (s_binding_cancel_requested)
        {
            s_binding_cancel_requested = false;
            s_pending_pair_valid = false;
            memset(&s_pending_pair, 0, sizeof(s_pending_pair));
            ENTITY_LOGI("[HTTP_DEVICE_API] binding cancelled waited=%ds\r\n", waited_seconds);
            return -23;
        }

        if (delay_seconds <= 0)
        {
            delay_seconds = DEVICE_API_PAIR_DEFAULT_POLL_SECONDS;
        }
        if (waited_seconds + delay_seconds > pair.expires_in_seconds)
        {
            delay_seconds = pair.expires_in_seconds - waited_seconds;
        }
        if (delay_seconds <= 0)
        {
            break;
        }

        rtos_delay_milliseconds((uint32_t)delay_seconds * 1000U);
        waited_seconds += delay_seconds;

        if (s_binding_cancel_requested)
        {
            s_binding_cancel_requested = false;
            s_pending_pair_valid = false;
            memset(&s_pending_pair, 0, sizeof(s_pending_pair));
            ENTITY_LOGI("[HTTP_DEVICE_API] binding cancelled waited=%ds\r\n", waited_seconds);
            return -23;
        }

        ret = Device_Api_Check_Binding_Status(pair.pair_token, &status);
        if (ret != 0)
        {
            return ret;
        }

        if (strcmp(status.status, "bound") == 0)
        {
            if (status.device_token[0] == '\0')
            {
                ENTITY_LOGE("[HTTP_DEVICE_API] binding bound but missing device_token\r\n");
                return -20;
            }

            snprintf(s_device_token, sizeof(s_device_token), "%s", status.device_token);
            ENTITY_LOGI("[HTTP_DEVICE_API] device bound token_len=%u\r\n",
                        (unsigned)strlen(s_device_token));
            (void)Device_Api_Save_Device_Token();
            s_pending_pair_valid = false;
            memset(&s_pending_pair, 0, sizeof(s_pending_pair));
            return 0;
        }

        if (strcmp(status.status, "pending") == 0)
        {
            ENTITY_LOGI("[HTTP_DEVICE_API] binding pending PAIR_CODE=%s device=%s waited=%ds poll_after=%ds\r\n",
                        pair.code,
                        s_device_id,
                        waited_seconds,
                        status.poll_after_seconds);
            pair.poll_after_seconds = status.poll_after_seconds;
            continue;
        }

        ENTITY_LOGW("[HTTP_DEVICE_API] binding stopped status=%s waited=%ds\r\n",
                    status.status,
                    waited_seconds);
        s_pending_pair_valid = false;
        memset(&s_pending_pair, 0, sizeof(s_pending_pair));
        return -21;
    }

    ENTITY_LOGW("[HTTP_DEVICE_API] pair-code expired after %ds\r\n", waited_seconds);
    s_pending_pair_valid = false;
    memset(&s_pending_pair, 0, sizeof(s_pending_pair));
    return -22;
}

int Device_Api_Conversation_Start(Device_Api_Conversation_Start_Result_t *out)
{
    char url[DEVICE_API_URL_MAX];
    char body[DEVICE_API_BODY_MAX];
    char *response_body = NULL;
    int status_code = 0;
    int ret = 0;

    if (out == NULL)
    {
        return -1;
    }

    Device_Api_Ensure_Device_Id();
    (void)Device_Api_Load_Device_Token();

    ENTITY_LOGI("[HTTP_DEVICE_API] conversation start request device=%s token_len=%u\r\n",
                s_device_id,
                (unsigned)strlen(s_device_token));

    if (s_device_token[0] == '\0')
    {
        ENTITY_LOGE("[HTTP_DEVICE_API] missing device_token; configure token before starting AI\r\n");
        return -2;
    }

    ret = Device_Api_Build_Url(url, sizeof(url), "/devices/%s/conversations/start");
    if (ret != 0)
    {
        ENTITY_LOGE("[HTTP_DEVICE_API] build start url failed ret=%d\r\n", ret);
        return -3;
    }

    ret = snprintf(body,
                   sizeof(body),
                   "{\"trigger\":\"button\","
                   "\"features\":{\"ai_qos\":true,"
                   "\"fast_send_multiplier\":3,\"show_transcript\":true},"
                   "\"firmware_version\":\"bk7258-w20-poc\"}");
    if (ret < 0 || (size_t)ret >= sizeof(body))
    {
        return -4;
    }

    ret = Device_Api_Http_Post_Json(url, body, &status_code, &response_body);
    if (ret != 0)
    {
        ENTITY_LOGE("[HTTP_DEVICE_API] conversation start HTTP failed ret=%d\r\n", ret);
        return -5;
    }

    if (status_code != 200 && status_code != 201)
    {
        ENTITY_LOGE("[HTTP_DEVICE_API] conversation start rejected http_status=%d\r\n", status_code);
        return -6;
    }

    ret = Device_Api_Parse_Start_Response(response_body, out);
    if (ret != 0)
    {
        ENTITY_LOGE("[HTTP_DEVICE_API] conversation start response parse failed ret=%d\r\n", ret);
        return -7;
    }

    ENTITY_LOGI("[HTTP_DEVICE_API] conversation started id=%s channel=%s uid=%d user_account=%s token_len=%u\r\n",
                out->conversation_id,
                out->channel,
                out->uid,
                out->user_account,
                (unsigned)strlen(out->token));
    return 0;
}

int Device_Api_Conversation_Stop(const char *conversation_id)
{
    char url[DEVICE_API_URL_MAX];
    char body[DEVICE_API_BODY_MAX];
    char *response_body = NULL;
    int status_code = 0;
    int ret = 0;

    if (conversation_id == NULL || conversation_id[0] == '\0')
    {
        return 0;
    }

    Device_Api_Ensure_Device_Id();

    if (s_device_token[0] == '\0')
    {
        ENTITY_LOGW("[HTTP_DEVICE_API] skip conversation stop: missing device_token\r\n");
        return -1;
    }

    ret = Device_Api_Build_Url(url, sizeof(url), "/devices/%s/conversations/stop");
    if (ret != 0)
    {
        ENTITY_LOGE("[HTTP_DEVICE_API] build stop url failed ret=%d\r\n", ret);
        return -2;
    }

    ret = snprintf(body,
                   sizeof(body),
                   "{\"conversation_id\":\"%s\",\"reason\":\"device_hangup\"}",
                   conversation_id);
    if (ret < 0 || (size_t)ret >= sizeof(body))
    {
        return -3;
    }

    ENTITY_LOGI("[HTTP_DEVICE_API] conversation stop request id=%s\r\n", conversation_id);
    ret = Device_Api_Http_Post_Json(url, body, &status_code, &response_body);
    if (ret != 0)
    {
        ENTITY_LOGW("[HTTP_DEVICE_API] conversation stop HTTP failed ret=%d\r\n", ret);
        return -4;
    }

    if (status_code < 200 || status_code >= 300)
    {
        ENTITY_LOGW("[HTTP_DEVICE_API] conversation stop rejected http_status=%d\r\n", status_code);
        return -5;
    }

    (void)response_body;
    ENTITY_LOGI("[HTTP_DEVICE_API] conversation stopped id=%s http_status=%d\r\n", conversation_id, status_code);
    return 0;
}

const char *Device_Api_Client_Last_Conversation_Id(void)
{
    return s_last_conversation_id;
}
