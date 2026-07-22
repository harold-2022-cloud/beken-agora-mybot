//bsp_ble_bk7258.c
#include "bsp_ble.h"

#include <string.h>

#include "common/bk_include.h"
#include "components/bluetooth/bk_dm_bluetooth.h"
#include "components/bluetooth/bk_dm_bt.h"
#include "components/bluetooth/bk_dm_bt_types.h"
#include "components/bluetooth/bk_dm_gap_ble.h"
#include "components/bluetooth/bk_dm_gap_ble_types.h"
#include "components/bluetooth/bk_dm_gatts.h"
#include "components/bluetooth/bk_dm_gatt_types.h"
#include "components/log.h"
#include "entity_ble_gatt.h"
#include "os/os.h"

#define TAG "entity_ble_bk"

#define SYNC_TIMEOUT_MS 4000
#define BLE_ADV_HANDLE 0
#define DATAINOUT_LEN 254

#define ENTITY_BK_SVC_UUID      0x1910
#define ENTITY_BK_DATA_IN_UUID  0x2b11
#define ENTITY_BK_DATA_OUT_UUID 0x2b10

static beken_semaphore_t s_sema = NULL;
static bk_gatt_if_t s_gatts_if = 0;
static uint16_t s_conn_id = 0xFFFF;
static bk_bd_addr_t s_remote_addr;
static uint8_t s_initialized;
static uint8_t s_adv_created;

static uint16_t s_svc_handle;
static uint16_t s_data_in_handle;
static uint16_t s_data_out_handle;
static uint16_t s_ccc_handle;

static uint16_t *const s_attr_handles[4] = {
    &s_svc_handle,
    &s_data_in_handle,
    &s_data_out_handle,
    &s_ccc_handle,
};

static uint16_t s_ccc_state;
static uint8_t s_data_in_val[DATAINOUT_LEN];
static uint8_t s_data_out_val[DATAINOUT_LEN];
static uint8_t s_adv_data[32];
static uint8_t s_adv_len;
static uint8_t s_scan_rsp_data[32];
static uint8_t s_scan_rsp_len;

#define ATTR_TYPE_16(u) { .len = BK_UUID_LEN_16, .uuid = { .uuid16 = (u) } }
#define ATTR_VAL(l, v) { .attr_max_len = (l), .attr_len = (l), .attr_value = (v) }

static const bk_gatts_attr_db_t s_attr_db[] = {
    {
        .att_desc = {
            .attr_type = ATTR_TYPE_16(BK_GATT_UUID_PRI_SERVICE),
            .attr_content = ATTR_TYPE_16(ENTITY_BK_SVC_UUID),
        },
    },
    {
        .att_desc = {
            .attr_type = ATTR_TYPE_16(BK_GATT_UUID_CHAR_DECLARE),
            .attr_content = ATTR_TYPE_16(ENTITY_BK_DATA_IN_UUID),
            .value = ATTR_VAL(sizeof(s_data_in_val), s_data_in_val),
            .prop = BK_GATT_CHAR_PROP_BIT_WRITE | BK_GATT_CHAR_PROP_BIT_WRITE_NR,
            .perm = BK_GATT_PERM_WRITE,
        },
        .attr_control = { .auto_rsp = BK_GATT_RSP_BY_APP },
    },
    {
        .att_desc = {
            .attr_type = ATTR_TYPE_16(BK_GATT_UUID_CHAR_DECLARE),
            .attr_content = ATTR_TYPE_16(ENTITY_BK_DATA_OUT_UUID),
            .value = ATTR_VAL(sizeof(s_data_out_val), s_data_out_val),
            .prop = BK_GATT_CHAR_PROP_BIT_NOTIFY | BK_GATT_CHAR_PROP_BIT_INDICATE,
            .perm = BK_GATT_PERM_READ,
        },
        .attr_control = { .auto_rsp = BK_GATT_AUTO_RSP },
    },
    {
        .att_desc = {
            .attr_type = ATTR_TYPE_16(BK_GATT_UUID_CHAR_CLIENT_CONFIG),
            .value = ATTR_VAL(sizeof(s_ccc_state), (uint8_t *)&s_ccc_state),
            .perm = BK_GATT_PERM_READ | BK_GATT_PERM_WRITE,
        },
        .attr_control = { .auto_rsp = BK_GATT_RSP_BY_APP },
    },
};

