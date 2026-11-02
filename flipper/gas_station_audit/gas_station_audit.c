// Gas Station Audit — Main Entry Point
// SignalStack | Full attack surface scanner: BT skimmer + NFC/RFID + Sub-GHz

#include "gas_station_audit.h"

static bool gsa_back_callback(void* ctx) {
    GasStationAuditApp* app = ctx;
    return scene_manager_handle_back_event(app->scene_manager);
}

static bool gsa_custom_event_callback(void* ctx, uint32_t ev) {
    GasStationAuditApp* app = ctx;
    return scene_manager_handle_custom_event(app->scene_manager, ev);
}

GasStationAuditApp* gsa_app_alloc(void) {
    GasStationAuditApp* app = malloc(sizeof(GasStationAuditApp));
    memset(app, 0, sizeof(GasStationAuditApp));

    app->gui           = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->storage       = furi_record_open(RECORD_STORAGE);

    storage_simply_mkdir(app->storage, GSA_APP_DIR);

    app->scene_manager   = scene_manager_alloc(&gsa_scene_handlers, app);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, gsa_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, gsa_back_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // NFC
    app->nfc = nfc_alloc();

    // Views
    app->widget_splash = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, GsaViewSplash,
        widget_get_view(app->widget_splash));

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher, GsaViewSubmenu,
        submenu_get_view(app->submenu));

    app->bt_view = view_alloc();
    view_set_context(app->bt_view, app);
    view_set_draw_callback(app->bt_view, gsa_bt_view_draw);
    view_set_input_callback(app->bt_view, gsa_bt_view_input);
    view_allocate_model(app->bt_view, ViewModelTypeLocking, sizeof(uint8_t));
    view_dispatcher_add_view(app->view_dispatcher, GsaViewBt, app->bt_view);

    app->nfc_view = view_alloc();
    view_set_context(app->nfc_view, app);
    view_set_draw_callback(app->nfc_view, gsa_nfc_view_draw);
    view_set_input_callback(app->nfc_view, gsa_nfc_view_input);
    view_allocate_model(app->nfc_view, ViewModelTypeLocking, sizeof(uint8_t));
    view_dispatcher_add_view(app->view_dispatcher, GsaViewNfc, app->nfc_view);

    app->rf_view = view_alloc();
    view_set_context(app->rf_view, app);
    view_set_draw_callback(app->rf_view, gsa_rf_view_draw);
    view_set_input_callback(app->rf_view, gsa_rf_view_input);
    view_allocate_model(app->rf_view, ViewModelTypeLocking, sizeof(uint8_t));
    view_dispatcher_add_view(app->view_dispatcher, GsaViewRf, app->rf_view);

    app->report_view = view_alloc();
    view_set_context(app->report_view, app);
    view_set_draw_callback(app->report_view, gsa_report_view_draw);
    view_set_input_callback(app->report_view, gsa_report_view_input);
    view_allocate_model(app->report_view, ViewModelTypeLocking, sizeof(uint8_t));
    view_dispatcher_add_view(app->view_dispatcher, GsaViewReport, app->report_view);

    app->text_box = text_box_alloc();
    view_dispatcher_add_view(app->view_dispatcher, GsaViewTextBox,
        text_box_get_view(app->text_box));

    return app;
}

void gsa_app_free(GasStationAuditApp* app) {
    furi_assert(app);

    if(app->bt_timer) {
        furi_timer_stop(app->bt_timer);
        furi_timer_free(app->bt_timer);
    }
    if(app->rf_timer) {
        furi_timer_stop(app->rf_timer);
        furi_timer_free(app->rf_timer);
    }
    if(app->nfc_poller) {
        nfc_poller_stop(app->nfc_poller);
        nfc_poller_free(app->nfc_poller);
    }

    view_dispatcher_remove_view(app->view_dispatcher, GsaViewSplash);
    view_dispatcher_remove_view(app->view_dispatcher, GsaViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, GsaViewBt);
    view_dispatcher_remove_view(app->view_dispatcher, GsaViewNfc);
    view_dispatcher_remove_view(app->view_dispatcher, GsaViewRf);
    view_dispatcher_remove_view(app->view_dispatcher, GsaViewReport);
    view_dispatcher_remove_view(app->view_dispatcher, GsaViewTextBox);

    widget_free(app->widget_splash);
    submenu_free(app->submenu);
    view_free(app->bt_view);
    view_free(app->nfc_view);
    view_free(app->rf_view);
    view_free(app->report_view);
    text_box_free(app->text_box);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    nfc_free(app->nfc);

    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t gas_station_audit_app(void* p) {
    UNUSED(p);
    GasStationAuditApp* app = gsa_app_alloc();
    scene_manager_next_scene(app->scene_manager, GsaSceneSplash);
    view_dispatcher_run(app->view_dispatcher);
    gsa_app_free(app);
    return 0;
}
