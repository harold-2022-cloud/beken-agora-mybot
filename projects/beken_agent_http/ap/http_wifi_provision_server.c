#include "http_wifi_provision_server.h"

#include "bsp_flash.h"
#include "bsp_wifi.h"
#include "cJSON.h"
#include "components/system.h"
#include <components/bk_uid.h>
#include "device_api_client.h"
#include "entity_dev_info.h"
#include "entity_iot_cloud.h"
#include "entity_log.h"
#include "entity_wifi.h"
#include "ef_cfg.h"
#include "os/os.h"

#include "lwip/sockets.h"

#if CONFIG_COUNTDOWN
#include "countdown.h"
#endif

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define HTTP_WIFI_PROV_PORT 80
#define HTTP_WIFI_PROV_AP_IP "192.168.4.1"
#define HTTP_WIFI_PROV_AP_MASK "255.255.255.0"
#define HTTP_WIFI_PROV_REQ_MAX 1536
#define HTTP_WIFI_PROV_RESP_MAX 4096
#define HTTP_WIFI_PROV_STACK 6144
#define HTTP_WIFI_PROV_PRIO 4
#define HTTP_WIFI_PROV_EF_RESERVED_SECTORS 1u
#define HTTP_WIFI_PROV_STANDBY_COUNTDOWN_MS (3u * 60u * 1000u)

static volatile int s_http_wifi_prov_running;
static int s_http_wifi_prov_listen_fd = -1;
static beken_thread_t s_http_wifi_prov_thread = NULL;
static volatile int s_http_wifi_prov_sta_submitted;
static volatile int s_http_wifi_prov_sta_online;
static char s_http_wifi_prov_request[HTTP_WIFI_PROV_REQ_MAX];
static char s_http_wifi_prov_response[HTTP_WIFI_PROV_RESP_MAX];
static Entity_Wifi_Sta_Scan_Result_t s_http_wifi_prov_scan;

static const char s_http_wifi_prov_index_html[] =
    "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Device Wi-Fi Setup</title><style>"
    "body{font-family:Arial,sans-serif;margin:24px;background:#f6f7f8;color:#111}"
    "main{max-width:520px;margin:auto;background:white;padding:18px;border:1px solid #ddd}"
    "button,input,select{font-size:16px;padding:10px;margin:6px 0;width:100%;box-sizing:border-box}"
    "#status{margin-top:14px;color:#555}"
    "#pairCode{margin:18px 0 8px;font-size:52px;font-weight:800;letter-spacing:2px;text-align:center}"
    "#pairHint{color:#555;text-align:center}"
    "#danger{margin-top:16px;border-top:1px solid #eee;padding-top:12px}"
    "</style></head><body><main><h2>Device Wi-Fi Setup</h2>"
    "<div id='setup'><select id='ssid'></select><input id='password' type='password' placeholder='Wi-Fi password'>"
    "<button onclick='scan()'>Refresh Wi-Fi List</button><button onclick='submitWifi()'>Connect</button>"
    "</div><h3 id='pairTitle'>Pair Code</h3><div id='pairCode'>----</div>"
    "<div id='pairHint'>Connect Wi-Fi first. Pair code refreshes automatically after STA is online.</div>"
    "<div id='danger'><button onclick='clearEnv()'>Clear Saved Setup</button></div>"
    "<div id='status'>Scanning Wi-Fi...</div>"
    "<script>"
    "let pairTimer=null;"
    "async function scan(){let r=await fetch('/scan');let j=await r.json();let s=document.getElementById('ssid');"
    "s.innerHTML='';(j.aps||[]).forEach(a=>{let o=document.createElement('option');o.value=a.ssid;o.textContent=a.ssid+'  '+a.rssi+'dBm';s.appendChild(o);});}"
    "async function submitWifi(){let body={ssid:document.getElementById('ssid').value,password:document.getElementById('password').value};"
    "document.getElementById('status').textContent='Connecting Wi-Fi...';"
    "let r=await fetch('/submit',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});"
    "document.getElementById('status').textContent=r.ok?'Waiting for pair code...':'Wi-Fi submit failed';"
    "pollPair();if(!pairTimer){pairTimer=setInterval(pollPair,1500);}}"
    "async function pollPair(){try{let r=await fetch('/pair-code');let j=await r.json();"
    "let code=j.pair_code||'';if(code){document.getElementById('setup').style.display='none';"
    "document.getElementById('pairCode').textContent=code;document.getElementById('pairHint').textContent='Enter this code on the binding page.';"
    "document.getElementById('status').textContent='';}else{document.getElementById('status').textContent='Waiting for device Wi-Fi and pair code...';}}catch(e){}}"
    "async function clearEnv(){if(!confirm('Clear saved Wi-Fi and pairing data?'))return;"
    "let r=await fetch('/clear-env',{method:'POST'});let j=await r.json();"
    "document.getElementById('status').textContent='Clear setup ret='+j.ret+'. Reboot or submit Wi-Fi again.';"
    "document.getElementById('pairCode').textContent='----';document.getElementById('setup').style.display='block';}"
    "scan();</script></main></body></html>";

