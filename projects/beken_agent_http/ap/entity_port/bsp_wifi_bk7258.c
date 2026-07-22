//bsp_wifi_bk7258.c
#include "bsp_wifi.h"

#include <string.h>

#include "bk_wifi.h"
#include "common/bk_include.h"
#include "components/event.h"
#include "components/log.h"
#include "components/netif.h"
#include "entity_dev_info.h"
#include "entity_iot_cloud.h"
#include "entity_wifi.h"
#include "modules/wifi.h"
#include "modules/wifi_types.h"

#define TAG "entity_wifi_bk"

typedef void (*Wifi_Event_App_Cb_f)(uint8_t event);
typedef void (*Wifi_Scan_Done_Cb_f)(void *result);

static Wifi_Event_App_Cb_f s_wifi_event_cb;
static Wifi_Scan_Done_Cb_f s_scan_done_cb;
static Entity_Wifi_Sta_Scan_Result_t s_scan_results;
static wifi_sta_config_t s_sta_cfg;
static wifi_ap_config_t s_ap_cfg;
static uint8_t s_auto_reconnect = 1;
static uint8_t s_event_registered = 0;

static wifi_security_t Bsp_Wifi_Map_Security(uint8_t auth_mode)
{
    switch ((Entity_Wifi_Auth_Mode_e)auth_mode)
    {
        case ENTITY_SECURITY_WEP:
            return WIFI_SECURITY_WEP;
        case ENTITY_SECURITY_WPAPSK:
            return WIFI_SECURITY_WPA_TKIP;
        case ENTITY_SECURITY_WPA2PSK:
            return WIFI_SECURITY_WPA2_TKIP;
        case ENTITY_SECURITY_WPAPSK_WPA2PSK_MIX:
            return WIFI_SECURITY_WPA2_MIXED;
        case ENTITY_SECURITY_AUTO:
            return WIFI_SECURITY_AUTO;
        case ENTITY_WIFI_SECURITY_OPEN:
        default:
            return WIFI_SECURITY_NONE;
    }
}

static Entity_Wifi_Auth_Mode_e Bsp_Wifi_Map_Auth(wifi_security_t security)
{
    switch (security)
    {
        case WIFI_SECURITY_NONE:
            return ENTITY_WIFI_SECURITY_OPEN;
        case WIFI_SECURITY_WEP:
            return ENTITY_SECURITY_WEP;
        case WIFI_SECURITY_WPA_TKIP:
        case WIFI_SECURITY_WPA_AES:
            return ENTITY_SECURITY_WPAPSK;
        case WIFI_SECURITY_WPA2_TKIP:
        case WIFI_SECURITY_WPA2_AES:
            return ENTITY_SECURITY_WPA2PSK;
        default:
            return ENTITY_SECURITY_WPAPSK_WPA2PSK_MIX;
    }
}

static int Bsp_Wifi_Update_Scan_Results(void)
{
    wifi_scan_result_t result = {0};
    uint32_t out = 0;
    bk_err_t ret;

    memset(&s_scan_results, 0, sizeof(s_scan_results));
    ret = bk_wifi_scan_get_result(&result);
    if (ret != BK_OK)
    {
        BK_LOGE(TAG, "scan_get_result failed ret=%d\r\n", ret);
        return -1;
    }

    BK_LOGI(TAG, "scan_get_result raw_ap_num=%u\r\n", (unsigned int)result.ap_num);
    for (uint32_t i = 0; (i < result.ap_num) && (out < ENTITY_WIFI_SCAN_AP_NUM_MAX); ++i)
    {
        if (result.aps[i].ssid[0] == '\0')
        {
            continue;
        }

        strncpy(s_scan_results.Ap_Infos[out].Ssid,
                result.aps[i].ssid,
                sizeof(s_scan_results.Ap_Infos[out].Ssid) - 1);
        s_scan_results.Ap_Infos[out].Rssi = (int8_t)result.aps[i].rssi;
        s_scan_results.Ap_Infos[out].Auth = Bsp_Wifi_Map_Auth(result.aps[i].security);
        out++;
    }

    s_scan_results.Num = out;
    bk_wifi_scan_free_result(&result);
    BK_LOGI(TAG, "scan_update filtered_ap_num=%u\r\n", (unsigned int)s_scan_results.Num);
    return 0;
}

