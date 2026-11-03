#include "../pager_decoder.h"

static void (*const pd_on_enter[])(void*) = {
    pd_scene_menu_on_enter,
    pd_scene_scan_on_enter,
    pd_scene_log_on_enter,
};
static bool (*const pd_on_event[])(void*, SceneManagerEvent) = {
    pd_scene_menu_on_event,
    pd_scene_scan_on_event,
    pd_scene_log_on_event,
};
static void (*const pd_on_exit[])(void*) = {
    pd_scene_menu_on_exit,
    pd_scene_scan_on_exit,
    pd_scene_log_on_exit,
};

const SceneManagerHandlers pd_scene_handlers = {
    .on_enter_handlers = pd_on_enter,
    .on_event_handlers = pd_on_event,
    .on_exit_handlers  = pd_on_exit,
    .scene_num         = PdSceneCount,
};