static void http_wifi_prov_log_easyflash_env(const char *stage)
{
    size_t total = ENV_AREA_SIZE;
    size_t usable = total;
    size_t reserved = EF_ERASE_MIN_SIZE * HTTP_WIFI_PROV_EF_RESERVED_SECTORS;
    size_t value_len = sizeof(Entity_Dev_Config_Net_Info_t);
    size_t key_len = strlen("config_net_data");
    size_t required_est = 40u + ((key_len + 3u) & ~3u) + ((value_len + 3u) & ~3u);

    if (usable > reserved)
    {
        usable -= reserved;
    }

    ENTITY_LOGI("[HTTP_WIFI_PROV][EF_ENV] stage=%s total=%u usable_est=%u reserved=%u config_value_len=%u required_est=%u\r\n",
                stage ? stage : "unknown",
                (unsigned)total,
                (unsigned)usable,
                (unsigned)reserved,
                (unsigned)value_len,
                (unsigned)required_est);
}

static int http_send_all(int fd, const char *data, size_t len)
{
    size_t sent = 0;

    while (sent < len)
    {
        int ret = send(fd, data + sent, len - sent, 0);
        if (ret <= 0)
        {
            return -1;
        }
        sent += (size_t)ret;
    }

    return 0;
}

static int http_send_response(int fd, const char *status, const char *content_type, const char *body)
{
    char header[256];
    size_t body_len = body ? strlen(body) : 0;
    int header_len = snprintf(header,
                              sizeof(header),
                              "HTTP/1.1 %s\r\n"
                              "Content-Type: %s\r\n"
                              "Content-Length: %u\r\n"
                              "Connection: close\r\n"
                              "Cache-Control: no-store\r\n"
                              "\r\n",
                              status,
                              content_type,
                              (unsigned)body_len);
    if (header_len < 0 || (size_t)header_len >= sizeof(header))
    {
        return -1;
    }
    if (http_send_all(fd, header, (size_t)header_len) != 0)
    {
        return -2;
    }
    if (body_len > 0 && http_send_all(fd, body, body_len) != 0)
    {
        return -3;
    }
    return 0;
}

static int http_send_redirect_to_root(int fd, const char *request)
{
    const char *response =
        "HTTP/1.1 302 Found\r\n"
        "Location: http://192.168.4.1/\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n"
        "Cache-Control: no-store\r\n"
        "\r\n";

    ENTITY_LOGI("[HTTP_WIFI_PROV] captive redirect %.48s\r\n", request ? request : "");
    return http_send_all(fd, response, strlen(response));
}

static int http_request_is_root_get(const char *request)
{
    return request != NULL &&
           (strncmp(request, "GET / HTTP/1.", 13) == 0 ||
            strncmp(request, "GET /? HTTP/1.", 14) == 0);
}

static void http_wifi_prov_pause_standby_countdown(void)
{
#if CONFIG_COUNTDOWN
    ENTITY_LOGI("[HTTP_WIFI_PROV] pause standby countdown while AP provisioning is active\r\n");
    stop_countdown();
#else
    ENTITY_LOGI("[HTTP_WIFI_PROV] pause standby countdown skipped: CONFIG_COUNTDOWN disabled\r\n");
#endif
}

static void http_wifi_prov_resume_standby_countdown(void)
{
#if CONFIG_COUNTDOWN
    ENTITY_LOGI("[HTTP_WIFI_PROV] resume standby countdown after AP provisioning stop timeout_ms=%u\r\n",
                (unsigned)HTTP_WIFI_PROV_STANDBY_COUNTDOWN_MS);
    start_countdown(HTTP_WIFI_PROV_STANDBY_COUNTDOWN_MS);
#else
    ENTITY_LOGI("[HTTP_WIFI_PROV] resume standby countdown skipped: CONFIG_COUNTDOWN disabled\r\n");
#endif
}

