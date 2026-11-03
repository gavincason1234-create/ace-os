// Medical Pager Decoder — POCSAG on 152/157 MHz
// Company: SignalStack  |  Authorized demonstrations only.
// Demonstrates that hospital pager traffic is unencrypted 2FSK broadcast.
// Passive receive only — no transmission.  All data received is in the air
// and legally receivable for research under FCC Part 97 / equivalent.

#include "pager_decoder.h"
#include <string.h>

// ─────────────────────────────────────────────────────────────────────────────
// SubGHz POCSAG callback — called by receiver when a frame is decoded
// ─────────────────────────────────────────────────────────────────────────────

void pd_subghz_cb(SubGhzReceiver* receiver, SubGhzProtocolDecoderBase* decoder,
                  void* ctx) {
    UNUSED(receiver);
    PdApp* app = ctx;

    // Pull decoded string from the POCSAG decoder
    // SubGhz protocol decoders expose a get_string method
    FuriString* str = furi_string_alloc();
    SubGhzProtocolStatus status = subghz_protocol_decoder_base_get_string(decoder, str);

    if(status == SubGhzProtocolStatusOk) {
        const char* raw = furi_string_get_cstr(str);

        // Parse: POCSAG strings are formatted as:
        //   "POCSAG512 Address: 1234567 Function: 0 Alpha: Hello World"
        //   "POCSAG1200 Address: 1234567 Function: 3 Numeric: 1-800-555-1234"

        PdMessage msg;
        memset(&msg, 0, sizeof(PdMessage));
        msg.freq  = PD_FREQUENCIES[app->freq_idx];
        msg.baud  = pd_baud_value(app->baud);
        msg.valid = true;

        // Parse capcode
        const char* addr_p = strstr(raw, "Address:");
        if(addr_p) msg.capcode = (uint32_t)strtoul(addr_p + 9, NULL, 10);

        // Determine type and extract text
        const char* alpha_p   = strstr(raw, "Alpha:");
        const char* num_p     = strstr(raw, "Numeric:");
        const char* func3_p   = strstr(raw, "Function: 3");
        if(alpha_p) {
            msg.msg_type = PdMsgAlpha;
            strncpy(msg.text, alpha_p + 7, PD_MSG_TEXT_LEN - 1);
        } else if(num_p) {
            msg.msg_type = PdMsgNumeric;
            strncpy(msg.text, num_p + 9, PD_MSG_TEXT_LEN - 1);
        } else if(func3_p) {
            msg.msg_type = PdMsgTone;
            strncpy(msg.text, "(tone only)", PD_MSG_TEXT_LEN - 1);
        } else {
            // Fallback: copy raw
            strncpy(msg.text, raw, PD_MSG_TEXT_LEN - 1);
        }
        msg.text[PD_MSG_TEXT_LEN - 1] = '\0';

        // Add to circular log
        uint8_t slot = app->log_count % PD_MSG_LOG_SIZE;
        app->log[slot] = msg;
        app->log_count++;
        app->last = msg;
        app->total_rx++;

        view_dispatcher_send_custom_event(app->view_dispatcher,
                                         PdEventMsgReceived);
    }
    furi_string_free(str);
}

// ─────────────────────────────────────────────────────────────────────────────
// Radio setup helpers
// ─────────────────────────────────────────────────────────────────────────────

void pd_radio_start(PdApp* app) {
    // Configure CC1101 for POCSAG 2FSK receive
    furi_hal_subghz_reset();
    furi_hal_subghz_idle();
    furi_hal_subghz_load_preset(FuriHalSubGhzPreset2FSKDev238Async);
    furi_hal_subghz_set_frequency_and_path(PD_FREQUENCIES[app->freq_idx]);

    // Build receiver with POCSAG protocol
    SubGhzProtocolRegistry* registry = subghz_protocol_registry_items_get_registry();

    app->environment = subghz_environment_alloc();
    subghz_environment_set_protocol_registry(app->environment, registry);

    app->receiver = subghz_receiver_alloc_init(app->environment);
    subghz_receiver_set_filter(app->receiver, SubGhzProtocolFlag_Decodable);
    subghz_receiver_set_rx_callback(app->receiver, pd_subghz_cb, app);

    app->worker = subghz_worker_alloc();
    subghz_worker_set_overrun_callback(app->worker, NULL);
    subghz_worker_set_pair_callback(app->worker, NULL);
    subghz_worker_set_filter(app->worker, SubGhzWorkerFilterRaw);
    subghz_worker_rx_start(app->worker, app->receiver);

    furi_hal_subghz_start_async_rx(subghz_worker_rx_callback, app->worker);
}

void pd_radio_stop(PdApp* app) {
    if(!app->worker) return;
    furi_hal_subghz_stop_async_rx();
    subghz_worker_rx_stop(app->worker);
    subghz_worker_free(app->worker);
    app->worker = NULL;
    subghz_receiver_free(app->receiver);
    app->receiver = NULL;
    subghz_environment_free(app->environment);
    app->environment = NULL;
    furi_hal_subghz_idle();
    furi_hal_subghz_sleep();
}

// ─────────────────────────────────────────────────────────────────────────────
// App alloc / free / entry
// ─────────────────────────────────────────────────────────────────────────────

static PdApp* pd_alloc(void) {
    PdApp* app = malloc(sizeof(PdApp));
    memset(app, 0, sizeof(PdApp));

    app->gui           = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->scene_manager = scene_manager_alloc(&pd_scene_handlers, app);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(app->view_dispatcher);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher, PdViewSubmenu,
                             submenu_get_view(app->submenu));

    app->scan_view = view_alloc();
    view_allocate_model(app->scan_view, ViewModelTypeLocking, 256);
    view_dispatcher_add_view(app->view_dispatcher, PdViewScan, app->scan_view);

    app->log_view = view_alloc();
    view_allocate_model(app->log_view, ViewModelTypeLocking, 256);
    view_dispatcher_add_view(app->view_dispatcher, PdViewLog, app->log_view);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui,
                                  ViewDispatcherTypeFullscreen);

    app->freq_idx = 0;
    app->baud     = PdBaud1200;

    return app;
}

static void pd_free(PdApp* app) {
    pd_radio_stop(app);

    view_dispatcher_remove_view(app->view_dispatcher, PdViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, PdViewScan);
    view_dispatcher_remove_view(app->view_dispatcher, PdViewLog);

    submenu_free(app->submenu);
    view_free(app->scan_view);
    view_free(app->log_view);
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    free(app);
}

int32_t pager_decoder_app(void* p) {
    UNUSED(p);
    PdApp* app = pd_alloc();
    scene_manager_next_scene(app->scene_manager, PdSceneMenu);
    view_dispatcher_run(app->view_dispatcher);
    pd_free(app);
    return 0;
}
