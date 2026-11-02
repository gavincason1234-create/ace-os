// Splash screen — SignalStack logo + version, auto-advances after 1.5s

#include "signalstack_scene.h"

static void splash_timer_callback(void* ctx) {
    SignalStackApp* app = ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher, SignalStackEventSplashDone);
}

void signalstack_scene_splash_on_enter(void* ctx) {
    SignalStackApp* app = ctx;
    widget_reset(app->widget_splash);

    // Title block
    widget_add_string_element(app->widget_splash, 64, 10,
        AlignCenter, AlignCenter, FontPrimary, "SignalStack");

    // Tagline
    widget_add_string_element(app->widget_splash, 64, 26,
        AlignCenter, AlignCenter, FontSecondary, "NFC  Scanner  v1.0");

    // Frequency badge
    widget_add_frame_element(app->widget_splash, 22, 34, 84, 14, 3);
    widget_add_string_element(app->widget_splash, 64, 41,
        AlignCenter, AlignCenter, FontSecondary, "13.56 MHz  Full Spectrum");

    // Loading dots
    widget_add_string_element(app->widget_splash, 64, 56,
        AlignCenter, AlignCenter, FontSecondary, "Initializing...");

    view_dispatcher_switch_to_view(app->view_dispatcher, SignalStackViewSplash);

    // Auto-advance after 1500 ms
    FuriTimer* timer = furi_timer_alloc(splash_timer_callback, FuriTimerTypeOnce, app);
    furi_timer_start(timer, 1500);
    scene_manager_set_scene_state(app->scene_manager, SignalStackSceneSplash,
        (uint32_t)(uintptr_t)timer);
}

bool signalstack_scene_splash_on_event(void* ctx, SceneManagerEvent event) {
    SignalStackApp* app = ctx;
    if(event.type == SceneManagerEventTypeCustom &&
       event.event == SignalStackEventSplashDone) {
        scene_manager_next_scene(app->scene_manager, SignalStackSceneMainMenu);
        return true;
    }
    return false;
}

void signalstack_scene_splash_on_exit(void* ctx) {
    SignalStackApp* app = ctx;
    FuriTimer* timer = (FuriTimer*)(uintptr_t)
        scene_manager_get_scene_state(app->scene_manager, SignalStackSceneSplash);
    if(timer) {
        furi_timer_stop(timer);
        furi_timer_free(timer);
    }
    widget_reset(app->widget_splash);
}
