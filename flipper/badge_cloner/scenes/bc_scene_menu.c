#include "../badge_cloner.h"

static void menu_cb(void* ctx, uint32_t idx) {
    BcApp* app = ctx;
    switch(idx) {
    case 0: scene_manager_next_scene(app->scene_manager, BcSceneRead);    break;
    case 1: scene_manager_next_scene(app->scene_manager, BcSceneVault);   break;
    case 2: scene_manager_next_scene(app->scene_manager, BcSceneEmulate); break;
    default: break;
    }
}

void bc_scene_menu_on_enter(void* ctx) {
    BcApp* app = ctx;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "[ BADGE CLONER ]");
    submenu_add_item(app->submenu, "Read Badge",   0, menu_cb, app);
    submenu_add_item(app->submenu, "Vault",        1, menu_cb, app);
    submenu_add_item(app->submenu, "Emulate",      2, menu_cb, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, BcViewSubmenu);
}

bool bc_scene_menu_on_event(void* ctx, SceneManagerEvent ev) {
    UNUSED(ctx); UNUSED(ev);
    return false;
}

void bc_scene_menu_on_exit(void* ctx) {
    BcApp* app = ctx;
    submenu_reset(app->submenu);
}
