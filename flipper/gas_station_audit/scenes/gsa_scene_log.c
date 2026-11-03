// Saved Data / Wipe — view encrypted log or shred everything

#include "../gas_station_audit.h"
#include "../gsa_storage.h"

typedef enum {
    LogMenuView = 0,
    LogMenuWipe,
} LogMenuItem;

static void log_menu_cb(void* ctx, uint32_t idx) {
    GasStationAuditApp* app = ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher,
        GsaEventMenuBtScan + 10 + idx);
}

void gsa_scene_log_on_enter(void* ctx) {
    GasStationAuditApp* app = ctx;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "[ENCRYPTED STORE]");
    submenu_add_item(app->submenu, ">> VIEW DECRYPTED LOG", LogMenuView, log_menu_cb, app);
    submenu_add_item(app->submenu, ">> SHRED ALL DATA",     LogMenuWipe, log_menu_cb, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, GsaViewSubmenu);
}

bool gsa_scene_log_on_event(void* ctx, SceneManagerEvent ev) {
    GasStationAuditApp* app = ctx;
    if(ev.type == SceneManagerEventTypeCustom) {
        uint32_t idx = ev.event - GsaEventMenuBtScan - 10;
        if(idx == LogMenuView) {
            // Decrypt and display in text box
            char* buf = malloc(4096);
            if(!buf) return true;
            if(gsa_file_exists(app->storage)) {
                size_t n = gsa_read_encrypted_records(app->storage, buf, 4096);
                if(n == 0) snprintf(buf, 4096, "(no records found)");
            } else {
                snprintf(buf, 4096, "(no encrypted store found)\n\nRun scans first.");
            }
            text_box_reset(app->text_box);
            text_box_set_text(app->text_box, buf);
            text_box_set_font(app->text_box, TextBoxFontText);
            free(buf);
            view_dispatcher_switch_to_view(app->view_dispatcher, GsaViewTextBox);
            return true;
        }
        if(idx == LogMenuWipe) {
            // Shred everything
            gsa_wipe_file(app->storage);
            notification_message(app->notifications, &sequence_success);
            // Bounce back
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
    }
    if(ev.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

void gsa_scene_log_on_exit(void* ctx) {
    GasStationAuditApp* app = ctx;
    submenu_reset(app->submenu);
}
