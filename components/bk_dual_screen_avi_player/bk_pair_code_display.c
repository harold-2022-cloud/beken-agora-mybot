#include "os/os.h"
#include "os/mem.h"
#include "lcd_panel_devices.h"
#include "components/bk_display.h"
#include "gpio_driver.h"
#include <driver/gpio.h>

#include "bk_dual_screen_avi_player.h"

#define TAG "pair_code"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)

#define LCD_BL_IO GPIO_25
#define PAIR_CODE_DISPLAY_WIDTH 160
#define PAIR_CODE_DISPLAY_HEIGHT 160
#define PAIR_CODE_DISPLAY_BYTES_PER_PIXEL 2
#define PAIR_CODE_DISPLAY_SCREEN_COUNT 2
#define PAIR_CODE_DISPLAY_FRAME_SIZE \
    (PAIR_CODE_DISPLAY_WIDTH * PAIR_CODE_DISPLAY_HEIGHT * \
     PAIR_CODE_DISPLAY_BYTES_PER_PIXEL * PAIR_CODE_DISPLAY_SCREEN_COUNT)

#define PAIR_CODE_DIGIT_W 5
#define PAIR_CODE_DIGIT_H 7
#define PAIR_CODE_DIGIT_SCALE 8
#define PAIR_CODE_DIGIT_GAP 8
#define PAIR_CODE_RGB565_BLACK 0x0000
#define PAIR_CODE_RGB565_WHITE 0xffff

extern bk_display_dual_spi_ctlr_config_t dual_spi_ctlr_config;

static bk_display_ctlr_handle_t s_pair_display_handle = NULL;
static frame_buffer_t *s_pair_display_frame = NULL;
static bool s_pair_backlight_opened = false;