#define ATTR_DB_SIZE (sizeof(s_attr_db) / sizeof(s_attr_db[0]))

static int Bsp_Ble_Sema_Take(void)
{
    return rtos_get_semaphore(&s_sema, SYNC_TIMEOUT_MS);
}

static int Bsp_Ble_Sema_Give(void)
{
    return rtos_set_semaphore(&s_sema);
}

static void Bsp_Ble_Gap_Callback(bk_ble_gap_cb_event_t event, bk_ble_gap_cb_param_t *param)
{
    BK_LOGI(TAG, "gap_event=%d\r\n", event);

    switch (event)
    {
        case BK_BLE_GAP_EXT_ADV_PARAMS_SET_COMPLETE_EVT:
        case BK_BLE_GAP_EXT_ADV_SET_RAND_ADDR_COMPLETE_EVT:
        case BK_BLE_GAP_EXT_ADV_DATA_RAW_SET_COMPLETE_EVT:
        case BK_BLE_GAP_EXT_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT:
        case BK_BLE_GAP_EXT_ADV_START_COMPLETE_EVT:
        case BK_BLE_GAP_EXT_ADV_STOP_COMPLETE_EVT:
        case BK_BLE_GAP_EXT_ADV_SET_REMOVE_COMPLETE_EVT:
            Bsp_Ble_Sema_Give();
            break;
        case BK_BLE_GAP_UPDATE_CONN_PARAMS_REQ_EVT:
        {
            struct ble_conntection_update_param_req *p = (typeof(p))param;
            p->accept = 1;
            break;
        }
        default:
            break;
    }
}

