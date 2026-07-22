#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

typedef struct
{
    char app_id[64];
    char channel[128];
    char token[512];
    char conversation_id[96];
    char user_account[64];
    int uid;
} Device_Api_Conversation_Start_Result_t;

void Device_Api_Client_Init(const char *base_url,
                            const char *device_id,
                            const char *device_token);

int Device_Api_Conversation_Start(Device_Api_Conversation_Start_Result_t *out);
int Device_Api_Conversation_Stop(const char *conversation_id);
int Device_Api_Clear_Device_Token(void);
void Device_Api_Binding_Cancel(void);
void Device_Api_Binding_Clear_Cancel(void);
int Device_Api_Binding_Begin(void);
int Device_Api_Binding_Run(void);
int Device_Api_Get_Pending_Pair_Code(char *pair_code,
                                     size_t pair_code_len,
                                     char *device_id,
                                     size_t device_id_len,
                                     int *expires_in_seconds,
                                     int *poll_after_seconds,
                                     int *has_device_token);

const char *Device_Api_Client_Last_Conversation_Id(void);

#ifdef __cplusplus
}
#endif
