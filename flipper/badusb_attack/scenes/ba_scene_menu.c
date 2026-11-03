#include "../badusb_attack.h"

static void menu_cb(void* ctx, uint32_t idx) {
    BaBadUsbApp* app = ctx;
    app->selected = (uint8_t)idx;
    scene_manager_next_scene(app->scene_manager, BaSceneExecute);
}

void ba_scene_menu_on_enter(void* ctx) {
    BaBadUsbApp* app = ctx;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "[ BADUSB ATTACK ]");
    for(uint8_t i = 0; i < BA_PAYLOAD_COUNT; i++) {
        submenu_add_item(app->submenu, BA_PAYLOADS[i].name, i, menu_cb, app);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, BaViewSubmenu);
}

bool ba_scene_menu_on_event(void* ctx, SceneManagerEvent ev) {
    UNUSED(ctx); UNUSED(ev);
    return false;
}

void ba_scene_menu_on_exit(void* ctx) {
    BaBadUsbApp* app = ctx;
    submenu_reset(app->submenu);
}
