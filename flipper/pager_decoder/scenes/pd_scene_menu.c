#include "../pager_decoder.h"

static void menu_cb(void* ctx, uint32_t idx) {
    PdApp* app = ctx;
    switch(idx) {
    case 0: scene_manager_next_scene(app->scene_manager, PdSceneScan); break;
    case 1: scene_manager_next_scene(app->scene_manager, PdSceneLog);  break;
    default: break;
    }
}

void pd_scene_menu_on_enter(void* ctx) {
    PdApp* app = ctx;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "[ PAGER DECODER ]");
    submenu_add_item(app->submenu, "Scan / Live Decode", 0, menu_cb, app);
    submenu_add_item(app->submenu, "Message Log",        1, menu_cb, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, PdViewSubmenu);
}

bool pd_scene_menu_on_event(void* ctx, SceneManagerEvent ev) {
    UNUSED(ctx); UNUSED(ev);
    return false;
}

void pd_scene_menu_on_exit(void* ctx) {
    PdApp* app = ctx;
    submenu_reset(app->submenu);
}
