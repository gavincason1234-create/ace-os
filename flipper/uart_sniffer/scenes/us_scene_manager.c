// UART Debug Sniffer — scene manager table

#include "../uart_sniffer.h"

static const SceneManagerHandlers us_scene_handlers_impl = {
    .on_enter_handlers = {
        us_scene_baud_on_enter,
        us_scene_sniff_on_enter,
    },
    .on_event_handlers = {
        us_scene_baud_on_event,
        us_scene_sniff_on_event,
    },
    .on_exit_handlers = {
        us_scene_baud_on_exit,
        us_scene_sniff_on_exit,
    },
    .scene_num = UsSceneCount,
};

const SceneManagerHandlers us_scene_handlers = us_scene_handlers_impl;
