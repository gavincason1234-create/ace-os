// Sub-GHz Replay Attack — OOK/ASK signal capture and replay
// Company: SignalStack  |  Authorized security demonstrations only.
//
// Demonstrates that fixed-code OOK devices (garage openers pre-2010,
// generic wireless key fobs, alarm remotes, gate controls) broadcast
// an unencrypted fixed bit pattern that can be captured and replayed
// indefinitely with any Sub-GHz transceiver.
//
// Rolling-code devices (HCS300, KeeLoq, etc.) are NOT vulnerable to
// simple replay — this app demonstrates the difference visually.
//
// Passive receive only during capture; transmit only on explicit user
// command.  Never transmits on licensed bands without authorization.

#include "subghz_replay.h"
#include <string.h>

// ─────────────────────────────────────────────────────────────────────────────
// Raw capture callback — SubGHz worker feeds pulse/gap durations here
// Called from the SubGHz worker thread (ISR-adjacent, keep fast)
// ─────────────────────────────────────────────────────────────────────────────

static void sr_raw_callback(SubGhzReceiverRawData* data, void* ctx) {
    SrApp* app = ctx;
    if(!app->capturing) return;

    // data->data is an array of int32_t durations (µs):
    //   positive = pulse (carrier on), negative = gap (carrier off)
    for(uint16_t i = 0; i < data->data_count && app->raw_count < SR_RAW_MAX_PULSES; i++) {
        app->raw_buf[app->raw_count++] = data->data[i];
    }

    // Stop capture when buffer fills or gap > 20ms detected (end of burst)
    bool end_of_burst = false;
    if(app->raw_count >= 8) {
        int32_t last = app->raw_buf[app->raw_count - 1];
        if(last < 0 && (-last) > 20000) end_of_burst = true;
    }

    if(app->raw_count >= SR_RAW_MAX_PULSES || end_of_burst) {
        app->capturing = false;
        view_dispatcher_send_custom_event(app->view_dispatcher,
                                         SrEventSignalCaptured);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Radio helpers
// ─────────────────────────────────────────────────────────────────────────────

void sr_radio_start_rx(SrApp* app) {
    furi_hal_subghz_reset();
    furi_hal_subghz_idle();
    furi_hal_subghz_load_preset(FuriHalSubGhzPresetOokAsync);
    furi_hal_subghz_set_frequency_and_path(sr_current_freq(app));

    SubGhzProtocolRegistry* reg = subghz_protocol_registry_items_get_registry();
    app->environment = subghz_environment_alloc();
    subghz_environment_set_protocol_registry(app->environment, reg);

    app->receiver = subghz_receiver_alloc_init(app->environment);
    subghz_receiver_set_filter(app->receiver, SubGhzProtocolFlag_Decodable);
    subghz_receiver_set_raw_rx_callback(app->receiver, sr_raw_callback, app);

    app->worker = subghz_worker_alloc();
    subghz_worker_set_filter(app->worker, SubGhzWorkerFilterRaw);
    subghz_worker_rx_start(app->worker, app->receiver);

    furi_hal_subghz_start_async_rx(subghz_worker_rx_callback, app->worker);
}

void sr_radio_stop_rx(SrApp* app) {
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
}

// Replay: transmit a raw capture using CC1101 OOK async TX
// The raw SubGHz protocol transmitter handles the duration array directly.
void sr_radio_transmit(SrApp* app, const SrCapture* cap) {
    furi_hal_subghz_reset();
    furi_hal_subghz_idle();
    furi_hal_subghz_load_preset(FuriHalSubGhzPresetOokAsync);
    furi_hal_subghz_set_frequency_and_path(cap->freq);

    // Build a SubGhzProtocolDecoderRaw-compatible block for TX.
    // The raw transmitter accepts a flipper .sub file key-value block;
    // we use the lower-level furi_hal_subghz_async_tx path with a
    // direct duration callback instead.

    // TX via furi_hal_subghz_start_async_tx with a callback that
    // feeds durations from cap->durations[].
    // We store the position in a static (single-instance) since
    // the callback signature is fixed (void* context).

    // Use a FuriHalSubGhzAsyncTx callback approach:
    // callback(uint32_t* duration, bool* level, void* ctx) — sets each
    // duration+level pair in sequence.
    UNUSED(app); UNUSED(cap);
    // NOTE: Full async TX implementation requires the async TX callback
    // system.  Provided as a structural placeholder; the real
    // furi_hal_subghz_start_async_tx path is wired in the scene.
}

void sr_radio_stop_tx(SrApp* app) {
    UNUSED(app);
    furi_hal_subghz_stop_async_tx();
    furi_hal_subghz_idle();
    furi_hal_subghz_sleep();
}

// ─────────────────────────────────────────────────────────────────────────────
// Vault helpers
// ─────────────────────────────────────────────────────────────────────────────

// Commit the live raw_buf into the vault
void sr_vault_save(SrApp* app) {
    if(app->raw_count < 4) return;  // too short — noise

    if(app->vault_count >= SR_CAPTURE_MAX) {
        // Evict oldest (shift left)
        memmove(&app->vault[0], &app->vault[1],
                (SR_CAPTURE_MAX - 1) * sizeof(SrCapture));
        app->vault_count = SR_CAPTURE_MAX - 1;
    }

    SrCapture* cap = &app->vault[app->vault_count];
    memset(cap, 0, sizeof(SrCapture));

    cap->freq         = SR_FREQUENCIES[app->freq_idx];
    cap->duration_count = app->raw_count;
    memcpy(cap->durations, app->raw_buf,
           app->raw_count * sizeof(int32_t));
    strncpy(cap->freq_label, SR_FREQ_LABELS[app->freq_idx], 13);
    strncpy(cap->hint,       SR_FREQ_HINTS[app->freq_idx],  23);
    snprintf(cap->label, 20, "Cap #%u", (unsigned)(app->vault_count + 1));
    cap->valid = true;

    sr_analyze(cap);
    app->vault_count++;
    app->vault_sel = app->vault_count - 1;
}

// ─────────────────────────────────────────────────────────────────────────────
// App alloc / free / entry
// ─────────────────────────────────────────────────────────────────────────────

static SrApp* sr_alloc(void) {
    SrApp* app = malloc(sizeof(SrApp));
    memset(app, 0, sizeof(SrApp));

    app->gui           = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->scene_manager = scene_manager_alloc(&sr_scene_handlers, app);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(app->view_dispatcher);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher, SrViewSubmenu,
                             submenu_get_view(app->submenu));

    app->capture_view = view_alloc();
    view_allocate_model(app->capture_view, ViewModelTypeLocking, 256);
    view_dispatcher_add_view(app->view_dispatcher, SrViewCapture, app->capture_view);

    app->vault_view = view_alloc();
    view_allocate_model(app->vault_view, ViewModelTypeLocking, 256);
    view_dispatcher_add_view(app->view_dispatcher, SrViewVault, app->vault_view);

    app->waveform_view = view_alloc();
    view_allocate_model(app->waveform_view, ViewModelTypeLocking, 512);
    view_dispatcher_add_view(app->view_dispatcher, SrViewWaveform, app->waveform_view);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui,
                                  ViewDispatcherTypeFullscreen);

    app->freq_idx = 2;  // default: 433.920 MHz
    return app;
}

static void sr_free(SrApp* app) {
    sr_radio_stop_rx(app);

    view_dispatcher_remove_view(app->view_dispatcher, SrViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, SrViewCapture);
    view_dispatcher_remove_view(app->view_dispatcher, SrViewVault);
    view_dispatcher_remove_view(app->view_dispatcher, SrViewWaveform);

    submenu_free(app->submenu);
    view_free(app->capture_view);
    view_free(app->vault_view);
    view_free(app->waveform_view);
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    free(app);
}

int32_t subghz_replay_app(void* p) {
    UNUSED(p);
    SrApp* app = sr_alloc();
    scene_manager_next_scene(app->scene_manager, SrSceneMenu);
    view_dispatcher_run(app->view_dispatcher);
    sr_free(app);
    return 0;
}
