#include "../ble_phantom.h"

static void (*const bp_on_enter_handlers[])(void*) = {
    bp_scene_menu_on_enter,
    bp_scene_attack_on_enter,
};

static bool (*const bp_on_event_handlers[])(void*, SceneManagerEvent) = {
    bp_scene_menu_on_event,
    bp_scene_attack_on_event,
};

static void (*const bp_on_exit_handlers[])(void*) = {
    bp_scene_menu_on_exit,
    bp_scene_attack_on_exit,
};

const SceneManagerHandlers bp_scene_handlers = {
    .on_enter_handlers = bp_on_enter_handlers,
    .on_event_handlers = bp_on_event_handlers,
    .on_exit_handlers  = bp_on_exit_handlers,
    .scene_num         = PhantomSceneCount,
};
