// Splash screen — hacker terminal boot sequence

#include "../gas_station_audit.h"

static void splash_timer_cb(void* ctx) {
    GasStationAuditApp* app = ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher, GsaEventSplashDone);
}

void gsa_scene_splash_on_enter(void* ctx) {
    GasStationAuditApp* app = ctx;
    widget_reset(app->widget_splash);

    // Title bar
    widget_add_string_element(app->widget_splash, 64, 2,
        AlignCenter, AlignTop, FontPrimary, "SIGNALSTACK");

    // Subtitle
    widget_add_string_element(app->widget_splash, 64, 14,
        AlignCenter, AlignTop, FontSecondary, "GAS STATION AUDIT v1.0");

    // Divider line (using underscores — widgets don't draw lines)
    widget_add_string_element(app->widget_splash, 64, 22,
        AlignCenter, AlignTop, FontSecondary, "------------------------");

    // Boot messages — hacker terminal style
    widget_add_string_element(app->widget_splash, 2, 30,
        AlignLeft, AlignTop, FontSecondary, "> Initializing BT scanner...");
    widget_add_string_element(app->widget_splash, 2, 38,
        AlignLeft, AlignTop, FontSecondary, "> Loading skimmer DB [14]...");
    widget_add_string_element(app->widget_splash, 2, 46,
        AlignLeft, AlignTop, FontSecondary, "> NFC/RFID subsystem ready.");
    widget_add_string_element(app->widget_splash, 2, 54,
        AlignLeft, AlignTop, FontSecondary, "> STEALTH MODE: ON");

    view_dispatcher_switch_to_view(app->view_dispatcher, GsaViewSplash);

    // Auto-advance after 2.5s
    FuriTimer* t = furi_timer_alloc(splash_timer_cb, FuriTimerTypeOnce, app);
    furi_timer_start(t, 2500);
    // Store in scan_tick temporarily (reuse field as timer pointer)
    app->scan_tick = (uint32_t)(uintptr_t)t;
}

bool gsa_scene_splash_on_event(void* ctx, SceneManagerEvent ev) {
    GasStationAuditApp* app = ctx;
    if(ev.type == SceneManagerEventTypeCustom && ev.event == GsaEventSplashDone) {
        // Free the one-shot timer
        FuriTimer* t = (FuriTimer*)(uintptr_t)app->scan_tick;
        furi_timer_free(t);
        app->scan_tick = 0;
        scene_manager_next_scene(app->scene_manager, GsaSceneMainMenu);
        return true;
    }
    return false;
}

void gsa_scene_splash_on_exit(void* ctx) {
    GasStationAuditApp* app = ctx;
    widget_reset(app->widget_splash);
}
