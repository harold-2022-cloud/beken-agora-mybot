#pragma once

#include <stddef.h>
#include <stdint.h>

#include <components/bk_voice_service_types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*App_Rtc_Facade_Bridge_Token_Request_Cb)(void *user);

int App_Rtc_Facade_Bridge_Init(void);
int App_Rtc_Facade_Bridge_On_Token_Result(const char *rtc_token,
                                          const char *channel_name,
                                          const char *app_id,
                                          int uid);
int App_Rtc_Facade_Bridge_On_Token_Result_With_User_Account(const char *rtc_token,
                                                            const char *channel_name,
                                                            const char *app_id,
                                                            int uid,
                                                            const char *user_account);
int App_Rtc_Facade_Bridge_Stop(void);
int App_Rtc_Facade_Bridge_Stop_And_Notify(void);
unsigned char App_Rtc_Facade_Bridge_Is_Active(void);
int App_Rtc_Facade_Bridge_Send_Audio(const uint8_t *data,
                                     size_t len,
                                     audio_enc_type_t audio_type);
int App_Rtc_Facade_Bridge_Audio_Read_Callback(unsigned char *data, unsigned int len, void *user);
void App_Rtc_Facade_Bridge_Set_Token_Request_Callback(App_Rtc_Facade_Bridge_Token_Request_Cb cb,
                                                      void *user);

#ifdef __cplusplus
}
#endif