static size_t json_append_escaped(char *out, size_t out_len, size_t offset, const char *value)
{
    const unsigned char *p = (const unsigned char *)(value ? value : "");

    while (*p != '\0' && offset + 2 < out_len)
    {
        if (*p == '"' || *p == '\\')
        {
            out[offset++] = '\\';
            out[offset++] = (char)*p;
        }
        else if (*p >= 0x20)
        {
            out[offset++] = (char)*p;
        }
        p++;
    }
    if (offset < out_len)
    {
        out[offset] = '\0';
    }
    return offset;
}

static size_t http_wifi_prov_strnlen(const char *value, size_t max_len)
{
    size_t len = 0;

    if (value == NULL)
    {
        return 0;
    }

    while (len < max_len && value[len] != '\0')
    {
        len++;
    }

    return len;
}

static uint32_t http_wifi_prov_hash_string(const char *value, size_t max_len)
{
    size_t len = http_wifi_prov_strnlen(value, max_len);
    uint32_t hash = 2166136261u;

    for (size_t i = 0; i < len; ++i)
    {
        hash ^= (uint8_t)value[i];
        hash *= 16777619u;
    }

    return hash;
}

static void http_wifi_prov_build_ap_ssid(char *ssid, size_t ssid_len)
{
    unsigned char uid[32] = {0};
    uint8_t mac[6] = {0};
    uint32_t uid_hash = 2166136261u;
    bool uid_any_non_zero = false;
    bool uid_any_not_ff = false;

    if (bk_uid_get_data(uid) == BK_OK)
    {
        for (size_t i = 0; i < sizeof(uid); ++i)
        {
            uid_any_non_zero = uid_any_non_zero || (uid[i] != 0x00);
            uid_any_not_ff = uid_any_not_ff || (uid[i] != 0xff);
            uid_hash ^= uid[i];
            uid_hash *= 16777619u;
        }

        if (uid_any_non_zero && uid_any_not_ff)
        {
            snprintf(ssid, ssid_len, "R1-%06X", (unsigned int)(uid_hash & 0xFFFFFFu));
            return;
        }

        ENTITY_LOGW("[HTTP_WIFI_PROV] chip UID invalid; fallback to BT MAC suffix for AP SSID\r\n");
    }
    else
    {
        ENTITY_LOGW("[HTTP_WIFI_PROV] chip UID unavailable; fallback to BT MAC suffix for AP SSID\r\n");
    }

    if (bk_get_mac(mac, MAC_TYPE_BLUETOOTH) != BK_OK)
    {
        ENTITY_LOGW("[HTTP_WIFI_PROV] BT MAC unavailable; use zero suffix for AP SSID\r\n");
    }

    snprintf(ssid,
             ssid_len,
             "R1-%02X%02X%02X",
             mac[3],
             mac[4],
             mac[5]);
}

static int http_wifi_prov_handle_scan(int fd)
{
    Entity_Wifi_Sta_Scan_Result_t *scan = &s_http_wifi_prov_scan;
    char *body = s_http_wifi_prov_response;
    size_t offset = 0;
    int ret = 0;

    memset(scan, 0, sizeof(*scan));
    memset(body, 0, HTTP_WIFI_PROV_RESP_MAX);
    ret = Bsp_Wifi_Copy_Scan_Results(scan, sizeof(*scan));
    ENTITY_LOGI("[HTTP_WIFI_PROV] GET /scan ret=%d count=%u\r\n", ret, (unsigned)scan->Num);

    offset += snprintf(body + offset, HTTP_WIFI_PROV_RESP_MAX - offset, "{\"support_5g\":false,\"aps\":[");
    for (uint32_t i = 0; i < scan->Num && i < ENTITY_WIFI_SCAN_AP_NUM_MAX && offset + 96 < HTTP_WIFI_PROV_RESP_MAX; ++i)
    {
        if (i > 0)
        {
            offset += snprintf(body + offset, HTTP_WIFI_PROV_RESP_MAX - offset, ",");
        }
        offset += snprintf(body + offset, HTTP_WIFI_PROV_RESP_MAX - offset, "{\"ssid\":\"");
        offset = json_append_escaped(body, HTTP_WIFI_PROV_RESP_MAX, offset, scan->Ap_Infos[i].Ssid);
        offset += snprintf(body + offset,
                           HTTP_WIFI_PROV_RESP_MAX - offset,
                           "\",\"rssi\":%d,\"authmode\":%d}",
                           (int)scan->Ap_Infos[i].Rssi,
                           (int)scan->Ap_Infos[i].Auth);
    }
    snprintf(body + offset, HTTP_WIFI_PROV_RESP_MAX - offset, "]}");

    (void)Bsp_Wifi_Sta_Scan_Start(NULL);
    return http_send_response(fd, "200 OK", "application/json", body);
}

