#include "../subghz_replay.h"

static void menu_cb(void* ctx, uint32_t idx) {
    SrApp* app = ctx;
    switch(idx) {
    case 0: scene_manager_next_scene(app->scene_manager, SrSceneCapture); break;
    case 1: scene_manager_next_scene(app->scene_manager, SrSceneVault);   break;
    default: break;
    }
}

void sr_scene_menu_on_enter(void* ctx) {
    SrApp* app = ctx;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "[ SUBGHZ REPLAY ]");
    submenu_add_item(app->submenu, "Capture Signal", 0, menu_cb, app);
    submenu_add_item(app->submenu, "Replay Vault",   1, menu_cb, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, SrViewSubmenu);
}

bool sr_scene_menu_on_event(void* ctx, SceneManagerEvent ev) {
    UNUSED(ctx); UNUSED(ev);
    return false;
}

void sr_scene_menu_on_exit(void* ctx) {
    SrApp* app = ctx;
    submenu_reset(app->submenu);
}