static int Bsp_Wifi_Event_Callback(void *arg, event_module_t mod, int event_id, void *data)
{
    (void)arg;
    (void)mod;

    switch (event_id)
    {
        case EVENT_WIFI_STA_CONNECTED:
            BK_LOGI(TAG, "event STA_CONNECTED\r\n");
            if (s_wifi_event_cb)
            {
#if CONFIG_HTTP_AGENT_ENABLE_MQTT
                s_wifi_event_cb(ENTITY_WIFI_EVT_STA_CONNECTED);
#else
                BK_LOGI(TAG, "[HTTP_AGENT][MQTT_DISABLED] STA_CONNECTED: skip SDK MQTT Wi-Fi callback\r\n");
#endif
            }
            break;

        case EVENT_WIFI_STA_DISCONNECTED:
        {
            wifi_event_sta_disconnected_t *disc = (wifi_event_sta_disconnected_t *)data;
            BK_LOGW(TAG, "event STA_DISCONNECTED reason=%d local=%d auto_reconnect=%u\r\n",
                    disc ? (int)disc->disconnect_reason : -1,
                    disc ? (int)disc->local_generated : -1,
                    (unsigned int)s_auto_reconnect);
            if (s_auto_reconnect &&
                disc != NULL &&
                !disc->local_generated &&
                disc->disconnect_reason != WIFI_REASON_RESERVED)
            {
                bk_err_t reconn_ret = bk_wifi_sta_connect();
                BK_LOGW(TAG, "auto_reconnect ret=%d\r\n", reconn_ret);
            }

            if (s_wifi_event_cb)
            {
#if CONFIG_HTTP_AGENT_ENABLE_MQTT
                s_wifi_event_cb(ENTITY_WIFI_EVT_STA_DISCONNECTED);
#else
                BK_LOGI(TAG, "[HTTP_AGENT][MQTT_DISABLED] STA_DISCONNECTED: notify product only\r\n");
                Entity_Set_Dev_Status(DEV_WIFI_DISCONNECT_STATE);
#endif
            }
            break;
        }

        case EVENT_WIFI_SCAN_DONE:
        {
            int update_ret = Bsp_Wifi_Update_Scan_Results();
            BK_LOGI(TAG, "event SCAN_DONE update_ret=%d cb=%p app_cb=%p\r\n",
                    update_ret, s_scan_done_cb, s_wifi_event_cb);
            if (s_scan_done_cb)
            {
                s_scan_done_cb(&s_scan_results);
            }
            if (s_wifi_event_cb)
            {
                s_wifi_event_cb(ENTITY_WIFI_EVT_SCAN_DONE);
            }
            break;
        }

        default:
            BK_LOGD(TAG, "event id=%d\r\n", event_id);
            break;
    }

    return 0;
}

static int Bsp_Netif_Event_Callback(void *arg, event_module_t mod, int event_id, void *data)
{
    (void)arg;
    (void)mod;
    (void)data;

    if ((event_id == EVENT_NETIF_GOT_IP4) && s_wifi_event_cb)
    {
        BK_LOGI(TAG, "event GOT_IP4\r\n");
#if CONFIG_HTTP_AGENT_ENABLE_MQTT
        s_wifi_event_cb(ENTITY_WIFI_EVT_IP_CHANGE);
#else
        BK_LOGI(TAG, "[HTTP_AGENT][MQTT_DISABLED] GOT_IP4: notify product DEV_WIFI_CONNECTED_STATE only\r\n");
        Entity_Set_Dev_Status(DEV_WIFI_CONNECTED_STATE);
#endif
    }

    return 0;
}

void Bsp_Wifi_Init(uint8_t mode)
{
    (void)mode;

    if (!s_event_registered)
    {
        bk_err_t wifi_ret = bk_event_register_cb(EVENT_MOD_WIFI, EVENT_ID_ALL, Bsp_Wifi_Event_Callback, NULL);
        bk_err_t netif_ret = bk_event_register_cb(EVENT_MOD_NETIF, EVENT_ID_ALL, Bsp_Netif_Event_Callback, NULL);
        BK_LOGI(TAG, "Bsp_Wifi_Init mode=%u register wifi_ret=%d netif_ret=%d\r\n",
                (unsigned int)mode, wifi_ret, netif_ret);
        s_event_registered = 1;
        return;
    }

    BK_LOGI(TAG, "Bsp_Wifi_Init mode=%u already registered\r\n", (unsigned int)mode);
}

