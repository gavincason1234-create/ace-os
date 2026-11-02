// SignalStack NFC Scanner — Main Entry Point
// Momentum Firmware compatible | 13.56 MHz full-spectrum

#include "signalstack_nfc.h"
#include "scenes/signalstack_scene.h"

// ── View dispatcher callback ──────────────────────────────────────────────────
static bool signalstack_back_event_callback(void* ctx) {
    SignalStackApp* app = ctx;
    return scene_manager_handle_back_event(app->scene_manager);
}

static bool signalstack_custom_event_callback(void* ctx, uint32_t event) {
    SignalStackApp* app = ctx;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

// ── App allocation ────────────────────────────────────────────────────────────
SignalStackApp* signalstack_app_alloc(void) {
    SignalStackApp* app = malloc(sizeof(SignalStackApp));
    memset(app, 0, sizeof(SignalStackApp));

    // Core services
    app->gui           = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->storage       = furi_record_open(RECORD_STORAGE);

    // Default settings
    app->auto_save       = true;
    app->vibrate_on_scan = true;
    app->beep_on_scan    = true;
    app->save_raw_nfc    = true;

    // Ensure dirs exist
    storage_simply_mkdir(app->storage, SIGNALSTACK_APP_PATH_PREFIX);
    storage_simply_mkdir(app->storage, SIGNALSTACK_NFC_DIR);

    // Scene / view dispatcher
    app->scene_manager   = scene_manager_alloc(&signalstack_scene_handlers, app);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, signalstack_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, signalstack_back_event_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // NFC
    app->nfc        = nfc_alloc();
    app->nfc_device = nfc_device_alloc();

    // Views
    app->widget_splash = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, SignalStackViewSplash,
        widget_get_view(app->widget_splash));

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher, SignalStackViewSubmenu,
        submenu_get_view(app->submenu));

    app->scan_view = view_alloc();
    view_set_context(app->scan_view, app);
    view_set_draw_callback(app->scan_view, signalstack_scan_view_draw);
    view_set_input_callback(app->scan_view, signalstack_scan_view_input);
    view_dispatcher_add_view(app->view_dispatcher, SignalStackViewScan, app->scan_view);

    app->widget_result = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, SignalStackViewResult,
        widget_get_view(app->widget_result));

    app->text_box = text_box_alloc();
    view_dispatcher_add_view(app->view_dispatcher, SignalStackViewTextBox,
        text_box_get_view(app->text_box));

    app->var_list = variable_item_list_alloc();
    view_dispatcher_add_view(app->view_dispatcher, SignalStackViewVarList,
        variable_item_list_get_view(app->var_list));

    // Load history from log
    signalstack_load_history_from_log(app);

    return app;
}

// ── App deallocation ──────────────────────────────────────────────────────────
void signalstack_app_free(SignalStackApp* app) {
    furi_assert(app);

    if(app->poller) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
    }

    view_dispatcher_remove_view(app->view_dispatcher, SignalStackViewSplash);
    view_dispatcher_remove_view(app->view_dispatcher, SignalStackViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, SignalStackViewScan);
    view_dispatcher_remove_view(app->view_dispatcher, SignalStackViewResult);
    view_dispatcher_remove_view(app->view_dispatcher, SignalStackViewTextBox);
    view_dispatcher_remove_view(app->view_dispatcher, SignalStackViewVarList);

    widget_free(app->widget_splash);
    submenu_free(app->submenu);
    view_free(app->scan_view);
    widget_free(app->widget_result);
    text_box_free(app->text_box);
    variable_item_list_free(app->var_list);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    nfc_device_free(app->nfc_device);
    nfc_free(app->nfc);

    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

// ── App entry point ───────────────────────────────────────────────────────────
int32_t signalstack_nfc_app(void* p) {
    UNUSED(p);
    SignalStackApp* app = signalstack_app_alloc();

    scene_manager_next_scene(app->scene_manager, SignalStackSceneSplash);
    view_dispatcher_run(app->view_dispatcher);

    signalstack_app_free(app);
    return 0;
}