static const uint8_t s_digit_5x7[10][PAIR_CODE_DIGIT_H] = {
    {0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e},
    {0x04, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x0e},
    {0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f},
    {0x1e, 0x01, 0x01, 0x0e, 0x01, 0x01, 0x1e},
    {0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02},
    {0x1f, 0x10, 0x10, 0x1e, 0x01, 0x01, 0x1e},
    {0x06, 0x08, 0x10, 0x1e, 0x11, 0x11, 0x0e},
    {0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
    {0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e},
    {0x0e, 0x11, 0x11, 0x0f, 0x01, 0x02, 0x0c},
};

static bk_err_t pair_code_display_frame_cb(void *frame)
{
    (void)frame;
    return BK_OK;
}

static bool pair_code_is_six_digits(const char *code)
{
    if (code == NULL) {
        return false;
    }

    for (int i = 0; i < 6; ++i) {
        if (code[i] < '0' || code[i] > '9') {
            return false;
        }
    }

    return code[6] == '\0';
}

static bk_err_t pair_code_backlight_open(void)
{
    if (s_pair_backlight_opened) {
        return BK_OK;
    }

    gpio_dev_unmap(LCD_BL_IO);
    BK_LOG_ON_ERR(bk_gpio_enable_output(LCD_BL_IO));
    BK_LOG_ON_ERR(bk_gpio_pull_up(LCD_BL_IO));
    bk_gpio_set_output_high(LCD_BL_IO);
    s_pair_backlight_opened = true;

    return BK_OK;
}

static void pair_code_backlight_close(void)
{
    if (!s_pair_backlight_opened) {
        return;
    }

    BK_LOG_ON_ERR(bk_gpio_pull_down(LCD_BL_IO));
    bk_gpio_set_output_low(LCD_BL_IO);
    s_pair_backlight_opened = false;
}

static void pair_code_put_pixel(uint16_t *screen, int x, int y, uint16_t color)
{
    if (x < 0 || y < 0 ||
        x >= PAIR_CODE_DISPLAY_WIDTH ||
        y >= PAIR_CODE_DISPLAY_HEIGHT) {
        return;
    }

    screen[(y * PAIR_CODE_DISPLAY_WIDTH) + x] = color;
}

static void pair_code_fill_rect(uint16_t *screen,
                                int x,
                                int y,
                                int w,
                                int h,
                                uint16_t color)
{
    for (int yy = 0; yy < h; ++yy) {
        for (int xx = 0; xx < w; ++xx) {
            pair_code_put_pixel(screen, x + xx, y + yy, color);
        }
    }
}

static void pair_code_draw_digit(uint16_t *screen,
                                 int x,
                                 int y,
                                 char digit,
                                 uint16_t color)
{
    const uint8_t *rows = s_digit_5x7[digit - '0'];

    for (int row = 0; row < PAIR_CODE_DIGIT_H; ++row) {
        for (int col = 0; col < PAIR_CODE_DIGIT_W; ++col) {
            if ((rows[row] & (1U << (PAIR_CODE_DIGIT_W - 1 - col))) == 0) {
                continue;
            }
            pair_code_fill_rect(screen,
                                x + (col * PAIR_CODE_DIGIT_SCALE),
                                y + (row * PAIR_CODE_DIGIT_SCALE),
                                PAIR_CODE_DIGIT_SCALE,
                                PAIR_CODE_DIGIT_SCALE,
                                color);
        }
    }
}

static void pair_code_draw_three_digits(uint16_t *screen, const char *digits)
{
    const int digit_px_w = PAIR_CODE_DIGIT_W * PAIR_CODE_DIGIT_SCALE;
    const int digit_px_h = PAIR_CODE_DIGIT_H * PAIR_CODE_DIGIT_SCALE;
    const int total_w = (digit_px_w * 3) + (PAIR_CODE_DIGIT_GAP * 2);
    const int start_x = (PAIR_CODE_DISPLAY_WIDTH - total_w) / 2;
    const int start_y = (PAIR_CODE_DISPLAY_HEIGHT - digit_px_h) / 2;

    pair_code_draw_digit(screen, start_x, start_y, digits[0], PAIR_CODE_RGB565_WHITE);
    pair_code_draw_digit(screen,
                         start_x + digit_px_w + PAIR_CODE_DIGIT_GAP,
                         start_y,
                         digits[1],
                         PAIR_CODE_RGB565_WHITE);
    pair_code_draw_digit(screen,
                         start_x + ((digit_px_w + PAIR_CODE_DIGIT_GAP) * 2),
                         start_y,
                         digits[2],
                         PAIR_CODE_RGB565_WHITE);
}

static void pair_code_render_frame(const char *six_digit_code)
{
    uint16_t *left = (uint16_t *)s_pair_display_frame->frame;
    uint16_t *right = (uint16_t *)(s_pair_display_frame->frame +
                                   (PAIR_CODE_DISPLAY_FRAME_SIZE / 2));

    for (int i = 0; i < PAIR_CODE_DISPLAY_WIDTH * PAIR_CODE_DISPLAY_HEIGHT; ++i) {
        left[i] = PAIR_CODE_RGB565_BLACK;
        right[i] = PAIR_CODE_RGB565_BLACK;
    }

    pair_code_draw_three_digits(left, six_digit_code);
    pair_code_draw_three_digits(right, six_digit_code + 3);
}

static bk_err_t pair_code_display_open(void)
{
    bk_err_t ret;

    if (s_pair_display_handle != NULL && s_pair_display_frame != NULL) {
        return BK_OK;
    }

    s_pair_display_frame = os_malloc(sizeof(frame_buffer_t));
    if (s_pair_display_frame == NULL) {
        LOGE("frame struct malloc failed\r\n");
        return BK_FAIL;
    }
    os_memset(s_pair_display_frame, 0, sizeof(frame_buffer_t));

    s_pair_display_frame->frame = psram_malloc(PAIR_CODE_DISPLAY_FRAME_SIZE);
    if (s_pair_display_frame->frame == NULL) {
        LOGE("frame psram malloc failed size=%u\r\n",
             (unsigned)PAIR_CODE_DISPLAY_FRAME_SIZE);
        os_free(s_pair_display_frame);
        s_pair_display_frame = NULL;
        return BK_FAIL;
    }

    s_pair_display_frame->size = PAIR_CODE_DISPLAY_FRAME_SIZE;
    s_pair_display_frame->width = PAIR_CODE_DISPLAY_WIDTH;
    s_pair_display_frame->height = PAIR_CODE_DISPLAY_HEIGHT * PAIR_CODE_DISPLAY_SCREEN_COUNT;
    s_pair_display_frame->fmt = PIXEL_FMT_RGB565;

    ret = bk_display_dual_spi_new(&s_pair_display_handle, &dual_spi_ctlr_config);
    if (ret != BK_OK) {
        LOGE("bk_display_dual_spi_new failed ret=%d\r\n", ret);
        psram_free(s_pair_display_frame->frame);
        os_free(s_pair_display_frame);
        s_pair_display_frame = NULL;
        return ret;
    }

    ret = bk_display_open(s_pair_display_handle);
    if (ret != BK_OK) {
        LOGE("bk_display_open failed ret=%d\r\n", ret);
        bk_display_delete(s_pair_display_handle);
        s_pair_display_handle = NULL;
        psram_free(s_pair_display_frame->frame);
        os_free(s_pair_display_frame);
        s_pair_display_frame = NULL;
        return ret;
    }

    (void)pair_code_backlight_open();
    return BK_OK;
}

bk_err_t bk_pair_code_display_show(const char *six_digit_code)
{
    bk_err_t ret;

    if (!pair_code_is_six_digits(six_digit_code)) {
        LOGW("invalid pair code\r\n");
        return BK_ERR_PARAM;
    }

    (void)bk_dual_screen_avi_player_stop();

    ret = pair_code_display_open();
    if (ret != BK_OK) {
        return ret;
    }

    pair_code_render_frame(six_digit_code);

    ret = bk_display_flush(s_pair_display_handle,
                           s_pair_display_frame,
                           pair_code_display_frame_cb);
    if (ret != BK_OK) {
        LOGE("bk_display_flush failed ret=%d\r\n", ret);
        return ret;
    }

    LOGI("show PAIR_CODE=%s left=%.3s right=%.3s\r\n",
         six_digit_code,
         six_digit_code,
         six_digit_code + 3);
    return BK_OK;
}

bk_err_t bk_pair_code_display_clear(void)
{
    bk_err_t ret = BK_OK;

    if (s_pair_display_handle != NULL) {
        ret = bk_display_close(s_pair_display_handle);
        if (ret != BK_OK) {
            LOGW("bk_display_close failed ret=%d\r\n", ret);
        }

        ret = bk_display_delete(s_pair_display_handle);
        if (ret != BK_OK) {
            LOGW("bk_display_delete failed ret=%d\r\n", ret);
        }
        s_pair_display_handle = NULL;
    }

    if (s_pair_display_frame != NULL) {
        if (s_pair_display_frame->frame != NULL) {
            psram_free(s_pair_display_frame->frame);
            s_pair_display_frame->frame = NULL;
        }
        os_free(s_pair_display_frame);
        s_pair_display_frame = NULL;
    }

    pair_code_backlight_close();
    LOGI("clear\r\n");
    return ret;
}
