#pragma once

#ifndef __BK_DUAL_SCREEN_AVI_PLAYER_H__
#define __BK_DUAL_SCREEN_AVI_PLAYER_H__

#include <common/bk_err.h>

#ifdef __cplusplus
extern "C" {
#endif

bk_err_t bk_dual_screen_avi_player_start(char *file_path);

bk_err_t bk_dual_screen_avi_player_stop(void);

bk_err_t bk_pair_code_display_show(const char *six_digit_code);

bk_err_t bk_pair_code_display_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* __BK_DUAL_SCREEN_AVI_PLAYER_H__ */
