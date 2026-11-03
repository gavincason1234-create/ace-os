#include "../subghz_replay.h"

static void (*const sr_on_enter[])(void*) = {
    sr_scene_menu_on_enter,
    sr_scene_capture_on_enter,
    sr_scene_vault_on_enter,
    sr_scene_waveform_on_enter,
};
static bool (*const sr_on_event[])(void*, SceneManagerEvent) = {
    sr_scene_menu_on_event,
    sr_scene_capture_on_event,
    sr_scene_vault_on_event,
    sr_scene_waveform_on_event,
};
static void (*const sr_on_exit[])(void*) = {
    sr_scene_menu_on_exit,
    sr_scene_capture_on_exit,
    sr_scene_vault_on_exit,
    sr_scene_waveform_on_exit,
};

const SceneManagerHandlers sr_scene_handlers = {
    .on_enter_handlers = sr_on_enter,
    .on_event_handlers = sr_on_event,
    .on_exit_handlers  = sr_on_exit,
    .scene_num         = SrSceneCount,
};
