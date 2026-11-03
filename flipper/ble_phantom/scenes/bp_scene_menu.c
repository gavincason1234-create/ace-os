// Attack mode selection — hacker-style pick list

#include "../ble_phantom.h"

static void menu_callback(void* ctx, uint32_t idx) {
    BlePhantomApp* app = ctx;
    app->mode = (PhantomMode)idx;
    scene_manager_next_scene(app->scene_manager, PhantomSceneAttack);
}

void bp_scene_menu_on_enter(void* ctx) {
    BlePhantomApp* app = ctx;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "[ BLE PHANTOM ]");
    for(uint8_t i = 0; i < PhantomModeCount; i++)
        submenu_add_item(app->submenu, bp_mode_name((PhantomMode)i), i, menu_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, PhantomViewSubmenu);
}

bool bp_scene_menu_on_event(void* ctx, SceneManagerEvent ev) {
    UNUSED(ctx);
    UNUSED(ev);
    return false;
}

void bp_scene_menu_on_exit(void* ctx) {
    BlePhantomApp* app = ctx;
    submenu_reset(app->submenu);
}
