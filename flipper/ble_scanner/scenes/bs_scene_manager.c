// BLE Scanner — scene manager table

#include "../ble_scanner.h"

static const SceneManagerHandlers bs_scene_handlers_impl = {
    .on_enter_handlers = {
        bs_scene_scan_on_enter,
        bs_scene_detail_on_enter,
        bs_scene_stats_on_enter,
    },
    .on_event_handlers = {
        bs_scene_scan_on_event,
        bs_scene_detail_on_event,
        bs_scene_stats_on_event,
    },
    .on_exit_handlers = {
        bs_scene_scan_on_exit,
        bs_scene_detail_on_exit,
        bs_scene_stats_on_exit,
    },
    .scene_num = BsSceneCount,
};

const SceneManagerHandlers bs_scene_handlers = bs_scene_handlers_impl;
