// Sub-GHz Spectrum Analyzer — scene manager table

#include "../subghz_spectrum.h"

static const SceneManagerHandlers ss_scene_handlers_impl = {
    .on_enter_handlers = {
        ss_scene_range_on_enter,
        ss_scene_spectrum_on_enter,
    },
    .on_event_handlers = {
        ss_scene_range_on_event,
        ss_scene_spectrum_on_event,
    },
    .on_exit_handlers = {
        ss_scene_range_on_exit,
        ss_scene_spectrum_on_exit,
    },
    .scene_num = SsSceneCount,
};

const SceneManagerHandlers ss_scene_handlers = ss_scene_handlers_impl;
