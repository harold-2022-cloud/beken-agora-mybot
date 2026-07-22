//bsp_flash_bk7258.c
#include "bsp_flash.h"

#include "bsp_system.h"
#include "com_crc.h"
#include "entity_log.h"

#include <easyflash.h>
#include <common/bk_err.h>
#include <driver/flash.h>
#include <driver/flash_partition.h>
#include <os/os.h>
#include <string.h>

#ifndef ERASE_FLASH_TIMEOUT
#define ERASE_FLASH_TIMEOUT          (56)
#endif
#ifndef WRITE_FLASH_TIMEOUT
#define WRITE_FLASH_TIMEOUT          (4)
#endif
#ifndef ERASE_TOUCH_TIMEOUT
#define ERASE_TOUCH_TIMEOUT          (500)
#endif
#ifndef FLASH_SECTOR_SIZE
#define FLASH_SECTOR_SIZE            (0x1000)
#endif
#ifndef OTA_FLASH_BUFFER_LENGTH
#define OTA_FLASH_BUFFER_LENGTH      (1024)
#endif
#ifndef OTA_TEMP_FLASH_BUFFER_LENGTH
#define OTA_TEMP_FLASH_BUFFER_LENGTH (1024 * 5)
#endif

extern int bk_set_env_enhance(const char *key, const void *value, int value_len);
extern int bk_get_env_enhance(const char *key, void *value, int value_len);

typedef struct
{
    uint16_t wr_last_len;
    uint32_t wr_address;
    uint32_t image_size;
    uint32_t received_total_size;
    uint8_t *wr_buf;
    uint8_t *wr_tmp_buf;
    uint8_t *rd_buf;
    bk_logic_partition_t *pt;
    flash_protect_type_t protect_type;
    uint8_t wr_first_flag;
    uint8_t init_flag;
} Bsp_Ota_Info_t;

static Bsp_Ota_Info_t *s_ota_info;

int Bsp_Flash_Save_Key_Value(const char *key, unsigned char *pdata, unsigned int len)
{
    int ret = bk_set_env_enhance(key, pdata, (int)len);
    if (ret != 0)
    {
        ENTITY_LOGE("Bsp_Flash_Save_Key_Value fail key=%s ret=%d\r\n", key, ret);
    }
    return ret;
}

int Bsp_Flash_Read_Key_Value(const char *key, unsigned char *pdata, unsigned int len)
{
    int ret = bk_get_env_enhance(key, pdata, (int)len);
    if (ret <= 0)
    {
        ENTITY_LOGW("Bsp_Flash_Read_Key_Value miss key=%s ret=%d\r\n", key, ret);
    }
    return ret;
}

int Bsp_Flash_Delete_Key(const char *key)
{
    int ret = ef_del_env(key);
    if (ret != 0)
    {
        ENTITY_LOGW("Bsp_Flash_Delete_Key key=%s ret=%d\r\n", key, ret);
    }
    return ret;
}

int Bsp_Flash_Reset_Env_To_Default(void)
{
    int ret = ef_env_set_default();
    ENTITY_LOGW("Bsp_Flash_Reset_Env_To_Default ret=%d\r\n", ret);
    if (ret == 0)
    {
        ret = ef_load_env();
        ENTITY_LOGW("Bsp_Flash_Reset_Env_To_Default reload ret=%d\r\n", ret);
    }
    return ret;
}

static int Bsp_Ota_Alloc_Buf(uint8_t **buf, uint32_t len)
{
    *buf = (uint8_t *)Bsp_Mem_Malloc(len);
    if (*buf == NULL)
    {
        return BK_FAIL;
    }
    memset(*buf, 0, len);
    return BK_OK;
}

