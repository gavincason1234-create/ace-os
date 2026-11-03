// BadUSB Launcher — scene manager table

#include "../badusb_launcher.h"

static const SceneManagerHandlers bu_scene_handlers_impl = {
    .on_enter_handlers = {
        bu_scene_menu_on_enter,
        bu_scene_run_on_enter,
    },
    .on_event_handlers = {
        bu_scene_menu_on_event,
        bu_scene_run_on_event,
    },
    .on_exit_handlers = {
        bu_scene_menu_on_exit,
        bu_scene_run_on_exit,
    },
    .scene_num = BuSceneCount,
};

const SceneManagerHandlers bu_scene_handlers = bu_scene_handlers_impl;