static int32_t Bsp_Ble_Gatts_Callback(bk_gatts_cb_event_t event,
                                      bk_gatt_if_t gatts_if,
                                      bk_ble_gatts_cb_param_t *comm_param)
{
    BK_LOGI(TAG, "gatts_event=%d gatts_if=%d\r\n", event, gatts_if);

    switch (event)
    {
        case BK_GATTS_REG_EVT:
        {
            struct gatts_reg_evt_param *p = (typeof(p))comm_param;
            s_gatts_if = p->gatt_if;
            BK_LOGI(TAG, "gatts_reg gatt_if=%d\r\n", s_gatts_if);
            Bsp_Ble_Sema_Give();
            break;
        }
        case BK_GATTS_CREAT_ATTR_TAB_EVT:
        {
            struct gatts_add_attr_tab_evt_param *p = (typeof(p))comm_param;
            for (int i = 0; (i < (int)p->num_handle) && (i < (int)ATTR_DB_SIZE); ++i)
            {
                *s_attr_handles[i] = p->handles[i];
            }
            BK_LOGI(TAG, "gatts_attr_tab num=%d svc=%u in=%u out=%u ccc=%u\r\n",
                    (int)p->num_handle,
                    (unsigned int)s_svc_handle,
                    (unsigned int)s_data_in_handle,
                    (unsigned int)s_data_out_handle,
                    (unsigned int)s_ccc_handle);
            Bsp_Ble_Sema_Give();
            break;
        }
        case BK_GATTS_START_EVT:
        case BK_GATTS_STOP_EVT:
        case BK_GATTS_UNREG_EVT:
            Bsp_Ble_Sema_Give();
            break;
        case BK_GATTS_WRITE_EVT:
        {
            struct gatts_write_evt_param *p = (typeof(p))comm_param;
            BK_LOGI(TAG, "gatts_write handle=%u len=%u need_rsp=%d conn_id=%u\r\n",
                    (unsigned int)p->handle,
                    (unsigned int)p->len,
                    (int)p->need_rsp,
                    (unsigned int)p->conn_id);
            if (p->handle == s_data_in_handle)
            {
                Entity_Ble_Gatt_Data_In_Callback(p->value, p->len);
            }
            else if ((p->handle == s_ccc_handle) && (p->len == 2))
            {
                s_ccc_state = (uint16_t)(p->value[0] | (p->value[1] << 8));
                Entity_Ble_Gatt_Ccc_Cfg_Change_Callback(s_ccc_state);
            }

            if (p->need_rsp)
            {
                bk_gatt_rsp_t rsp;
                memset(&rsp, 0, sizeof(rsp));
                rsp.attr_value.auth_req = BK_GATT_AUTH_REQ_NONE;
                rsp.attr_value.handle = p->handle;
                rsp.attr_value.offset = p->offset;
                bk_ble_gatts_send_response(gatts_if, p->conn_id, p->trans_id, BK_GATT_OK, &rsp);
            }
            break;
        }
        case BK_GATTS_READ_EVT:
        {
            struct gatts_read_evt_param *p = (typeof(p))comm_param;
            if (p->need_rsp)
            {
                bk_gatt_rsp_t rsp;
                memset(&rsp, 0, sizeof(rsp));
                rsp.attr_value.auth_req = BK_GATT_AUTH_REQ_NONE;
                rsp.attr_value.handle = p->handle;
                rsp.attr_value.offset = p->offset;
                bk_ble_gatts_send_response(gatts_if, p->conn_id, p->trans_id, BK_GATT_OK, &rsp);
            }
            break;
        }
        case BK_GATTS_CONNECT_EVT:
        {
            struct gatts_connect_evt_param *p = (typeof(p))comm_param;
            s_conn_id = p->conn_id;
            memcpy(s_remote_addr, p->remote_bda, BK_BD_ADDR_LEN);
            BK_LOGI(TAG, "gatts_connect conn_id=%u\r\n", (unsigned int)s_conn_id);
            Entity_Ble_Connect_Envent_Callback(ENTITY_BLE_CONNECT_EVENT_CONNECTED, 0, s_remote_addr);
            break;
        }
        case BK_GATTS_DISCONNECT_EVT:
            BK_LOGI(TAG, "gatts_disconnect conn_id=%u\r\n", (unsigned int)s_conn_id);
            s_conn_id = 0xFFFF;
            Entity_Ble_Connect_Envent_Callback(ENTITY_BLE_CONNECT_EVENT_DISCONNECTED, 0, s_remote_addr);
            break;
        default:
            break;
    }

    return 0;
}

void Bsp_Ble_Init(void)
{
    int ret;

    if (s_initialized)
    {
        BK_LOGI(TAG, "Bsp_Ble_Init already initialized\r\n");
        return;
    }

    BK_LOGI(TAG, "Bsp_Ble_Init start\r\n");
    if (rtos_init_semaphore(&s_sema, 1) != 0)
    {
        BK_LOGE(TAG, "rtos_init_semaphore failed\r\n");
        return;
    }

    ret = bk_bluetooth_init();
    BK_LOGI(TAG, "bk_bluetooth_init ret=%d\r\n", ret);
#if !CONFIG_BLUETOOTH_AP
    ret = bk_bt_gap_set_visibility(BK_BT_CONNECTABLE, BK_BT_DISCOVERABLE);
    BK_LOGI(TAG, "bk_bt_gap_set_visibility ret=%d\r\n", ret);
#endif
    bk_ble_gap_register_callback(Bsp_Ble_Gap_Callback);
    bk_ble_gatts_register_callback(Bsp_Ble_Gatts_Callback);
    BK_LOGI(TAG, "ble callbacks registered\r\n");

    ret = bk_ble_gatts_app_register(0);
    BK_LOGI(TAG, "bk_ble_gatts_app_register ret=%d\r\n", ret);
    if (ret != 0)
    {
        return;
    }
    if (Bsp_Ble_Sema_Take() != kNoErr)
    {
        BK_LOGE(TAG, "wait gatts app register timeout\r\n");
        return;
    }

    ret = bk_ble_gatts_create_attr_tab(s_attr_db, s_gatts_if, ATTR_DB_SIZE, 30);
    BK_LOGI(TAG, "bk_ble_gatts_create_attr_tab ret=%d\r\n", ret);
    if (ret != 0)
    {
        return;
    }
    if (Bsp_Ble_Sema_Take() != kNoErr)
    {
        BK_LOGE(TAG, "wait create attr tab timeout\r\n");
        return;
    }

    ret = bk_ble_gatts_start_service(s_svc_handle);
    BK_LOGI(TAG, "bk_ble_gatts_start_service handle=%u ret=%d\r\n",
            (unsigned int)s_svc_handle, ret);
    if (ret != 0)
    {
        return;
    }
    if (Bsp_Ble_Sema_Take() != kNoErr)
    {
        BK_LOGE(TAG, "wait start service timeout\r\n");
        return;
    }

    ret = Bsp_Ble_Set_Device_Name("YLX");
    BK_LOGI(TAG, "Bsp_Ble_Set_Device_Name ret=%d\r\n", ret);
    s_initialized = 1;
    BK_LOGI(TAG, "Bsp_Ble_Init done\r\n");
}

