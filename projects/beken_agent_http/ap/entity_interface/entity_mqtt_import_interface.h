//entity_mqtt_import_interface.h
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    int result;
    const char *rtc_token;
    const char *channel_name;
    const char *app_id;
    int uid;
} entity_ai_token_result_t;

typedef void (*entity_ai_token_result_cb_t)(const entity_ai_token_result_t *result, void *user);

void Entity_Mqtt_Import_Callback_Init(void);
void Entity_Mqtt_Ai_Token_Result_Callback_Set(entity_ai_token_result_cb_t cb, void *user);

#ifdef __cplusplus
}
#endif