static int http_wifi_prov_save_wifi(const char *ssid, const char *password)
{
    Entity_Config_Net_Info_t *config = Entity_Get_Config_Net_Info();
    Entity_Dev_Config_Net_Info_t *dev_config = Entity_Get_Dev_Config_Net_Info();

    if (config == NULL || dev_config == NULL || ssid == NULL || ssid[0] == '\0')
    {
        return -1;
    }

    memset(config->Wifi_Info.Ssid, 0, sizeof(config->Wifi_Info.Ssid));
    memset(config->Wifi_Info.Key, 0, sizeof(config->Wifi_Info.Key));
    strncpy(config->Wifi_Info.Ssid, ssid, sizeof(config->Wifi_Info.Ssid) - 1);
    strncpy(config->Wifi_Info.Key, password ? password : "", sizeof(config->Wifi_Info.Key) - 1);
    dev_config->Magic_Header = MAGIC_HEADER_VALUE;
    dev_config->Flag_Bind = 0;
    dev_config->Flag_Wifi_Info_Vaild = 0;
    dev_config->Bind_Type = WIFI_BIND_TYPE;

    ENTITY_LOGI("[HTTP_WIFI_PROV][CFG_WRITE] magic=0x%08X flag_bind=%u bind_type=%u wifi_valid=%u ssid_len=%u ssid_hash=0x%08X pwd_len=%u pwd_hash=0x%08X\r\n",
                (unsigned)dev_config->Magic_Header,
                dev_config->Flag_Bind,
                dev_config->Bind_Type,
                dev_config->Flag_Wifi_Info_Vaild,
                (unsigned)http_wifi_prov_strnlen(config->Wifi_Info.Ssid, sizeof(config->Wifi_Info.Ssid)),
                (unsigned)http_wifi_prov_hash_string(config->Wifi_Info.Ssid, sizeof(config->Wifi_Info.Ssid)),
                (unsigned)http_wifi_prov_strnlen(config->Wifi_Info.Key, sizeof(config->Wifi_Info.Key)),
                (unsigned)http_wifi_prov_hash_string(config->Wifi_Info.Key, sizeof(config->Wifi_Info.Key)));

    return Entity_Save_Config_Net_Info_To_Flash();
}

static void http_wifi_prov_forget_large_keys(void)
{
    int config_ret = Bsp_Flash_Delete_Key("config_net_data");
    int token_ret = Device_Api_Clear_Device_Token();

    ENTITY_LOGI("[HTTP_WIFI_PROV][EF_ENV] forget large keys config_net_data_ret=%d http_device_token_ret=%d\r\n",
                config_ret,
                token_ret);
}

