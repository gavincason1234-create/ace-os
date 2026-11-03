#include "../tv_killswitch.h"

static void menu_cb(void* ctx, uint32_t idx) {
    TvKillApp* app = ctx;
    app->category = (TvkCategory)idx;
    scene_manager_next_scene(app->scene_manager, TvkSceneKill);
}

void tvk_scene_menu_on_enter(void* ctx) {
    TvKillApp* app = ctx;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "[ TV KILL SWITCH ]");
    for(uint8_t i = 0; i < TvkCategoryCount; i++)
        submenu_add_item(app->submenu, tvk_category_name((TvkCategory)i), i, menu_cb, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, TvkViewSubmenu);
}

bool tvk_scene_menu_on_event(void* ctx, SceneManagerEvent ev) {
    UNUSED(ctx); UNUSED(ev);
    return false;
}

void tvk_scene_menu_on_exit(void* ctx) {
    TvKillApp* app = ctx;
    submenu_reset(app->submenu);
}
