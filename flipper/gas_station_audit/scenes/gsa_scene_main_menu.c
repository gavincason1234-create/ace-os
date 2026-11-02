#include "../gas_station_audit.h"

typedef enum {
    MenuBtScan = 0,
    MenuNfcScan,
    MenuRfMonitor,
    MenuPriceSign,
    MenuChecklist,
    MenuReport,
    MenuLog,
} MenuItem;

static void menu_cb(void* ctx, uint32_t idx) {
    GasStationAuditApp* app = ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher,
        GsaEventMenuBtScan + idx);
}

void gsa_scene_main_menu_on_enter(void* ctx) {
    GasStationAuditApp* app = ctx;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "[SIGNALSTACK] GAS AUDIT");

    submenu_add_item(app->submenu, ">> BT SKIMMER SCAN",   MenuBtScan,    menu_cb, app);
    submenu_add_item(app->submenu, ">> NFC/RFID CARD SCAN",MenuNfcScan,   menu_cb, app);
    submenu_add_item(app->submenu, ">> RF SIGNAL MONITOR", MenuRfMonitor, menu_cb, app);
    submenu_add_item(app->submenu, ">> PRICE SIGN RF AUDIT",MenuPriceSign,menu_cb, app);
    submenu_add_item(app->submenu, ">> FIELD CHECKLIST",   MenuChecklist, menu_cb, app);
    submenu_add_item(app->submenu, ">> PUMP THREAT REPORT",MenuReport,    menu_cb, app);
    submenu_add_item(app->submenu, ">> SAVED DATA / WIPE", MenuLog,       menu_cb, app);

    uint32_t sel = scene_manager_get_scene_state(app->scene_manager, GsaSceneMainMenu);
    submenu_set_selected_item(app->submenu, sel);
    view_dispatcher_switch_to_view(app->view_dispatcher, GsaViewSubmenu);
}

bool gsa_scene_main_menu_on_event(void* ctx, SceneManagerEvent ev) {
    GasStationAuditApp* app = ctx;
    if(ev.type == SceneManagerEventTypeCustom) {
        uint32_t idx = ev.event - GsaEventMenuBtScan;
        scene_manager_set_scene_state(app->scene_manager, GsaSceneMainMenu, idx);
        switch(idx) {
        case MenuBtScan:    scene_manager_next_scene(app->scene_manager, GsaSceneBtScan);     return true;
        case MenuNfcScan:   scene_manager_next_scene(app->scene_manager, GsaSceneNfcScan);   return true;
        case MenuRfMonitor: scene_manager_next_scene(app->scene_manager, GsaSceneRfMonitor); return true;
        case MenuPriceSign: scene_manager_next_scene(app->scene_manager, GsaScenePriceSign); return true;
        case MenuChecklist: scene_manager_next_scene(app->scene_manager, GsaSceneChecklist); return true;
        case MenuReport:    scene_manager_next_scene(app->scene_manager, GsaSceneReport);    return true;
        case MenuLog:       scene_manager_next_scene(app->scene_manager, GsaSceneLog);       return true;
        default: break;
        }
    } else if(ev.type == SceneManagerEventTypeBack) {
        view_dispatcher_stop(app->view_dispatcher);
        return true;
    }
    return false;
}

void gsa_scene_main_menu_on_exit(void* ctx) {
    GasStationAuditApp* app = ctx;
    submenu_reset(app->submenu);
}