int Bsp_Ble_Gatt_Notify_Send(unsigned char conn_index, unsigned char *send_data, unsigned short len)
{
    if ((s_conn_id == 0xFFFF) || (send_data == NULL))
    {
        BK_LOGW(TAG, "notify skip entity_conn_index=%u bk_conn_id=%u data=%p len=%u\r\n",
                (unsigned int)conn_index, (unsigned int)s_conn_id, send_data, (unsigned int)len);
        return -1;
    }

    /*
     * Entity stores a logical connection index (0 for the current single link).
     * BK GATTS send API requires the real BK conn_id reported by CONNECT_EVT.
     */
    int ret = bk_ble_gatts_send_indicate(s_gatts_if, s_conn_id, s_data_out_handle, len, send_data, 0);
    BK_LOGI(TAG, "notify entity_conn_index=%u bk_conn_id=%u len=%u ret=%d\r\n",
            (unsigned int)conn_index, (unsigned int)s_conn_id, (unsigned int)len, ret);
    return ret;
}

int Bsp_Ble_Gatt_Indicate_Send(unsigned char conn_index, unsigned char *send_data, unsigned short len)
{
    if ((s_conn_id == 0xFFFF) || (send_data == NULL))
    {
        BK_LOGW(TAG, "indicate skip entity_conn_index=%u bk_conn_id=%u data=%p len=%u\r\n",
                (unsigned int)conn_index, (unsigned int)s_conn_id, send_data, (unsigned int)len);
        return -1;
    }

    int ret = bk_ble_gatts_send_indicate(s_gatts_if, s_conn_id, s_data_out_handle, len, send_data, 1);
    BK_LOGI(TAG, "indicate entity_conn_index=%u bk_conn_id=%u len=%u ret=%d\r\n",
            (unsigned int)conn_index, (unsigned int)s_conn_id, (unsigned int)len, ret);
    return ret;
}

int Bsp_Ble_Set_Adv_Data(unsigned char *adv_data, unsigned char adv_len)
{
    if ((adv_data == NULL) || (adv_len > sizeof(s_adv_data)))
    {
        BK_LOGE(TAG, "Bsp_Ble_Set_Adv_Data invalid data=%p len=%u max=%u\r\n",
                adv_data, (unsigned int)adv_len, (unsigned int)sizeof(s_adv_data));
        return -1;
    }

    memcpy(s_adv_data, adv_data, adv_len);
    s_adv_len = adv_len;
    BK_LOGI(TAG, "Bsp_Ble_Set_Adv_Data len=%u\r\n", (unsigned int)s_adv_len);
    return 0;
}

