#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SDK = ROOT.parents[1] / "ai_iot_sdk"
PROCESS = ROOT / "ap" / "entity_iot_process.c"
DEVICE_API = ROOT / "ap" / "device_api_client.c"
RTC_BRIDGE = ROOT / "ap" / "app_rtc_facade_bridge.c"
BSP_FLASH_H = ROOT / "ap" / "entity_port" / "bsp_flash.h"
BSP_FLASH_C = ROOT / "ap" / "entity_port" / "bsp_flash_bk7258.c"
AUDIO_PARA = ROOT / "ap" / "audio_para.c"
AP_CMAKE = ROOT / "ap" / "CMakeLists.txt"
AP_KCONFIG = ROOT / "ap" / "Kconfig.projbuild"
ENTITY_INTERFACE = ROOT / "ap" / "entity_interface" / "entity_interface.c"
PERIPH_INTERFACE = ROOT / "ap" / "entity_interface" / "entity_periph_import_interface.c"
BSP_WIFI = ROOT / "ap" / "entity_port" / "bsp_wifi_bk7258.c"
HTTP_WIFI_PROV_C = ROOT / "ap" / "http_wifi_provision_server.c"
HTTP_WIFI_PROV_H = ROOT / "ap" / "http_wifi_provision_server.h"
PAIR_CODE_DISPLAY_C = ROOT.parents[1] / "components" / "bk_dual_screen_avi_player" / "bk_pair_code_display.c"
DUAL_SCREEN_H = ROOT.parents[1] / "components" / "bk_dual_screen_avi_player" / "bk_dual_screen_avi_player.h"
DUAL_SCREEN_CMAKE = ROOT.parents[1] / "components" / "bk_dual_screen_avi_player" / "CMakeLists.txt"
EASYFLASH_CFG = ROOT.parents[1] / "bk_avdk_smp" / "ap" / "components" / "easy_flash" / "easy_flash_V4.X" / "inc" / "ef_cfg.h"
DHCP_SERVER_AP = ROOT.parents[1] / "bk_avdk_smp" / "ap" / "components" / "lwip_intf_v2_1" / "dhcpd" / "dhcp-server.c"
DHCP_SERVER_CP = ROOT.parents[1] / "bk_avdk_smp" / "cp" / "components" / "lwip_intf_v2_1" / "dhcpd" / "dhcp-server.c"
LWIP_KCONFIG_AP = ROOT.parents[1] / "bk_avdk_smp" / "ap" / "components" / "lwip_intf_v2_1" / "Kconfig"
LWIP_KCONFIG_CP = ROOT.parents[1] / "bk_avdk_smp" / "cp" / "components" / "lwip_intf_v2_1" / "Kconfig"
PRODUCT_AP_CONFIG = ROOT / "ap" / "config" / "bk7258_ap" / "config"
PRODUCT_CP_CONFIG = ROOT / "cp" / "config" / "bk7258" / "config"
PATCH_DIR = ROOT.parents[1] / "patches" / "beken"

