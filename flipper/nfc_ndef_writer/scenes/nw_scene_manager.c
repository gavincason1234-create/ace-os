// NFC NDEF Writer — scene manager table

#include "../nfc_ndef_writer.h"

static const SceneManagerHandlers nw_scene_handlers_impl = {
    .on_enter_handlers = {
        nw_scene_menu_on_enter,
        nw_scene_write_on_enter,
    },
    .on_event_handlers = {
        nw_scene_menu_on_event,
        nw_scene_write_on_event,
    },
    .on_exit_handlers = {
        nw_scene_menu_on_exit,
        nw_scene_write_on_exit,
    },
    .scene_num = NwSceneCount,
};

const SceneManagerHandlers nw_scene_handlers = nw_scene_handlers_impl;