int Bsp_Ota_Flash_Init(void)
{
    if (s_ota_info == NULL)
    {
        s_ota_info = (Bsp_Ota_Info_t *)Bsp_Mem_Calloc(1, sizeof(Bsp_Ota_Info_t));
        if (s_ota_info == NULL)
        {
            ENTITY_LOGE("ota info malloc failed\r\n");
            return BK_FAIL;
        }
    }

    if ((Bsp_Ota_Alloc_Buf(&s_ota_info->wr_buf, OTA_FLASH_BUFFER_LENGTH) != BK_OK) ||
        (Bsp_Ota_Alloc_Buf(&s_ota_info->wr_tmp_buf, OTA_TEMP_FLASH_BUFFER_LENGTH) != BK_OK) ||
        (Bsp_Ota_Alloc_Buf(&s_ota_info->rd_buf, OTA_FLASH_BUFFER_LENGTH) != BK_OK))
    {
        Bsp_Ota_Flash_Deinit();
        return BK_FAIL;
    }

    s_ota_info->pt = bk_flash_partition_get_info(BK_PARTITION_OTA);
    if (s_ota_info->pt == NULL)
    {
        ENTITY_LOGE("BK_PARTITION_OTA not found\r\n");
        Bsp_Ota_Flash_Deinit();
        return BK_FAIL;
    }

    s_ota_info->received_total_size = 0;
    s_ota_info->wr_last_len = 0;
    s_ota_info->wr_first_flag = 0;
    s_ota_info->wr_address = s_ota_info->pt->partition_start_addr;
    s_ota_info->protect_type = bk_flash_get_protect_type();
    bk_flash_set_protect_type(FLASH_PROTECT_NONE);
    s_ota_info->init_flag = 1;
    ENTITY_LOGI("ota write to 0x%x\r\n", s_ota_info->wr_address);
    return BK_OK;
}

static int Bsp_Ota_Flash_Write_Flash(uint16_t len)
{
    uint32_t anchor_time;
    uint32_t temp_time;
    uint8_t flash_ready;

    if (s_ota_info == NULL)
    {
        return BK_FAIL;
    }

    if ((s_ota_info->wr_address % FLASH_SECTOR_SIZE) == 0)
    {
        anchor_time = rtos_get_time();
        while (1)
        {
            flash_ready = ble_callback_deal_handler(ERASE_FLASH_TIMEOUT);
            temp_time = rtos_get_time();
            temp_time = (temp_time >= anchor_time) ? (temp_time - anchor_time)
                                                   : (temp_time + (0xFFFFFFFFu - anchor_time));
            if (temp_time >= ERASE_TOUCH_TIMEOUT)
            {
                flash_ready = 1;
            }

            if (flash_ready)
            {
                if ((len != 0) &&
                    ((s_ota_info->wr_address + len) <=
                     (s_ota_info->pt->partition_start_addr + s_ota_info->pt->partition_length)))
                {
                    bk_flash_erase_sector(s_ota_info->wr_address);
                }
                break;
            }
            rtos_delay_milliseconds(2);
        }
    }

    if ((s_ota_info->wr_address < s_ota_info->pt->partition_start_addr) ||
        ((s_ota_info->wr_address + len) >
         (s_ota_info->pt->partition_start_addr + s_ota_info->pt->partition_length)))
    {
        return BK_FAIL;
    }

    anchor_time = rtos_get_time();
    while (1)
    {
        flash_ready = ble_callback_deal_handler(WRITE_FLASH_TIMEOUT);
        temp_time = rtos_get_time();
        temp_time = (temp_time >= anchor_time) ? (temp_time - anchor_time)
                                               : (temp_time + (0xFFFFFFFFu - anchor_time));
        if (temp_time >= ERASE_TOUCH_TIMEOUT)
        {
            flash_ready = 1;
        }

        if (flash_ready)
        {
            bk_flash_write_bytes(s_ota_info->wr_address, s_ota_info->wr_buf, len);
            bk_flash_read_bytes(s_ota_info->wr_address, s_ota_info->rd_buf, len);
            if (memcmp(s_ota_info->wr_buf, s_ota_info->rd_buf, len) != 0)
            {
                ENTITY_LOGE("ota flash verify failed\r\n");
                return BK_FAIL;
            }

            s_ota_info->wr_address += len;
            memset(s_ota_info->wr_buf, 0, len);
            return BK_OK;
        }
        rtos_delay_milliseconds(2);
    }
}