process_text = PROCESS.read_text(encoding="utf-8")
device_api_text = DEVICE_API.read_text(encoding="utf-8")
rtc_bridge_text = RTC_BRIDGE.read_text(encoding="utf-8")
bsp_flash_h_text = BSP_FLASH_H.read_text(encoding="utf-8")
bsp_flash_c_text = BSP_FLASH_C.read_text(encoding="utf-8")
audio_para_text = AUDIO_PARA.read_text(encoding="utf-8")
ap_cmake_text = AP_CMAKE.read_text(encoding="utf-8")
ap_kconfig_text = AP_KCONFIG.read_text(encoding="utf-8")
entity_interface_text = ENTITY_INTERFACE.read_text(encoding="utf-8")
periph_interface_text = PERIPH_INTERFACE.read_text(encoding="utf-8")
bsp_wifi_text = BSP_WIFI.read_text(encoding="utf-8")
http_wifi_prov_c_text = HTTP_WIFI_PROV_C.read_text(encoding="utf-8") if HTTP_WIFI_PROV_C.exists() else ""
http_wifi_prov_h_text = HTTP_WIFI_PROV_H.read_text(encoding="utf-8") if HTTP_WIFI_PROV_H.exists() else ""
pair_code_display_text = PAIR_CODE_DISPLAY_C.read_text(encoding="utf-8") if PAIR_CODE_DISPLAY_C.exists() else ""
dual_screen_h_text = DUAL_SCREEN_H.read_text(encoding="utf-8") if DUAL_SCREEN_H.exists() else ""
dual_screen_cmake_text = DUAL_SCREEN_CMAKE.read_text(encoding="utf-8") if DUAL_SCREEN_CMAKE.exists() else ""
easyflash_cfg_text = EASYFLASH_CFG.read_text(encoding="utf-8")
dhcp_server_ap_text = DHCP_SERVER_AP.read_text(encoding="utf-8")
dhcp_server_cp_text = DHCP_SERVER_CP.read_text(encoding="utf-8")
lwip_kconfig_ap_text = LWIP_KCONFIG_AP.read_text(encoding="utf-8")
lwip_kconfig_cp_text = LWIP_KCONFIG_CP.read_text(encoding="utf-8")
product_ap_config_text = PRODUCT_AP_CONFIG.read_text(encoding="utf-8")
product_cp_config_text = PRODUCT_CP_CONFIG.read_text(encoding="utf-8")
patches_text = "\n".join(
    patch.read_text(encoding="utf-8") for patch in sorted(PATCH_DIR.glob("*.patch"))
) if PATCH_DIR.exists() else ""
easyflash_cfg_text += "\n" + patches_text
dhcp_server_text = dhcp_server_ap_text + "\n" + dhcp_server_cp_text + "\n" + patches_text
lwip_kconfig_ap_text += "\n" + patches_text
lwip_kconfig_cp_text += "\n" + patches_text

try:
    start_case = process_text.split("case ENTITY_PRODUCT_WORK_AI_START:", 1)[1].split("break;", 1)[0]
except IndexError as exc:
    raise SystemExit("missing ENTITY_PRODUCT_WORK_AI_START case") from exc

