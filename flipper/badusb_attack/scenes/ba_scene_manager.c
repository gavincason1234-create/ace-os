#include "../badusb_attack.h"

static void (*const ba_on_enter[])(void*) = {
    ba_scene_menu_on_enter,
    ba_scene_execute_on_enter,
};
static bool (*const ba_on_event[])(void*, SceneManagerEvent) = {
    ba_scene_menu_on_event,
    ba_scene_execute_on_event,
};
static void (*const ba_on_exit[])(void*) = {
    ba_scene_menu_on_exit,
    ba_scene_execute_on_exit,
};

const SceneManagerHandlers ba_scene_handlers = {
    .on_enter_handlers = ba_on_enter,
    .on_event_handlers = ba_on_event,
    .on_exit_handlers  = ba_on_exit,
    .scene_num         = BaSceneCount,
};
