// Scene manager registration — order MUST match SignalStackScene enum

#include "signalstack_scene.h"

static void (*const ss_on_enter[SignalStackSceneCount])(void*) = {
    signalstack_scene_splash_on_enter,
    signalstack_scene_main_menu_on_enter,
    signalstack_scene_scan_on_enter,
    signalstack_scene_result_on_enter,
    signalstack_scene_history_on_enter,
    signalstack_scene_history_detail_on_enter,
    signalstack_scene_settings_on_enter,
    signalstack_scene_about_on_enter,
};

static bool (*const ss_on_event[SignalStackSceneCount])(void*, SceneManagerEvent) = {
    signalstack_scene_splash_on_event,
    signalstack_scene_main_menu_on_event,
    signalstack_scene_scan_on_event,
    signalstack_scene_result_on_event,
    signalstack_scene_history_on_event,
    signalstack_scene_history_detail_on_event,
    signalstack_scene_settings_on_event,
    signalstack_scene_about_on_event,
};

static void (*const ss_on_exit[SignalStackSceneCount])(void*) = {
    signalstack_scene_splash_on_exit,
    signalstack_scene_main_menu_on_exit,
    signalstack_scene_scan_on_exit,
    signalstack_scene_result_on_exit,
    signalstack_scene_history_on_exit,
    signalstack_scene_history_detail_on_exit,
    signalstack_scene_settings_on_exit,
    signalstack_scene_about_on_exit,
};

const SceneManagerHandlers signalstack_scene_handlers = {
    .on_enter_handlers = ss_on_enter,
    .on_event_handlers = ss_on_event,
    .on_exit_handlers  = ss_on_exit,
    .scene_num         = SignalStackSceneCount,
};