required = {
    "Product_Http_Ai_Start()": process_text,
    "Device_Api_Conversation_Start": process_text + "\n" + device_api_text,
    "App_Rtc_Facade_Bridge_On_Token_Result": process_text,
    "/devices/%s/conversations/start": device_api_text,
    "/devices/pair-codes": device_api_text,
    "/devices/%s/binding-status": device_api_text,
    "PAIR_CODE=": device_api_text,
    "binding pending PAIR_CODE=": device_api_text,
    "Device_Api_Notify_Pair_Code_Ble": device_api_text,
    "bk_pair_code_display_show(out->code)": device_api_text,
    "bk_pair_code_display_clear()": device_api_text,
    "bk_pair_code_display.c": dual_screen_cmake_text,
    "bk_pair_code_display_show": pair_code_display_text + "\n" + dual_screen_h_text,
    "bk_pair_code_display_clear": pair_code_display_text + "\n" + dual_screen_h_text,
    "static const uint8_t s_digit_5x7": pair_code_display_text,
    "PAIR_CODE_DISPLAY_WIDTH": pair_code_display_text,
    "PAIR_CODE_DISPLAY_HEIGHT": pair_code_display_text,
    "#define PAIR_CODE_DIGIT_SCALE 8": pair_code_display_text,
    "bk_dual_screen_avi_player_stop()": pair_code_display_text,
    "bk_display_flush": pair_code_display_text,
    "Entity_Ble_V1_Send_Packet_By_Notify": device_api_text,
    "\"pair_code\"": device_api_text,
    "\"expires_in\"": device_api_text,
    "\"device_id\"": device_api_text,
    "request method=": device_api_text,
    "write done bytes=": device_api_text,
    "read response bytes=": device_api_text,
    "read response no data total=": device_api_text,
    "\"code\"": device_api_text,
    "\"pair_token\"": device_api_text,
    "\"device_token\"": device_api_text,
    "DEVICE_API_DEVICE_TOKEN_KEY": device_api_text,
    "Bsp_Flash_Read_Key_Value": device_api_text,
    "Bsp_Flash_Save_Key_Value": device_api_text,
    "Entity_Network_t": device_api_text,
    "%s %s HTTP/1.1": device_api_text,
    "ENTITY_PRODUCT_WORK_HTTP_BIND": process_text,
    "Device_Api_Binding_Run": process_text + "\n" + device_api_text,
    "Device_Api_Binding_Begin": process_text + "\n" + device_api_text,
    "Device_Api_Binding_Clear_Cancel": process_text + "\n" + device_api_text + "\n" + http_wifi_prov_h_text,
    "Device_Api_Clear_Device_Token": process_text + "\n" + device_api_text,
    "Device_Api_Binding_Cancel": process_text + "\n" + device_api_text,
    "s_binding_cancel_requested": device_api_text,
    "binding cancelled": device_api_text,
    "Product_Http_Binding_Reset": process_text,
    "Product_Http_Binding_Request_Cancel": process_text,
    "s_config_net_pending_need_clear": process_text,
    "Bsp_Flash_Delete_Key": bsp_flash_h_text + "\n" + bsp_flash_c_text + "\n" + device_api_text,
    "ef_del_env": bsp_flash_c_text,
    "s_pending_pair": device_api_text,
    "cJSON_Parse": device_api_text,
    "Bsp_Wifi_Get_Macaddr": device_api_text,
    "AG-%02X%02X%02X%02X%02X%02X": device_api_text,
    "\"conversation_id\"": device_api_text,
    "\"rtc\"": device_api_text,
    "\"app_id\"": device_api_text,
    "\"channel\"": device_api_text,
    "\"uid\"": device_api_text,
    "cJSON_IsString(uid)": device_api_text,
    "cJSON_IsNumber(uid)": device_api_text,
    "user_account": device_api_text + "\n" + process_text,
    "App_Rtc_Facade_Bridge_On_Token_Result_With_User_Account": process_text,
    "\"token\"": device_api_text,
    "\\\"features\\\":{\\\"ai_qos\\\":false,": device_api_text,
    "result.enable_audio_ai_qos = false": rtc_bridge_text,
    "speaker_chan0_digital_gain = 0x30": audio_para_text,
    "http_wifi_provision_server.c": ap_cmake_text,
    "Http_Wifi_Provision_Server_Start": process_text + "\n" + http_wifi_prov_h_text + "\n" + http_wifi_prov_c_text,
    "Http_Wifi_Provision_Server_Stop": process_text + "\n" + http_wifi_prov_h_text + "\n" + http_wifi_prov_c_text,
    "GET /scan": http_wifi_prov_c_text,
    "POST /submit": http_wifi_prov_c_text,
    "GET /pair-code": http_wifi_prov_c_text,
    "POST /clear-env": http_wifi_prov_c_text,
    "Http_Wifi_Provision_Server_On_Sta_Connected": process_text + "\n" + http_wifi_prov_h_text + "\n" + http_wifi_prov_c_text,
    "phase=wifi_connecting": http_wifi_prov_c_text,
    "\\\"phase\\\":\\\"wifi_connecting\\\"": http_wifi_prov_c_text,
    "Bsp_Flash_Reset_Env_To_Default": bsp_flash_h_text + "\n" + bsp_flash_c_text + "\n" + http_wifi_prov_c_text,
    "ef_env_set_default": bsp_flash_c_text,
    "ENV_AREA_SIZE     (4 * EF_ERASE_MIN_SIZE)": easyflash_cfg_text,
    "[HTTP_WIFI_PROV][EF_ENV]": http_wifi_prov_c_text,
    "ENV_AREA_SIZE": http_wifi_prov_c_text,
    "sizeof(Entity_Dev_Config_Net_Info_t)": http_wifi_prov_c_text,
    "persist_ret": http_wifi_prov_c_text,
    "sta_config_ret": http_wifi_prov_c_text,
    "sta_connect_ret": http_wifi_prov_c_text,
    "pairCode": http_wifi_prov_c_text,
    "font-size:52px": http_wifi_prov_c_text,
    "font-weight:800": http_wifi_prov_c_text,
    "setInterval(pollPair,1500)": http_wifi_prov_c_text,
    "document.getElementById('setup').style.display='none'": http_wifi_prov_c_text,
    "Bsp_Wifi_Ap_Mode_Config": http_wifi_prov_c_text,
    "Bsp_Wifi_Ap_Start": http_wifi_prov_c_text,
    "Bsp_Wifi_Ap_Stop": http_wifi_prov_c_text,
    "[CAPTIVE_DNS] using Beken built-in DHCP DNS handler": http_wifi_prov_c_text,
    "[CAPTIVE_DNS_BUILTIN]": dhcp_server_text,
    "answer_rrs = htons(0x01)": dhcp_server_text,
    "dhcps.router_ip": dhcp_server_text,
    "CONFIG_BK_DHCPD_CAPTIVE_DNS": dhcp_server_text,
    "config BK_DHCPD_CAPTIVE_DNS": lwip_kconfig_ap_text + "\n" + lwip_kconfig_cp_text,
    "CONFIG_BK_DHCPD_CAPTIVE_DNS=y": product_ap_config_text + "\n" + product_cp_config_text,
    "[HTTP_WIFI_PROV] pause standby countdown": http_wifi_prov_c_text,
    "[HTTP_WIFI_PROV] resume standby countdown": http_wifi_prov_c_text,
    "HTTP/1.1 302 Found": http_wifi_prov_c_text,
    "Location: http://192.168.4.1/": http_wifi_prov_c_text,
    "captive redirect": http_wifi_prov_c_text,
    "Bsp_Wifi_Sta_Scan_Start": http_wifi_prov_c_text,
    "Bsp_Wifi_Copy_Scan_Results": http_wifi_prov_c_text,
    "Device_Api_Binding_Begin": http_wifi_prov_c_text + "\n" + process_text + "\n" + device_api_text,
    "HTTP_AGENT_ENABLE_MQTT": ap_kconfig_text + "\n" + entity_interface_text + "\n" + bsp_wifi_text + "\n" + process_text,
    "HTTP_AGENT_ENABLE_BLE_PROVISIONING": ap_kconfig_text + "\n" + periph_interface_text + "\n" + device_api_text,
    "[HTTP_AGENT][MQTT_DISABLED]": bsp_wifi_text,
    "[HTTP_AGENT][BLE_DISABLED]": periph_interface_text + "\n" + device_api_text,
}

