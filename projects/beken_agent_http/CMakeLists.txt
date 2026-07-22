# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{ARMINO_TOOLS_PATH}/build_tools/cmake/project.cmake)

set(ENTITY_SDK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../ai_iot_sdk)
# NOTE: do NOT add ${ENTITY_SDK_DIR} root: armino scans every subdir as a component.
set(EXCLUDE_COMPONENTS agora-iot-sdk)
set(EXTRA_COMPONENTS_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/../../components
    ${ENTITY_SDK_DIR}/entity_iot_sdk
    ${ENTITY_SDK_DIR}/chip_bk7258
    ${ENTITY_SDK_DIR}/platform_os
    ${ENTITY_SDK_DIR}/platform_storage
    ${ENTITY_SDK_DIR}/platform_ota
    ${ENTITY_SDK_DIR}/library/mi_mqtt
    ${ENTITY_SDK_DIR}/chip_bk7258/vendor/ai_iot_bk7258_agora_iot_sdk
    ${ENTITY_SDK_DIR}/media/rtc_facade
)

project(app)