static int http_wifi_prov_handle_submit(int fd, const char *body)
{
    cJSON *root = NULL;
    cJSON *ssid_item = NULL;
    cJSON *password_item = NULL;
    const char *ssid = NULL;
    const char *password = "";
    int persist_ret = 0;
    int sta_config_ret = 0;
    int sta_connect_ret = 0;
    char response[256];

    ENTITY_LOGI("[HTTP_WIFI_PROV] POST /submit body_len=%u\r\n", (unsigned)(body ? strlen(body) : 0));
    root = cJSON_Parse(body ? body : "");
    if (root == NULL)
    {
        return http_send_response(fd, "400 Bad Request", "application/json", "{\"ok\":false,\"error\":\"invalid_json\"}");
    }

    ssid_item = cJSON_GetObjectItem(root, "ssid");
    password_item = cJSON_GetObjectItem(root, "password");
    if (!cJSON_IsString(ssid_item) || ssid_item->valuestring == NULL || ssid_item->valuestring[0] == '\0')
    {
        cJSON_Delete(root);
        return http_send_response(fd, "400 Bad Request", "application/json", "{\"ok\":false,\"error\":\"missing_ssid\"}");
    }
    if (cJSON_IsString(password_item) && password_item->valuestring != NULL)
    {
        password = password_item->valuestring;
    }
    ssid = ssid_item->valuestring;
    s_http_wifi_prov_sta_submitted = 1;
    s_http_wifi_prov_sta_online = 0;

    http_wifi_prov_log_easyflash_env("before_save_wifi");
    persist_ret = http_wifi_prov_save_wifi(ssid, password);
    http_wifi_prov_log_easyflash_env("after_save_wifi");
    if (persist_ret != 0)
    {
        http_wifi_prov_forget_large_keys();
        http_wifi_prov_log_easyflash_env("after_forget_large_keys");
        persist_ret = http_wifi_prov_save_wifi(ssid, password);
        http_wifi_prov_log_easyflash_env("after_retry_save_wifi");
    }
    ENTITY_LOGI("[HTTP_WIFI_PROV] save wifi ssid=%s pass_len=%u persist_ret=%d\r\n",
                ssid,
                (unsigned)strlen(password),
                persist_ret);

    sta_config_ret = Bsp_Wifi_Sta_Mode_Config((char *)ssid, (char *)password, ENTITY_SECURITY_AUTO);
    if (sta_config_ret == 0)
    {
        sta_connect_ret = Bsp_Wifi_Sta_Conncet();
    }

    ENTITY_LOGI("[HTTP_WIFI_PROV] submit result persist_ret=%d sta_config_ret=%d sta_connect_ret=%d\r\n",
                persist_ret,
                sta_config_ret,
                sta_connect_ret);

    snprintf(response,
             sizeof(response),
             "{\"ok\":%s,\"persist_ret\":%d,\"sta_config_ret\":%d,\"sta_connect_ret\":%d,\"pair_code_url\":\"/pair-code\"}",
             (sta_config_ret == 0 && sta_connect_ret == 0) ? "true" : "false",
             persist_ret,
             sta_config_ret,
             sta_connect_ret);
    cJSON_Delete(root);
    return http_send_response(fd,
                              (sta_config_ret == 0 && sta_connect_ret == 0) ? "200 OK" : "500 Internal Server Error",
                              "application/json",
                              response);
}

static int http_wifi_prov_handle_pair_code(int fd)
{
    char pair_code[16];
    char device_id[80];
    int expires = 0;
    int poll_after = 0;
    int has_token = 0;
    int ret = 0;
    char body[256];

    if (!s_http_wifi_prov_sta_online)
    {
        if (s_http_wifi_prov_sta_submitted)
        {
            ENTITY_LOGI("[HTTP_WIFI_PROV] GET /pair-code phase=wifi_connecting sta_submitted=%d sta_online=%d\r\n",
                        s_http_wifi_prov_sta_submitted,
                        s_http_wifi_prov_sta_online);
            snprintf(body,
                     sizeof(body),
                     "{\"ok\":true,\"phase\":\"wifi_connecting\",\"bound\":false,\"pair_code\":\"\",\"device_id\":\"\",\"expires_in\":0,\"poll_after\":1}");
        }
        else
        {
            ENTITY_LOGI("[HTTP_WIFI_PROV] GET /pair-code phase=idle sta_submitted=%d sta_online=%d\r\n",
                        s_http_wifi_prov_sta_submitted,
                        s_http_wifi_prov_sta_online);
            snprintf(body,
                     sizeof(body),
                     "{\"ok\":true,\"phase\":\"idle\",\"bound\":false,\"pair_code\":\"\",\"device_id\":\"\",\"expires_in\":0,\"poll_after\":1}");
        }
        return http_send_response(fd, "200 OK", "application/json", body);
    }

    ret = Device_Api_Get_Pending_Pair_Code(pair_code,
                                           sizeof(pair_code),
                                           device_id,
                                           sizeof(device_id),
                                           &expires,
                                           &poll_after,
                                           &has_token);
    ENTITY_LOGI("[HTTP_WIFI_PROV] GET /pair-code ret=%d has_token=%d pair_code=%s\r\n",
                ret,
                has_token,
                pair_code[0] != '\0' ? pair_code : "none");
    if (pair_code[0] != '\0')
    {
        ENTITY_LOGI("[HTTP_WIFI_PROV] pair-code ready code=%s device=%s\r\n", pair_code, device_id);
    }

    snprintf(body,
             sizeof(body),
             "{\"ok\":true,\"bound\":%s,\"pair_code\":\"%s\",\"device_id\":\"%s\",\"expires_in\":%d,\"poll_after\":%d}",
             has_token ? "true" : "false",
             pair_code,
             device_id,
             expires,
             poll_after);
    return http_send_response(fd, "200 OK", "application/json", body);
}

