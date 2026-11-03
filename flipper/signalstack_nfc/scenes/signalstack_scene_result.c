// Result scene — full detail view of last scanned tag

#include "signalstack_scene.h"

void signalstack_scene_result_on_enter(void* ctx) {
    SignalStackApp* app = ctx;
    SignalStackScanResult* r = &app->last_scan;

    widget_reset(app->widget_result);

    // Header
    widget_add_string_element(app->widget_result, 64, 3,
        AlignCenter, AlignTop, FontPrimary, "Scan Result");
    widget_add_string_element(app->widget_result, 64, 14,
        AlignCenter, AlignTop, FontSecondary, r->proto_name);

    // UID row
    widget_add_string_element(app->widget_result, 2, 24,
        AlignLeft, AlignTop, FontSecondary, "UID:");
    widget_add_string_element(app->widget_result, 30, 24,
        AlignLeft, AlignTop, FontSecondary, r->uid_str);

    // Extra info
    if(r->extra[0]) {
        widget_add_string_element(app->widget_result, 2, 34,
            AlignLeft, AlignTop, FontSecondary, r->extra);
    }

    // Timestamp
    widget_add_string_element(app->widget_result, 2, 44,
        AlignLeft, AlignTop, FontSecondary, r->timestamp);

    // Saved status
    widget_add_string_element(app->widget_result, 2, 54,
        AlignLeft, AlignTop, FontSecondary,
        r->saved ? "Saved to SD card" : "Not saved");

    // Bottom hint
    widget_add_string_element(app->widget_result, 64, 62,
        AlignCenter, AlignTop, FontSecondary, "[Back] Scan again");

    view_dispatcher_switch_to_view(app->view_dispatcher, SignalStackViewResult);
}

bool signalstack_scene_result_on_event(void* ctx, SceneManagerEvent event) {
    UNUSED(ctx);
    UNUSED(event);
    return false;
}

void signalstack_scene_result_on_exit(void* ctx) {
    SignalStackApp* app = ctx;
    widget_reset(app->widget_result);
}