for needle, haystack in required.items():
    if needle not in haystack:
        raise SystemExit(f"missing required HTTP AI boundary marker: {needle}")

for forbidden in [
    "Entity_Device_Access_Export_Interface();",
    "Entity_Mqtt_App_Is_Connected()",
    "Entity_Mqtt_App_Prepare_Ai_Publish()",
    "Mqtt_Event_Agora_Agent_Device_Access_Report",
]:
    if forbidden in start_case:
        raise SystemExit(f"AI start path still depends on MQTT: {forbidden}")

for forbidden in [
    "HTTP transport not wired yet",
]:
    if forbidden in device_api_text:
        raise SystemExit(f"HTTP device API is still a shell: {forbidden}")

if "lvgl" in pair_code_display_text.lower():
    raise SystemExit("pair-code display must not enable or include LVGL")

if "pair_token=%s" in device_api_text or "pair_token=%.*s" in device_api_text:
    raise SystemExit("pair_token must not be printed in logs")

if "default n" not in ap_kconfig_text.split("config HTTP_AGENT_ENABLE_MQTT", 1)[1].split("config", 1)[0]:
    raise SystemExit("HTTP-only product must default MQTT runtime off")

if "default n" not in ap_kconfig_text.split("config HTTP_AGENT_ENABLE_BLE_PROVISIONING", 1)[1].split("config", 1)[0]:
    raise SystemExit("HTTP-only product must default BLE provisioning runtime off")

if "#if CONFIG_HTTP_AGENT_ENABLE_MQTT" not in entity_interface_text:
    raise SystemExit("Entity_Mqtt_Import_Callback_Init must be gated by CONFIG_HTTP_AGENT_ENABLE_MQTT")

if "#if CONFIG_HTTP_AGENT_ENABLE_BLE_PROVISIONING" not in periph_interface_text:
    raise SystemExit("Entity_Ble_Import_Interface_Init must be gated by CONFIG_HTTP_AGENT_ENABLE_BLE_PROVISIONING")

if "#if CONFIG_HTTP_AGENT_ENABLE_MQTT" not in bsp_wifi_text:
    raise SystemExit("Beken Wi-Fi event bridge must avoid SDK MQTT startup when HTTP_AGENT_ENABLE_MQTT is off")