static int http_wifi_prov_handle_clear_env(int fd)
{
    int ret = Bsp_Flash_Reset_Env_To_Default();
    char body[128];

    s_http_wifi_prov_sta_submitted = 0;
    s_http_wifi_prov_sta_online = 0;
    (void)Device_Api_Clear_Device_Token();

    ENTITY_LOGW("[HTTP_WIFI_PROV][EF_ENV] POST /clear-env ret=%d\r\n", ret);
    snprintf(body, sizeof(body), "{\"ok\":%s,\"ret\":%d}", ret == 0 ? "true" : "false", ret);
    return http_send_response(fd,
                              ret == 0 ? "200 OK" : "500 Internal Server Error",
                              "application/json",
                              body);
}

static const char *http_find_body(char *request, int request_len)
{
    char *body = strstr(request, "\r\n\r\n");
    (void)request_len;
    return body ? body + 4 : "";
}

static void http_wifi_prov_handle_client(int fd)
{
    char *request = s_http_wifi_prov_request;
    int len = recv(fd, request, HTTP_WIFI_PROV_REQ_MAX - 1, 0);

    if (len <= 0)
    {
        ENTITY_LOGW("[HTTP_WIFI_PROV] client read failed len=%d\r\n", len);
        return;
    }
    request[len] = '\0';

    if (strncmp(request, "GET /scan", 9) == 0)
    {
        (void)http_wifi_prov_handle_scan(fd);
    }
    else if (strncmp(request, "POST /submit", 12) == 0)
    {
        (void)http_wifi_prov_handle_submit(fd, http_find_body(request, len));
    }
    else if (strncmp(request, "GET /pair-code", 14) == 0)
    {
        (void)http_wifi_prov_handle_pair_code(fd);
    }
    else if (strncmp(request, "POST /clear-env", 15) == 0)
    {
        (void)http_wifi_prov_handle_clear_env(fd);
    }
    else if (http_request_is_root_get(request))
    {
        ENTITY_LOGI("[HTTP_WIFI_PROV] GET /\r\n");
        (void)http_send_response(fd, "200 OK", "text/html; charset=utf-8", s_http_wifi_prov_index_html);
    }
    else if (strncmp(request, "GET /", 5) == 0)
    {
        (void)http_send_redirect_to_root(fd, request);
    }
    else
    {
        ENTITY_LOGW("[HTTP_WIFI_PROV] unsupported request %.24s\r\n", request);
        (void)http_send_response(fd, "404 Not Found", "application/json", "{\"ok\":false,\"error\":\"not_found\"}");
    }
}