int Bsp_Ble_Set_Scan_Rsp_Data(unsigned char *scan_rsp_data, unsigned char scan_rsp_len)
{
    if ((scan_rsp_data == NULL) || (scan_rsp_len > sizeof(s_scan_rsp_data)))
    {
        BK_LOGE(TAG, "Bsp_Ble_Set_Scan_Rsp_Data invalid data=%p len=%u max=%u\r\n",
                scan_rsp_data, (unsigned int)scan_rsp_len, (unsigned int)sizeof(s_scan_rsp_data));
        return -1;
    }

    memcpy(s_scan_rsp_data, scan_rsp_data, scan_rsp_len);
    s_scan_rsp_len = scan_rsp_len;
    BK_LOGI(TAG, "Bsp_Ble_Set_Scan_Rsp_Data len=%u\r\n", (unsigned int)s_scan_rsp_len);
    return 0;
}

int Bsp_Ble_Adv_Start(void)
{
    int ret;

    BK_LOGI(TAG, "Bsp_Ble_Adv_Start entry initialized=%u adv_created=%u adv_len=%u scan_rsp_len=%u\r\n",
            (unsigned int)s_initialized,
            (unsigned int)s_adv_created,
            (unsigned int)s_adv_len,
            (unsigned int)s_scan_rsp_len);

    if (!s_adv_created)
    {
        bk_bd_addr_t local_addr = {0};
        bk_get_mac(local_addr, MAC_TYPE_BLUETOOTH);
        for (int i = 0; i < BK_BD_ADDR_LEN / 2; ++i)
        {
            uint8_t tmp = local_addr[i];
            local_addr[i] = local_addr[BK_BD_ADDR_LEN - 1 - i];
            local_addr[BK_BD_ADDR_LEN - 1 - i] = tmp;
        }
        local_addr[5] |= 0xc0;
        local_addr[0]++;

        bk_ble_gap_ext_adv_params_t params = {
            .type = BK_BLE_GAP_SET_EXT_ADV_PROP_LEGACY_IND,
            .interval_min = 120,
            .interval_max = 160,
            .channel_map = BK_ADV_CHNL_ALL,
            .filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
            .primary_phy = BK_BLE_GAP_PRI_PHY_1M,
            .secondary_phy = BK_BLE_GAP_PHY_1M,
            .sid = 0,
            .own_addr_type = BLE_ADDR_TYPE_RANDOM,
        };

        ret = bk_ble_gap_set_adv_params(BLE_ADV_HANDLE, &params);
        BK_LOGI(TAG, "bk_ble_gap_set_adv_params ret=%d\r\n", ret);
        if ((ret != kNoErr) || (Bsp_Ble_Sema_Take() != kNoErr))
        {
            BK_LOGE(TAG, "set_adv_params failed ret=%d\r\n", ret);
            return -1;
        }

        ret = bk_ble_gap_set_adv_rand_addr(BLE_ADV_HANDLE, local_addr);
        BK_LOGI(TAG, "bk_ble_gap_set_adv_rand_addr ret=%d\r\n", ret);
        if ((ret != kNoErr) || (Bsp_Ble_Sema_Take() != kNoErr))
        {
            BK_LOGE(TAG, "set_adv_rand_addr failed ret=%d\r\n", ret);
            return -1;
        }

        s_adv_created = 1;
    }

    if (s_adv_len != 0)
    {
        ret = bk_ble_gap_set_adv_data_raw(BLE_ADV_HANDLE, s_adv_len, s_adv_data);
        BK_LOGI(TAG, "bk_ble_gap_set_adv_data_raw len=%u ret=%d\r\n", (unsigned int)s_adv_len, ret);
        if ((ret != kNoErr) || (Bsp_Ble_Sema_Take() != kNoErr))
        {
            BK_LOGE(TAG, "set_adv_data_raw failed ret=%d\r\n", ret);
            return -1;
        }
    }

    if (s_scan_rsp_len != 0)
    {
        ret = bk_ble_gap_set_scan_rsp_data_raw(BLE_ADV_HANDLE, s_scan_rsp_len, s_scan_rsp_data);
        BK_LOGI(TAG, "bk_ble_gap_set_scan_rsp_data_raw len=%u ret=%d\r\n",
                (unsigned int)s_scan_rsp_len, ret);
        if ((ret != kNoErr) || (Bsp_Ble_Sema_Take() != kNoErr))
        {
            BK_LOGE(TAG, "set_scan_rsp_data_raw failed ret=%d\r\n", ret);
            return -1;
        }
    }

    const bk_ble_gap_ext_adv_t ext_adv = {
        .instance = BLE_ADV_HANDLE,
        .duration = 0,
        .max_events = 0,
    };
    ret = bk_ble_gap_adv_start(1, &ext_adv);
    BK_LOGI(TAG, "bk_ble_gap_adv_start ret=%d\r\n", ret);
    if ((ret != kNoErr) || (Bsp_Ble_Sema_Take() != kNoErr))
    {
        BK_LOGE(TAG, "adv_start failed ret=%d\r\n", ret);
        return -1;
    }

    BK_LOGI(TAG, "Bsp_Ble_Adv_Start done\r\n");
    return 0;
}

