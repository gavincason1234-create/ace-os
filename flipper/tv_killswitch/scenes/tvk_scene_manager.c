#include "../tv_killswitch.h"

static void (*const tvk_on_enter[])(void*) = {
    tvk_scene_menu_on_enter,
    tvk_scene_kill_on_enter,
};
static bool (*const tvk_on_event[])(void*, SceneManagerEvent) = {
    tvk_scene_menu_on_event,
    tvk_scene_kill_on_event,
};
static void (*const tvk_on_exit[])(void*) = {
    tvk_scene_menu_on_exit,
    tvk_scene_kill_on_exit,
};

const SceneManagerHandlers tvk_scene_handlers = {
    .on_enter_handlers = tvk_on_enter,
    .on_event_handlers = tvk_on_event,
    .on_exit_handlers  = tvk_on_exit,
    .scene_num         = TvkSceneCount,
};