static void http_wifi_prov_server_thread(beken_thread_arg_t arg)
{
    (void)arg;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    s_http_wifi_prov_listen_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s_http_wifi_prov_listen_fd < 0)
    {
        ENTITY_LOGE("[HTTP_WIFI_PROV] socket create failed fd=%d\r\n", s_http_wifi_prov_listen_fd);
        s_http_wifi_prov_running = 0;
        rtos_delete_thread(NULL);
        return;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(HTTP_WIFI_PROV_PORT);

    if (bind(s_http_wifi_prov_listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0 ||
        listen(s_http_wifi_prov_listen_fd, 2) != 0)
    {
        ENTITY_LOGE("[HTTP_WIFI_PROV] bind/listen failed fd=%d\r\n", s_http_wifi_prov_listen_fd);
        close(s_http_wifi_prov_listen_fd);
        s_http_wifi_prov_listen_fd = -1;
        s_http_wifi_prov_running = 0;
        rtos_delete_thread(NULL);
        return;
    }

    ENTITY_LOGI("[HTTP_WIFI_PROV] HTTP server listening on http://%s/\r\n", HTTP_WIFI_PROV_AP_IP);
    while (s_http_wifi_prov_running)
    {
        fd_set readfds;
        struct timeval timeout;
        int select_ret = 0;
        int client_fd = -1;

        FD_ZERO(&readfds);
        FD_SET(s_http_wifi_prov_listen_fd, &readfds);
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        select_ret = select(s_http_wifi_prov_listen_fd + 1, &readfds, NULL, NULL, &timeout);
        if (select_ret <= 0 || !FD_ISSET(s_http_wifi_prov_listen_fd, &readfds))
        {
            continue;
        }

        client_len = sizeof(client_addr);
        client_fd = accept(s_http_wifi_prov_listen_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0)
        {
            continue;
        }
        http_wifi_prov_handle_client(client_fd);
        close(client_fd);
    }

    if (s_http_wifi_prov_listen_fd >= 0)
    {
        close(s_http_wifi_prov_listen_fd);
        s_http_wifi_prov_listen_fd = -1;
    }
    s_http_wifi_prov_thread = NULL;
    rtos_delete_thread(NULL);
}

int Http_Wifi_Provision_Server_Start(uint8_t reset_binding)
{
    char ap_ssid[32];
    int ret = 0;

    if (s_http_wifi_prov_running)
    {
        ENTITY_LOGI("[HTTP_WIFI_PROV] already running\r\n");
        return 0;
    }

    http_wifi_prov_build_ap_ssid(ap_ssid, sizeof(ap_ssid));
    Bsp_Wifi_Init(ENTITY_WIFI_AP_MODE);
    ret = Bsp_Wifi_Ap_Mode_Config(ap_ssid, "", HTTP_WIFI_PROV_AP_IP, HTTP_WIFI_PROV_AP_IP, HTTP_WIFI_PROV_AP_MASK);
    if (ret == 0)
    {
        ret = Bsp_Wifi_Ap_Start();
    }
    if (ret != 0)
    {
        ENTITY_LOGE("[HTTP_WIFI_PROV] AP start failed ssid=%s ret=%d\r\n", ap_ssid, ret);
        return ret;
    }

    s_http_wifi_prov_sta_submitted = 0;
    s_http_wifi_prov_sta_online = 0;
    s_http_wifi_prov_running = 1;
    http_wifi_prov_pause_standby_countdown();
    ENTITY_LOGI("[CAPTIVE_DNS] using Beken built-in DHCP DNS handler answer=%s\r\n", HTTP_WIFI_PROV_AP_IP);

    ret = rtos_create_thread(&s_http_wifi_prov_thread,
                             HTTP_WIFI_PROV_PRIO,
                             "http_wifi_prov",
                             http_wifi_prov_server_thread,
                             HTTP_WIFI_PROV_STACK,
                             NULL);
    if (ret != kNoErr)
    {
        ENTITY_LOGE("[HTTP_WIFI_PROV] thread create failed ret=%d\r\n", ret);
        s_http_wifi_prov_running = 0;
        (void)Bsp_Wifi_Ap_Stop();
        http_wifi_prov_resume_standby_countdown();
        return -1;
    }

    ENTITY_LOGI("[HTTP_WIFI_PROV] AP web provisioning started ssid=%s url=http://%s/ reset_binding=%u\r\n",
                ap_ssid,
                HTTP_WIFI_PROV_AP_IP,
                (unsigned)reset_binding);
    (void)Bsp_Wifi_Sta_Scan_Start(NULL);
    return 0;
}

void Http_Wifi_Provision_Server_Stop(void)
{
    int fd = s_http_wifi_prov_listen_fd;

    if (!s_http_wifi_prov_running)
    {
        return;
    }

    s_http_wifi_prov_running = 0;
    if (fd >= 0)
    {
        close(fd);
        s_http_wifi_prov_listen_fd = -1;
    }
    (void)Bsp_Wifi_Ap_Stop();
    http_wifi_prov_resume_standby_countdown();
    ENTITY_LOGI("[HTTP_WIFI_PROV] AP web provisioning stopped\r\n");
}

int Http_Wifi_Provision_Server_Is_Running(void)
{
    return s_http_wifi_prov_running;
}

void Http_Wifi_Provision_Server_On_Sta_Connected(void)
{
    if (!s_http_wifi_prov_running)
    {
        return;
    }

    s_http_wifi_prov_sta_online = 1;
    ENTITY_LOGI("[HTTP_WIFI_PROV] phase=wifi_connected sta_submitted=%d sta_online=%d\r\n",
                s_http_wifi_prov_sta_submitted,
                s_http_wifi_prov_sta_online);
}
