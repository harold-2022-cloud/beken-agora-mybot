set(incs . include config/bk7258_ap entity_interface entity_port)
set(srcs
    ap_main.c audio_para.c app_agora_session_process.c app_rtc_facade_bridge.c
    entity_iot_process.c device_api_client.c http_wifi_provision_server.c
    entity_interface/entity_interface.c
    entity_interface/entity_product_hooks.c
    entity_interface/entity_system_import_interface.c
    entity_interface/entity_periph_import_interface.c
    entity_interface/entity_mqtt_import_interface.c
    entity_interface/entity_mqtt_export_interface.c
    entity_port/bsp_wifi_bk7258.c
    entity_port/bsp_flash_bk7258.c
)
# app_agora_session_process.c is always compiled; DataStream-specific code inside
# is guarded by #if CONFIG_ENABLE_AGORA_DATASTREAM

if (CONFIG_HTTP_AGENT_ENABLE_BLE_PROVISIONING)
    list(APPEND srcs entity_port/bsp_ble_bk7258.c)
endif()

# bk_solution_ai dependencies
set(priv_req
    media_service multimedia
    bk_voice_service
    audio_engine
    video_engine
    network_transfer
    bk_app_event
    bk_factory_config
    bk_motor
    bk_nfc
    bk_batt_monitor
    bk_key_app
    bk_countdown
    bk_dual_screen_avi_player
    bk_vfs
    bk_uid
)

if (CONFIG_HTTP_AGENT_ENABLE_BLE_PROVISIONING)
    list(APPEND priv_req bk_bluetooth bk_pan)
endif()

# entity SDK dependencies (replaces rino_iot_sdk/rino_interface/factory_test)
list(APPEND priv_req
    chip_bk7258
    entity_iot_sdk
    platform_os
    platform_storage
    platform_ota
    mi_mqtt
    rtc_facade
    json
    lwip_intf_v2_1
)

armino_component_register(SRCS "${srcs}" INCLUDE_DIRS "${incs}" PRIV_REQUIRES "${priv_req}")

set(device_api_region "$ENV{DEVICE_API_REGION}")
if(device_api_region STREQUAL "")
    set(device_api_region "global")
endif()
string(TOLOWER "${device_api_region}" device_api_region)

if(device_api_region STREQUAL "cn")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        DEVICE_API_DEFAULT_BASE_URL=DEVICE_API_BASE_URL_PROD_CN
        DEVICE_API_BUILD_REGION=\"cn\"
    )
elseif(device_api_region STREQUAL "global")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        DEVICE_API_DEFAULT_BASE_URL=DEVICE_API_BASE_URL_PROD_GLOBAL
        DEVICE_API_BUILD_REGION=\"global\"
    )
else()
    message(FATAL_ERROR "Unsupported DEVICE_API_REGION=${device_api_region}; expected cn or global")
endif()

message(STATUS "DEVICE_API_REGION=${device_api_region}")
