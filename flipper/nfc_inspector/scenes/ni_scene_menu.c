#include "../nfc_inspector.h"

static void menu_cb(void* ctx, uint32_t idx) {
    NiApp* app = ctx;
    switch(idx) {
    case 0: scene_manager_next_scene(app->scene_manager, NiSceneRead);    break;
    case 1:
        if(app->card.valid)
            scene_manager_next_scene(app->scene_manager, NiSceneResult);
        break;
    case 2:
        if(app->card.valid &&
           (app->card.type == NiCardMifareClassic1K ||
            app->card.type == NiCardMifareClassic4K))
            scene_manager_next_scene(app->scene_manager, NiSceneSectors);
        break;
    default: break;
    }
}

void ni_scene_menu_on_enter(void* ctx) {
    NiApp* app = ctx;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "[ NFC INSPECTOR ]");
    submenu_add_item(app->submenu, "Scan Card",     0, menu_cb, app);
    submenu_add_item(app->submenu, "Card Details",  1, menu_cb, app);
    submenu_add_item(app->submenu, "Sector Dump",   2, menu_cb, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, NiViewSubmenu);
}

bool ni_scene_menu_on_event(void* ctx, SceneManagerEvent ev) {
    UNUSED(ctx); UNUSED(ev);
    return false;
}

void ni_scene_menu_on_exit(void* ctx) {
    NiApp* app = ctx;
    submenu_reset(app->submenu);
}