int Bsp_Ble_Adv_Stop(void)
{
    const uint8_t inst[] = { BLE_ADV_HANDLE };
    int ret;

    if (!s_adv_created)
    {
        BK_LOGI(TAG, "Bsp_Ble_Adv_Stop skip: adv not created\r\n");
        return 0;
    }

    ret = bk_ble_gap_adv_stop(1, inst);
    BK_LOGI(TAG, "bk_ble_gap_adv_stop ret=%d\r\n", ret);
    if ((ret != kNoErr) || (Bsp_Ble_Sema_Take() != kNoErr))
    {
        BK_LOGE(TAG, "adv_stop failed ret=%d\r\n", ret);
        return -1;
    }

    ret = bk_ble_gap_adv_set_remove(BLE_ADV_HANDLE);
    BK_LOGI(TAG, "bk_ble_gap_adv_set_remove ret=%d\r\n", ret);
    if ((ret != kNoErr) || (Bsp_Ble_Sema_Take() != kNoErr))
    {
        BK_LOGE(TAG, "adv_set_remove failed ret=%d\r\n", ret);
        return -1;
    }

    s_adv_created = 0;
    BK_LOGI(TAG, "Bsp_Ble_Adv_Stop done\r\n");
    return 0;
}

void Bsp_Ble_Get_Local_Addr(unsigned char *mac_addr)
{
    if (mac_addr == NULL)
    {
        return;
    }

    bk_get_mac(mac_addr, MAC_TYPE_BLUETOOTH);
    for (int i = 0; i < BK_BD_ADDR_LEN / 2; ++i)
    {
        uint8_t tmp = mac_addr[i];
        mac_addr[i] = mac_addr[BK_BD_ADDR_LEN - 1 - i];
        mac_addr[BK_BD_ADDR_LEN - 1 - i] = tmp;
    }
    mac_addr[5] |= 0xc0;
    mac_addr[0]++;
}

int Bsp_Ble_Disconnect(unsigned char conn_index)
{
    (void)conn_index;
    if (s_conn_id == 0xFFFF)
    {
        BK_LOGI(TAG, "Bsp_Ble_Disconnect skip: no active connection\r\n");
        return 0;
    }
    int ret = bk_ble_gap_disconnect(s_remote_addr);
    BK_LOGI(TAG, "Bsp_Ble_Disconnect conn_id=%u ret=%d\r\n", (unsigned int)s_conn_id, ret);
    return ret;
}

void Bsp_Ble_Gatts_Disable(void)
{
    BK_LOGI(TAG, "Bsp_Ble_Gatts_Disable\r\n");
    (void)Bsp_Ble_Adv_Stop();
}

int Bsp_Ble_Set_Device_Name(const char *name)
{
    if (name == NULL)
    {
        name = "YLX";
    }
    int ret = bk_ble_gap_set_device_name(name);
    BK_LOGI(TAG, "Bsp_Ble_Set_Device_Name name=%s ret=%d\r\n", name, ret);
    return ret;
}
