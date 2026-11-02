// Main menu — Scan / History / Settings / About

#include "signalstack_scene.h"

typedef enum {
    MainMenuScan = 0,
    MainMenuHistory,
    MainMenuSettings,
    MainMenuAbout,
} MainMenuItem;

static void main_menu_callback(void* ctx, uint32_t index) {
    SignalStackApp* app = ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher,
        SignalStackEventMenuMain + index);
}

void signalstack_scene_main_menu_on_enter(void* ctx) {
    SignalStackApp* app = ctx;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "SignalStack NFC");

    submenu_add_item(app->submenu, "  Scan Tags",    MainMenuScan,     main_menu_callback, app);
    submenu_add_item(app->submenu, "  History",      MainMenuHistory,  main_menu_callback, app);
    submenu_add_item(app->submenu, "  Settings",     MainMenuSettings, main_menu_callback, app);
    submenu_add_item(app->submenu, "  About",        MainMenuAbout,    main_menu_callback, app);

    uint32_t selected = scene_manager_get_scene_state(
        app->scene_manager, SignalStackSceneMainMenu);
    submenu_set_selected_item(app->submenu, selected);

    view_dispatcher_switch_to_view(app->view_dispatcher, SignalStackViewSubmenu);
}

bool signalstack_scene_main_menu_on_event(void* ctx, SceneManagerEvent event) {
    SignalStackApp* app = ctx;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event - SignalStackEventMenuMain) {
        case MainMenuScan:
            scene_manager_set_scene_state(app->scene_manager, SignalStackSceneMainMenu, MainMenuScan);
            scene_manager_next_scene(app->scene_manager, SignalStackSceneScan);
            consumed = true;
            break;
        case MainMenuHistory:
            scene_manager_set_scene_state(app->scene_manager, SignalStackSceneMainMenu, MainMenuHistory);
            scene_manager_next_scene(app->scene_manager, SignalStackSceneHistory);
            consumed = true;
            break;
        case MainMenuSettings:
            scene_manager_set_scene_state(app->scene_manager, SignalStackSceneMainMenu, MainMenuSettings);
            scene_manager_next_scene(app->scene_manager, SignalStackSceneSettings);
            consumed = true;
            break;
        case MainMenuAbout:
            scene_manager_set_scene_state(app->scene_manager, SignalStackSceneMainMenu, MainMenuAbout);
            scene_manager_next_scene(app->scene_manager, SignalStackSceneAbout);
            consumed = true;
            break;
        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        view_dispatcher_stop(app->view_dispatcher);
        consumed = true;
    }

    return consumed;
}

void signalstack_scene_main_menu_on_exit(void* ctx) {
    SignalStackApp* app = ctx;
    submenu_reset(app->submenu);
}
