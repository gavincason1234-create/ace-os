#include "../nfc_inspector.h"

static void (*const ni_on_enter[])(void*) = {
    ni_scene_menu_on_enter,
    ni_scene_read_on_enter,
    ni_scene_result_on_enter,
    ni_scene_sectors_on_enter,
};
static bool (*const ni_on_event[])(void*, SceneManagerEvent) = {
    ni_scene_menu_on_event,
    ni_scene_read_on_event,
    ni_scene_result_on_event,
    ni_scene_sectors_on_event,
};
static void (*const ni_on_exit[])(void*) = {
    ni_scene_menu_on_exit,
    ni_scene_read_on_exit,
    ni_scene_result_on_exit,
    ni_scene_sectors_on_exit,
};

const SceneManagerHandlers ni_scene_handlers = {
    .on_enter_handlers = ni_on_enter,
    .on_event_handlers = ni_on_event,
    .on_exit_handlers  = ni_on_exit,
    .scene_num         = NiSceneCount,
};
