// History scene — scrollable list of all session scans + SD log

#include "signalstack_scene.h"

static void history_submenu_callback(void* ctx, uint32_t index) {
    SignalStackApp* app = ctx;
    app->history_selected = index;
    view_dispatcher_send_custom_event(app->view_dispatcher, SignalStackEventMenuHistory);
}

void signalstack_scene_history_on_enter(void* ctx) {
    SignalStackApp* app = ctx;
    submenu_reset(app->submenu);

    char header[32];
    snprintf(header, sizeof(header), "History (%lu scans)", app->history_count);
    submenu_set_header(app->submenu, header);

    if(app->history_count == 0) {
        submenu_add_item(app->submenu, "No scans yet", 0, NULL, NULL);
    } else {
        for(uint32_t i = 0; i < app->history_count; i++) {
            SignalStackScanResult* r = &app->history[app->history_count - 1 - i];
            // Show newest first
            static char label[48];
            snprintf(label, sizeof(label), "%s  %s", r->proto_name, r->uid_str);
            submenu_add_item(app->submenu, label, i, history_submenu_callback, app);
        }
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, SignalStackViewSubmenu);
}

bool signalstack_scene_history_on_event(void* ctx, SceneManagerEvent event) {
    SignalStackApp* app = ctx;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom &&
       event.event == SignalStackEventMenuHistory) {
        scene_manager_next_scene(app->scene_manager, SignalStackSceneHistoryDetail);
        consumed = true;
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void signalstack_scene_history_on_exit(void* ctx) {
    SignalStackApp* app = ctx;
    submenu_reset(app->submenu);
}

// ── History Detail ────────────────────────────────────────────────────────────

void signalstack_scene_history_detail_on_enter(void* ctx) {
    SignalStackApp* app = ctx;

    // history_selected is index from newest-first order
    uint32_t real_idx = app->history_count - 1 - app->history_selected;
    if(real_idx >= app->history_count) real_idx = 0;
    SignalStackScanResult* r = &app->history[real_idx];

    text_box_reset(app->text_box);
    text_box_set_font(app->text_box, TextBoxFontText);

    // Build full detail string
    FuriString* s = furi_string_alloc();
    furi_string_cat_printf(s, "Protocol:\n  %s\n\n", r->proto_name);
    furi_string_cat_printf(s, "UID (%u bytes):\n  %s\n\n", r->uid_len, r->uid_str);
    if(r->extra[0])
        furi_string_cat_printf(s, "Info:\n  %s\n\n", r->extra);
    furi_string_cat_printf(s, "Time:\n  %s\n\n", r->timestamp);
    furi_string_cat_printf(s, "Saved: %s\n", r->saved ? "Yes" : "No");
    furi_string_cat_printf(s, "\nProtocol details:\n  %s",
        signalstack_proto_description(r->protocol));

    text_box_set_text(app->text_box, furi_string_get_cstr(s));
    furi_string_free(s);

    view_dispatcher_switch_to_view(app->view_dispatcher, SignalStackViewTextBox);
}

bool signalstack_scene_history_detail_on_event(void* ctx, SceneManagerEvent event) {
    UNUSED(ctx);
    if(event.type == SceneManagerEventTypeBack) return false;
    return false;
}

void signalstack_scene_history_detail_on_exit(void* ctx) {
    SignalStackApp* app = ctx;
    text_box_reset(app->text_box);
}
