// Sub-GHz Capture — scene manager table

#include "../subghz_capture.h"

static const SceneManagerHandlers sc_scene_handlers_impl = {
    .on_enter_handlers = {
        sc_scene_freq_on_enter,
        sc_scene_capture_on_enter,
        sc_scene_vault_on_enter,
    },
    .on_event_handlers = {
        sc_scene_freq_on_event,
        sc_scene_capture_on_event,
        sc_scene_vault_on_event,
    },
    .on_exit_handlers = {
        sc_scene_freq_on_exit,
        sc_scene_capture_on_exit,
        sc_scene_vault_on_exit,
    },
    .scene_num = ScSceneCount,
};

const SceneManagerHandlers sc_scene_handlers = sc_scene_handlers_impl;