if "Entity_Set_Dev_Status(DEV_WIFI_CONNECTED_STATE)" not in bsp_wifi_text:
    raise SystemExit("HTTP-only Wi-Fi GOT_IP path must still notify product DEV_WIFI_CONNECTED_STATE")

if "if (Product_Http_Ai_Start() == 0)" not in start_case:
    raise SystemExit("AI start must only launch AVI/player after HTTP RTC start succeeds")

try:
    send_func = process_text.split("static int Entity_Product_Work_Send", 1)[1].split("static void Entity_Product_Worker_Task", 1)[0]
except IndexError as exc:
    raise SystemExit("missing Entity_Product_Work_Send") from exc

if "Product_Http_Binding_Request_Cancel()" not in send_func:
    raise SystemExit("CONFIG_NETWORK enqueue path must cancel long HTTP binding polling first")
if "s_config_net_pending_need_clear" not in send_func:
    raise SystemExit("CONFIG_NETWORK enqueue path must preserve pending config-net when queue is full")

config_network_cases = process_text.split("case CONFIG_NETWORK:")
if len(config_network_cases) < 3:
    raise SystemExit("expected key and external CONFIG_NETWORK handlers")
for idx, case_tail in enumerate(config_network_cases[1:], start=1):
    case_body = case_tail.split("break;", 1)[0]
    prompt_pos = case_body.find("app_event_send_msg(APP_EVT_NETWORK_PROVISIONING")
    enqueue_pos = case_body.find("Entity_Product_Work_Send(ENTITY_PRODUCT_WORK_CONFIG_NET")
    if prompt_pos < 0 or enqueue_pos < 0 or prompt_pos > enqueue_pos:
        raise SystemExit(f"CONFIG_NETWORK handler {idx} must play provisioning prompt before entering AP web provisioning")

try:
    binding_start_func = process_text.split("static int Product_Http_Binding_Start(void)\n{", 1)[1].split("static void Product_Config_Net_Run", 1)[0]
except IndexError as exc:
    raise SystemExit("missing Product_Http_Binding_Start") from exc

clear_cancel_pos = binding_start_func.find("Device_Api_Binding_Clear_Cancel()")
run_pos = binding_start_func.find("Device_Api_Binding_Run()")
if clear_cancel_pos < 0 or run_pos < 0 or clear_cancel_pos > run_pos:
    raise SystemExit("new HTTP binding lifecycle must clear stale cancel before Device_Api_Binding_Run")

try:
    config_net_case = process_text.split("case ENTITY_PRODUCT_WORK_CONFIG_NET:", 1)[1].split("break;", 1)[0]
except IndexError as exc:
    raise SystemExit("missing ENTITY_PRODUCT_WORK_CONFIG_NET case") from exc

try:
    config_net_func = process_text.split("static void Product_Config_Net_Run(uint32_t need_clear)\n{", 1)[1].split("static void Product_Http_Binding_Request_Cancel", 1)[0]
except IndexError as exc:
    raise SystemExit("missing Product_Config_Net_Run") from exc

if "Product_Config_Net_Run(msg.value)" not in config_net_case:
    raise SystemExit("ENTITY_PRODUCT_WORK_CONFIG_NET must run through Product_Config_Net_Run")

clear_pos = config_net_func.find("Device_Api_Clear_Device_Token()")
reset_pos = config_net_func.find("Product_Http_Binding_Reset()")
prov_pos = config_net_func.find("Http_Wifi_Provision_Server_Start")
legacy_pos = config_net_func.find("Entity_Manual_Config_Net_Export_Interface")
if clear_pos < 0 or prov_pos < 0 or clear_pos > prov_pos:
    raise SystemExit("need_clear=1 config-net path must clear http_device_token before AP web provisioning")
if reset_pos < 0 or prov_pos < 0 or reset_pos > prov_pos:
    raise SystemExit("need_clear=1 config-net path must reset product HTTP binding state before AP web provisioning")
if legacy_pos >= 0:
    raise SystemExit("AP web provisioning path must not enter BLE provisioning")

if "case DEV_BLE_PROVISION_WIFI_CONNECTED_STATE:" in process_text:
    raise SystemExit("product AP web provisioning must not depend on BLE provisioning SDK event")