int Bsp_Wifi_Sta_Mode_Config(char *ssid, char *passwd, uint8_t auth_mode)
{
    bk_err_t ret;

    if (ssid == NULL)
    {
        ssid = "";
    }
    if (passwd == NULL)
    {
        passwd = "";
    }

    memset(&s_sta_cfg, 0, sizeof(s_sta_cfg));
    strncpy(s_sta_cfg.ssid, ssid, sizeof(s_sta_cfg.ssid) - 1);
    strncpy(s_sta_cfg.password, passwd, sizeof(s_sta_cfg.password) - 1);
    s_sta_cfg.security = Bsp_Wifi_Map_Security(auth_mode);

    ret = bk_wifi_sta_set_config(&s_sta_cfg);
    BK_LOGI(TAG, "Bsp_Wifi_Sta_Mode_Config ssid=%s pass_len=%u auth=%u security=%d ret=%d\r\n",
            s_sta_cfg.ssid,
            (unsigned int)strlen(s_sta_cfg.password),
            (unsigned int)auth_mode,
            (int)s_sta_cfg.security,
            ret);
    return (ret == BK_OK) ? 0 : -1;
}

int Bsp_Wifi_Sta_Scan_Start(void *callback)
{
    bk_err_t ret;

    s_scan_done_cb = (Wifi_Scan_Done_Cb_f)callback;
    ret = bk_wifi_scan_start(NULL);
    BK_LOGI(TAG, "Bsp_Wifi_Sta_Scan_Start cb=%p ret=%d\r\n", callback, ret);
    return (ret == BK_OK) ? 0 : -1;
}

void *Bsp_Wifi_Get_Scan_Results(void)
{
    return &s_scan_results;
}

int Bsp_Wifi_Copy_Scan_Results(void *result, unsigned int len)
{
    if ((result == NULL) || (len < sizeof(s_scan_results)))
    {
        return -1;
    }

    memcpy(result, &s_scan_results, sizeof(s_scan_results));
    return 0;
}

void Bsp_Wifi_Get_Macaddr(uint8_t *mac_addr)
{
    if (mac_addr != NULL)
    {
        bk_wifi_sta_get_mac(mac_addr);
    }
}

int Bsp_Wifi_Sta_Conncet(void)
{
    bk_err_t ret = bk_wifi_sta_start();
    BK_LOGI(TAG, "Bsp_Wifi_Sta_Conncet ssid=%s security=%d ret=%d\r\n",
            s_sta_cfg.ssid, (int)s_sta_cfg.security, ret);
    return (ret == BK_OK) ? 0 : -1;
}

int Bsp_Wifi_Sta_Disconnect(void)
{
    bk_err_t ret = bk_wifi_sta_stop();
    BK_LOGI(TAG, "Bsp_Wifi_Sta_Disconnect ret=%d\r\n", ret);
    return (ret == BK_OK) ? 0 : -1;
}

void Bsp_Wifi_Sta_Auto_Reconnect_Enable(uint8_t enable)
{
    s_auto_reconnect = enable ? 1 : 0;
    BK_LOGI(TAG, "Bsp_Wifi_Sta_Auto_Reconnect_Enable enable=%u\r\n", (unsigned int)s_auto_reconnect);
}

void Bsp_Register_Wifi_Event_App_Callback(void *cb)
{
    s_wifi_event_cb = (Wifi_Event_App_Cb_f)cb;
#if CONFIG_HTTP_AGENT_ENABLE_MQTT
    BK_LOGI(TAG, "Bsp_Register_Wifi_Event_App_Callback cb=%p\r\n", cb);
#else
    BK_LOGI(TAG, "Bsp_Register_Wifi_Event_App_Callback cb=%p [HTTP_AGENT][MQTT_DISABLED]\r\n", cb);
#endif
}

