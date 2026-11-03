// iButton Key Cloner — scene manager table

#include "../ibutton_cloner.h"

static const SceneManagerHandlers ibc_scene_handlers_impl = {
    .on_enter_handlers = {
        ibc_scene_read_on_enter,
        ibc_scene_emulate_on_enter,
    },
    .on_event_handlers = {
        ibc_scene_read_on_event,
        ibc_scene_emulate_on_event,
    },
    .on_exit_handlers = {
        ibc_scene_read_on_exit,
        ibc_scene_emulate_on_exit,
    },
    .scene_num = IbcSceneCount,
};

const SceneManagerHandlers ibc_scene_handlers = ibc_scene_handlers_impl;