int Bsp_Ota_Flash_Process_Data(unsigned char *buf, uint16_t len, uint32_t total)
{
    uint32_t write_len;
    uint32_t offset = 0;
    int ret = BK_FAIL;

    if ((s_ota_info == NULL) || (s_ota_info->wr_tmp_buf == NULL) || (s_ota_info->wr_buf == NULL))
    {
        return BK_FAIL;
    }
    if ((buf == NULL) || (len > OTA_TEMP_FLASH_BUFFER_LENGTH))
    {
        return BK_FAIL;
    }

#if (CONFIG_TASK_WDT)
    extern void bk_task_wdt_feed(void);
    bk_task_wdt_feed();
#endif

    memcpy(s_ota_info->wr_tmp_buf, buf, len);
    if (s_ota_info->wr_first_flag == 0)
    {
        s_ota_info->image_size = total;
        s_ota_info->wr_first_flag = 1;
    }

    while (offset < len)
    {
        write_len = len - offset;
        if (write_len > (OTA_FLASH_BUFFER_LENGTH - s_ota_info->wr_last_len))
        {
            write_len = OTA_FLASH_BUFFER_LENGTH - s_ota_info->wr_last_len;
        }

        memcpy(s_ota_info->wr_buf + s_ota_info->wr_last_len,
               s_ota_info->wr_tmp_buf + offset,
               write_len);

        offset += write_len;
        s_ota_info->wr_last_len += write_len;
        s_ota_info->received_total_size += write_len;

        if (s_ota_info->received_total_size == s_ota_info->image_size)
        {
            ret = Bsp_Ota_Flash_Write_Flash(s_ota_info->wr_last_len);
            s_ota_info->wr_last_len = 0;
        }
        else if (s_ota_info->wr_last_len >= OTA_FLASH_BUFFER_LENGTH)
        {
            ret = Bsp_Ota_Flash_Write_Flash(OTA_FLASH_BUFFER_LENGTH);
            if (ret == BK_OK)
            {
                s_ota_info->wr_last_len = 0;
            }
        }
        else
        {
            ret = BK_OK;
        }

        if (ret != BK_OK)
        {
            return ret;
        }
    }

    return ret;
}

int Bsp_Ota_Flash_Check_Crc(uint32_t in_crc)
{
    uint32_t start_addr;
    uint32_t remain;
    uint32_t offset = 0;
    uint32_t crc = 0xFFFFFFFFu;

    if ((s_ota_info == NULL) || (s_ota_info->rd_buf == NULL) || (s_ota_info->pt == NULL))
    {
        return BK_FAIL;
    }

    start_addr = s_ota_info->pt->partition_start_addr;
    remain = s_ota_info->image_size;
    while (remain > 0)
    {
        uint32_t chunk = (remain > OTA_FLASH_BUFFER_LENGTH) ? OTA_FLASH_BUFFER_LENGTH : remain;
        bk_flash_read_bytes(start_addr + offset, s_ota_info->rd_buf, chunk);
        crc = Com_Section_Crc32(crc, s_ota_info->rd_buf, chunk, 0);
        offset += chunk;
        remain -= chunk;
    }

    if (crc != in_crc)
    {
        ENTITY_LOGE("ota crc mismatch calc=0x%x expected=0x%x\r\n", crc, in_crc);
        return BK_FAIL;
    }

    ENTITY_LOGI("ota crc ok 0x%x\r\n", crc);
    return BK_OK;
}

int Bsp_Ota_Flash_Deinit(void)
{
    if (s_ota_info == NULL)
    {
        return BK_OK;
    }

    if (s_ota_info->wr_buf != NULL)
    {
        Bsp_Mem_Free(s_ota_info->wr_buf);
        s_ota_info->wr_buf = NULL;
    }
    if (s_ota_info->wr_tmp_buf != NULL)
    {
        Bsp_Mem_Free(s_ota_info->wr_tmp_buf);
        s_ota_info->wr_tmp_buf = NULL;
    }
    if (s_ota_info->rd_buf != NULL)
    {
        Bsp_Mem_Free(s_ota_info->rd_buf);
        s_ota_info->rd_buf = NULL;
    }
    if (s_ota_info->init_flag)
    {
        bk_flash_set_protect_type(s_ota_info->protect_type);
    }
    s_ota_info->init_flag = 0;
    return BK_OK;
}

int Bsp_Ota_Flash_Complete(void)
{
    return BK_OK;
}