void Bsp_Print_Scan_Result(void *res)
{
    Entity_Wifi_Sta_Scan_Result_t *scan = (Entity_Wifi_Sta_Scan_Result_t *)res;
    if (scan == NULL)
    {
        scan = &s_scan_results;
    }

    BK_LOGI(TAG, "scan result count:%u\r\n", (unsigned int)scan->Num);
    for (uint32_t i = 0; (i < scan->Num) && (i < ENTITY_WIFI_SCAN_AP_NUM_MAX); ++i)
    {
        BK_LOGI(TAG, "[%u] ssid:%s rssi:%d auth:%d\r\n",
                (unsigned int)i,
                scan->Ap_Infos[i].Ssid,
                (int)scan->Ap_Infos[i].Rssi,
                (int)scan->Ap_Infos[i].Auth);
    }
}

int Bsp_Wifi_Ap_Start(void)
{
    bk_err_t ret = bk_wifi_ap_start();
    BK_LOGI(TAG, "Bsp_Wifi_Ap_Start ret=%d\r\n", ret);
    return (ret == BK_OK) ? 0 : -1;
}

int Bsp_Wifi_Ap_Stop(void)
{
    bk_err_t ret = bk_wifi_ap_stop();
    BK_LOGI(TAG, "Bsp_Wifi_Ap_Stop ret=%d\r\n", ret);
    return (ret == BK_OK) ? 0 : -1;
}

int Bsp_Wifi_Ap_Mode_Config(char *ap_ssid, char *ap_key, const char *local_ip, const char *gw_ip, const char *ip_mask)
{
    netif_ip4_config_t ip4 = {0};

    if (ap_ssid == NULL)
    {
        ap_ssid = "";
    }
    if (ap_key == NULL)
    {
        ap_key = "";
    }

    memset(&s_ap_cfg, 0, sizeof(s_ap_cfg));
    strncpy(s_ap_cfg.ssid, ap_ssid, sizeof(s_ap_cfg.ssid) - 1);
    strncpy(s_ap_cfg.password, ap_key, sizeof(s_ap_cfg.password) - 1);
    s_ap_cfg.security = (ap_key[0] == '\0') ? WIFI_SECURITY_NONE : WIFI_SECURITY_WPA2_MIXED;

    bk_err_t ret = bk_wifi_ap_set_config(&s_ap_cfg);
    BK_LOGI(TAG, "Bsp_Wifi_Ap_Mode_Config ssid=%s pass_len=%u security=%d disable_dns_server=%u ret=%d\r\n",
            s_ap_cfg.ssid,
            (unsigned int)strlen(s_ap_cfg.password),
            (int)s_ap_cfg.security,
            (unsigned int)s_ap_cfg.disable_dns_server,
            ret);
    if (ret != BK_OK)
    {
        return -1;
    }

    if ((local_ip != NULL) && (gw_ip != NULL) && (ip_mask != NULL))
    {
        strncpy(ip4.ip, local_ip, sizeof(ip4.ip) - 1);
        strncpy(ip4.gateway, gw_ip, sizeof(ip4.gateway) - 1);
        strncpy(ip4.mask, ip_mask, sizeof(ip4.mask) - 1);
        strncpy(ip4.dns, gw_ip, sizeof(ip4.dns) - 1);
        ret = bk_netif_set_ip4_config(NETIF_IF_AP, &ip4);
        BK_LOGI(TAG, "Bsp_Wifi_Ap_Mode_Config ip=%s gw=%s mask=%s dns=%s ret=%d\r\n",
                ip4.ip, ip4.gateway, ip4.mask, ip4.dns, ret);
        if (ret != BK_OK)
        {
            return -1;
        }
    }

    return 0;
}

int Bsp_Wifi_Load_Signal_Level_Quality(uint8_t *level, uint8_t *quality)
{
    wifi_link_status_t status = {0};
    int rssi;

    if ((level == NULL) || (quality == NULL))
    {
        return -1;
    }

    if (bk_wifi_sta_get_link_status(&status) != BK_OK)
    {
        return -1;
    }

    rssi = status.rssi;
    if (rssi >= -50)
    {
        *level = 4;
        *quality = 100;
    }
    else if (rssi >= -60)
    {
        *level = 3;
        *quality = 75;
    }
    else if (rssi >= -70)
    {
        *level = 2;
        *quality = 50;
    }
    else
    {
        *level = 1;
        *quality = 25;
    }

    return 0;
}