for forbidden in [
    "HTTP_WIFI_PROV_DNS_PORT",
    "http_wifi_prov_dns_thread",
    "http_wifi_prov_dns_build_response",
    "bind(s_http_wifi_prov_dns_fd",
]:
    if forbidden in http_wifi_prov_c_text:
        raise SystemExit(f"product AP provisioning must use Beken built-in DNS handler, not product UDP 53: {forbidden}")

for label, text in [
    ("ap", dhcp_server_ap_text + "\n" + patches_text),
    ("cp", dhcp_server_cp_text + "\n" + patches_text),
]:
    for needle in [
        "[CAPTIVE_DNS_BUILTIN]",
        "answer_rrs = htons(0x01)",
        "dhcps.router_ip",
    ]:
        if needle not in text:
            raise SystemExit(f"{label} DHCP DNS handler missing captive DNS marker: {needle}")
    if "#if CONFIG_BK_DHCPD_CAPTIVE_DNS" not in text:
        raise SystemExit(f"{label} DHCP DNS captive behavior must be guarded by CONFIG_BK_DHCPD_CAPTIVE_DNS")
    if "#else" not in text or "ERROR_REFUSED" not in text:
        raise SystemExit(f"{label} DHCP DNS handler must preserve default non-captive REFUSED behavior")

for label, text in [
    ("ap", lwip_kconfig_ap_text),
    ("cp", lwip_kconfig_cp_text),
]:
    try:
        block = text.split("config BK_DHCPD_CAPTIVE_DNS", 1)[1].split("\n\tconfig ", 1)[0]
    except IndexError as exc:
        raise SystemExit(f"{label} Kconfig missing BK_DHCPD_CAPTIVE_DNS") from exc
    if "default n" not in block:
        raise SystemExit(f"{label} BK_DHCPD_CAPTIVE_DNS must default off")

if "CONFIG_BK_DHCPD_CAPTIVE_DNS=y" not in product_ap_config_text:
    raise SystemExit("beken_agent_http AP config must enable CONFIG_BK_DHCPD_CAPTIVE_DNS")
if "CONFIG_BK_DHCPD_CAPTIVE_DNS=y" not in product_cp_config_text:
    raise SystemExit("beken_agent_http CP config must enable CONFIG_BK_DHCPD_CAPTIVE_DNS")

try:
    submit_func = http_wifi_prov_c_text.split("static int http_wifi_prov_handle_submit", 1)[1].split("static int http_wifi_prov_handle_pair_code", 1)[0]
except IndexError as exc:
    raise SystemExit("missing http_wifi_prov_handle_submit") from exc

mode_pos = submit_func.find("Bsp_Wifi_Sta_Mode_Config")
connect_pos = submit_func.find("Bsp_Wifi_Sta_Conncet")
if mode_pos < 0 or connect_pos < 0:
    raise SystemExit("/submit must attempt STA mode config and STA connect")
if "if (persist_ret == 0)" in submit_func[:mode_pos]:
    raise SystemExit("/submit must not gate STA mode config on flash persist success")

if "scan();pairTimer=setInterval(pollPair,1500)" in http_wifi_prov_c_text:
    raise SystemExit("/pair-code polling must not start before Wi-Fi submit")

try:
    pair_code_func = http_wifi_prov_c_text.split("static int http_wifi_prov_handle_pair_code", 1)[1].split("static int http_wifi_prov_handle_client", 1)[0]
except IndexError as exc:
    raise SystemExit("missing http_wifi_prov_handle_pair_code") from exc

wait_pos = pair_code_func.find("phase=wifi_connecting")
getter_pos = pair_code_func.find("Device_Api_Get_Pending_Pair_Code")
if wait_pos < 0 or getter_pos < 0 or wait_pos > getter_pos:
    raise SystemExit("/pair-code must return wifi_connecting before reading pending pair code")

try:
    unprovision_case = process_text.split("case DEV_UNPROVISION_STATE:", 1)[1].split("break;", 1)[0]
except IndexError as exc:
    raise SystemExit("missing DEV_UNPROVISION_STATE case") from exc

if "Entity_Product_Work_Send(ENTITY_PRODUCT_WORK_CONFIG_NET, 0)" not in unprovision_case:
    raise SystemExit("DEV_UNPROVISION_STATE must start AP web provisioning without clearing http_device_token")

print("PASS: beken_agent_http AI start uses HTTP + AP web provisioning boundary")
