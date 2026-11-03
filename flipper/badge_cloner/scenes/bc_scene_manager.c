#include "../badge_cloner.h"

static void (*const bc_on_enter[])(void*) = {
    bc_scene_menu_on_enter,
    bc_scene_read_on_enter,
    bc_scene_vault_on_enter,
    bc_scene_emulate_on_enter,
};
static bool (*const bc_on_event[])(void*, SceneManagerEvent) = {
    bc_scene_menu_on_event,
    bc_scene_read_on_event,
    bc_scene_vault_on_event,
    bc_scene_emulate_on_event,
};
static void (*const bc_on_exit[])(void*) = {
    bc_scene_menu_on_exit,
    bc_scene_read_on_exit,
    bc_scene_vault_on_exit,
    bc_scene_emulate_on_exit,
};

const SceneManagerHandlers bc_scene_handlers = {
    .on_enter_handlers = bc_on_enter,
    .on_event_handlers = bc_on_event,
    .on_exit_handlers  = bc_on_exit,
    .scene_num         = BcSceneCount,
};
