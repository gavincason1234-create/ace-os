// IR Remote Cloner — scene manager table

#include "../ir_cloner.h"

static const SceneManagerHandlers irc_scene_handlers_impl = {
    .on_enter_handlers = {
        irc_scene_learn_on_enter,
        irc_scene_vault_on_enter,
    },
    .on_event_handlers = {
        irc_scene_learn_on_event,
        irc_scene_vault_on_event,
    },
    .on_exit_handlers = {
        irc_scene_learn_on_exit,
        irc_scene_vault_on_exit,
    },
    .scene_num = IrcSceneCount,
};

const SceneManagerHandlers irc_scene_handlers = irc_scene_handlers_impl;
