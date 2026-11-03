// BLE Phantom Spammer — authorized cybersecurity demo tool
// Company: SignalStack  |  Use only with written authorization.
// Floods nearby devices with fake BLE pairing popups to demonstrate
// how unencrypted/unauthenticated BLE advertisements can be spoofed.

#include "ble_phantom.h"

// ─────────────────────────────────────────────────────────────────────────────
// App allocate
// ─────────────────────────────────────────────────────────────────────────────

static BlePhantomApp* ble_phantom_alloc(void) {
    BlePhantomApp* app = malloc(sizeof(BlePhantomApp));
    memset(app, 0, sizeof(BlePhantomApp));

    app->mode  = PhantomModeAppleAll;
    app->speed = PhantomSpeedNormal;

    // Services
    app->gui           = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    // Scene manager
    app->scene_manager = scene_manager_alloc(&bp_scene_handlers, app);

    // View dispatcher
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(app->view_dispatcher);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    // Submenu (mode select)
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher,
                             PhantomViewSubmenu,
                             submenu_get_view(app->submenu));

    // Attack view — manual draw / input callbacks set per-scene
    app->attack_view = view_alloc();
    view_allocate_model(app->attack_view, ViewModelTypeLocking, 256);
    view_dispatcher_add_view(app->view_dispatcher,
                             PhantomViewAttack,
                             app->attack_view);

    view_dispatcher_attach_to_gui(app->view_dispatcher,
                                  app->gui,
                                  ViewDispatcherTypeFullscreen);

    return app;
}

// ─────────────────────────────────────────────────────────────────────────────
// App free
// ─────────────────────────────────────────────────────────────────────────────

static void ble_phantom_free(BlePhantomApp* app) {
    // Make sure timer and beacon are stopped
    if(app->timer) {
        furi_timer_stop(app->timer);
        furi_timer_free(app->timer);
    }
    furi_hal_bt_extra_beacon_stop();

    view_dispatcher_remove_view(app->view_dispatcher, PhantomViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, PhantomViewAttack);
    submenu_free(app->submenu);
    view_free(app->attack_view);
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    free(app);
}

// ─────────────────────────────────────────────────────────────────────────────
// Entry point
// ─────────────────────────────────────────────────────────────────────────────

int32_t ble_phantom_app(void* p) {
    UNUSED(p);

    BlePhantomApp* app = ble_phantom_alloc();

    scene_manager_next_scene(app->scene_manager, PhantomSceneMenu);
    view_dispatcher_run(app->view_dispatcher);

    ble_phantom_free(app);
    return 0;
}
