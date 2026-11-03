#include "../gas_station_audit.h"

static void (*const gsa_on_enter[GsaSceneCount])(void*) = {
    gsa_scene_splash_on_enter,
    gsa_scene_main_menu_on_enter,
    gsa_scene_bt_scan_on_enter,
    gsa_scene_nfc_scan_on_enter,
    gsa_scene_rf_monitor_on_enter,
    gsa_scene_report_on_enter,
    gsa_scene_log_on_enter,
    gsa_scene_price_sign_on_enter,
    gsa_scene_checklist_on_enter,
};
static bool (*const gsa_on_event[GsaSceneCount])(void*, SceneManagerEvent) = {
    gsa_scene_splash_on_event,
    gsa_scene_main_menu_on_event,
    gsa_scene_bt_scan_on_event,
    gsa_scene_nfc_scan_on_event,
    gsa_scene_rf_monitor_on_event,
    gsa_scene_report_on_event,
    gsa_scene_log_on_event,
    gsa_scene_price_sign_on_event,
    gsa_scene_checklist_on_event,
};
static void (*const gsa_on_exit[GsaSceneCount])(void*) = {
    gsa_scene_splash_on_exit,
    gsa_scene_main_menu_on_exit,
    gsa_scene_bt_scan_on_exit,
    gsa_scene_nfc_scan_on_exit,
    gsa_scene_rf_monitor_on_exit,
    gsa_scene_report_on_exit,
    gsa_scene_log_on_exit,
    gsa_scene_price_sign_on_exit,
    gsa_scene_checklist_on_exit,
};

const SceneManagerHandlers gsa_scene_handlers = {
    .on_enter_handlers = gsa_on_enter,
    .on_event_handlers = gsa_on_event,
    .on_exit_handlers  = gsa_on_exit,
    .scene_num         = GsaSceneCount,
};
