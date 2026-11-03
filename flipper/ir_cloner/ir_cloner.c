// IR Remote Cloner — capture and replay any infrared remote signal
// Company: SignalStack  |  Authorized demonstrations only.
// Demonstrates that IR remote controls have ZERO authentication — any signal
// can be captured once and replayed indefinitely to control any device.

#include "ir_cloner.h"

// ─────────────────────────────────────────────────────────────────────────────
// Vault persistence (RC4-encrypted binary blob)
// ─────────────────────────────────────────────────────────────────────────────

static void irc_key(IrcApp* app, uint8_t key[8]) {
    UNUSED(app);
    const uint8_t* uid = furi_hal_version_uid();
    uint32_t u0 = uid ? (((uint32_t)uid[0] << 24) | ((uint32_t)uid[1] << 16) |
                          ((uint32_t)uid[2] << 8)  | uid[3]) : 0xDEADBEEFUL;
    uint32_t k0 = u0 ^ IRC_VAULT_SALT;
    uint32_t k1 = ~k0 ^ 0xA5A5A5A5UL;
    memcpy(key,     &k0, 4);
    memcpy(key + 4, &k1, 4);
}

static void irc_vault_save(IrcApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, "/ext/.system");
    storage_common_mkdir(storage, "/ext/.system/calibration");

    // Serialize: count + array of IrcSignal
    uint32_t blob_size = sizeof(uint8_t) + app->vault_count * sizeof(IrcSignal);
    uint8_t* blob = malloc(blob_size);
    if(!blob) { furi_record_close(RECORD_STORAGE); return; }
    blob[0] = app->vault_count;
    for(uint8_t i = 0; i < app->vault_count; i++)
        memcpy(blob + 1 + i * sizeof(IrcSignal), &app->vault[i], sizeof(IrcSignal));

    uint8_t key[8];
    irc_key(app, key);
    irc_rc4(blob, blob_size, key, 8);

    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, IRC_VAULT_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(f, blob, blob_size);
        storage_file_close(f);
    }
    storage_file_free(f);
    free(blob);
    furi_record_close(RECORD_STORAGE);
}

static void irc_vault_load(IrcApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(storage);
    if(!storage_file_open(f, IRC_VAULT_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(f);
        furi_record_close(RECORD_STORAGE);
        return;
    }
    uint64_t fsize = storage_file_size(f);
    if(fsize < 1 || fsize > sizeof(uint8_t) + IRC_VAULT_MAX * sizeof(IrcSignal)) {
        storage_file_close(f);
        storage_file_free(f);
        furi_record_close(RECORD_STORAGE);
        return;
    }
    uint8_t* blob = malloc((uint32_t)fsize);
    if(!blob) { storage_file_close(f); storage_file_free(f); furi_record_close(RECORD_STORAGE); return; }
    storage_file_read(f, blob, (uint32_t)fsize);
    storage_file_close(f);
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);

    uint8_t key[8];
    irc_key(app, key);
    irc_rc4(blob, (uint32_t)fsize, key, 8);

    uint8_t count = blob[0];
    if(count > IRC_VAULT_MAX) count = IRC_VAULT_MAX;
    app->vault_count = count;
    for(uint8_t i = 0; i < count; i++)
        memcpy(&app->vault[i], blob + 1 + i * sizeof(IrcSignal), sizeof(IrcSignal));
    free(blob);
}

// ─────────────────────────────────────────────────────────────────────────────
// Save current raw capture into vault
// ─────────────────────────────────────────────────────────────────────────────

void irc_save_current(IrcApp* app) {
    if(app->raw_count == 0) return;

    uint8_t slot;
    if(app->vault_count < IRC_VAULT_MAX) {
        slot = app->vault_count++;
    } else {
        // Evict oldest (slot 0), shift down
        for(uint8_t i = 0; i < IRC_VAULT_MAX - 1; i++)
            app->vault[i] = app->vault[i + 1];
        slot = IRC_VAULT_MAX - 1;
    }

    IrcSignal* s = &app->vault[slot];
    memset(s, 0, sizeof(IrcSignal));

    uint32_t copy = app->raw_count < IRC_TIMING_MAX ? app->raw_count : IRC_TIMING_MAX;
    memcpy(s->timings, app->raw_timings, copy * sizeof(uint32_t));
    s->timing_count = copy;
    s->frequency    = app->raw_freq ? app->raw_freq : 38000;
    s->duty_cycle   = (app->raw_duty > 0.0f) ? app->raw_duty : 0.33f;
    snprintf(s->label, sizeof(s->label), "Signal #%u", (unsigned)app->vault_count);

    s->has_decoded = app->last_decoded;
    if(app->last_decoded) {
        strncpy(s->protocol, app->last_protocol, 11);
        s->address = app->last_address;
        s->command = app->last_command;
    }
    s->valid = true;

    irc_vault_save(app);
}

// ─────────────────────────────────────────────────────────────────────────────
// IR worker callbacks — called from IR worker thread
// ─────────────────────────────────────────────────────────────────────────────

static void irc_signal_received(void* ctx, InfraredWorkerSignal* signal) {
    IrcApp* app = ctx;

    // Reset decoded overlay
    app->last_decoded = false;

    if(infrared_worker_signal_is_decoded(signal)) {
        const InfraredMessage* msg = infrared_worker_get_decoded_signal(signal);
        app->last_decoded = true;
        strncpy(app->last_protocol, infrared_get_protocol_name(msg->protocol), 11);
        app->last_address = msg->address;
        app->last_command = msg->command;
        // Build synthetic raw from decoded message for storage
        // Use the raw signal for actual replay fidelity
    }

    // Always grab raw timings for lossless replay
    const InfraredRawSignal* raw = infrared_worker_get_raw_signal(signal);
    if(raw && raw->timings_size > 0) {
        uint32_t copy = raw->timings_size < IRC_TIMING_MAX ? raw->timings_size : IRC_TIMING_MAX;
        memcpy(app->raw_timings, raw->timings, copy * sizeof(uint32_t));
        app->raw_count = copy;
        app->raw_freq  = raw->frequency;
        app->raw_duty  = raw->duty_cycle;
    } else {
        app->raw_count = 0;
    }

    view_dispatcher_send_custom_event(app->view_dispatcher, IrcEventSignalCaptured);
}

// ─────────────────────────────────────────────────────────────────────────────
// TX signal provider callback — feeds timings to InfraredWorker TX
// ─────────────────────────────────────────────────────────────────────────────

static volatile uint16_t s_tx_pos  = 0;
static const IrcSignal*  s_tx_sig  = NULL;

static void irc_tx_signal_provider(void* ctx, InfraredWorkerSignal* signal) {
    UNUSED(ctx);
    if(!s_tx_sig || s_tx_sig->timing_count == 0) return;
    infrared_worker_signal_set_raw_signal(
        signal,
        s_tx_sig->timings,
        s_tx_sig->timing_count,
        s_tx_sig->frequency,
        s_tx_sig->duty_cycle);
    s_tx_pos = 0;  // mark sent
}

void irc_start_tx(IrcApp* app, const IrcSignal* sig) {
    s_tx_sig = sig;
    s_tx_pos = 0;
    infrared_worker_tx_set_get_signal_callback(app->worker, irc_tx_signal_provider, app);
    infrared_worker_tx_start(app->worker);
    app->transmitting = true;
}

void irc_stop_tx(IrcApp* app) {
    if(app->transmitting) {
        infrared_worker_tx_stop(app->worker);
        app->transmitting = false;
        s_tx_sig = NULL;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// App lifecycle
// ─────────────────────────────────────────────────────────────────────────────

static IrcApp* irc_alloc(void) {
    IrcApp* app = malloc(sizeof(IrcApp));
    memset(app, 0, sizeof(IrcApp));

    app->gui           = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->scene_manager = scene_manager_alloc(&irc_scene_handlers, app);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(app->view_dispatcher);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    app->learn_view = view_alloc();
    view_allocate_model(app->learn_view, ViewModelTypeLocking, 256);
    view_dispatcher_add_view(app->view_dispatcher, IrcViewLearn, app->learn_view);

    app->vault_view = view_alloc();
    view_allocate_model(app->vault_view, ViewModelTypeLocking, 256);
    view_dispatcher_add_view(app->view_dispatcher, IrcViewVault, app->vault_view);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui,
                                  ViewDispatcherTypeFullscreen);

    app->worker = infrared_worker_alloc();

    irc_vault_load(app);
    return app;
}

static void irc_free(IrcApp* app) {
    irc_stop_tx(app);
    infrared_worker_free(app->worker);

    view_dispatcher_remove_view(app->view_dispatcher, IrcViewLearn);
    view_dispatcher_remove_view(app->view_dispatcher, IrcViewVault);
    view_free(app->learn_view);
    view_free(app->vault_view);
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    free(app);
}

int32_t ir_cloner_app(void* p) {
    UNUSED(p);
    IrcApp* app = irc_alloc();
    scene_manager_next_scene(app->scene_manager, IrcSceneLearn);
    view_dispatcher_run(app->view_dispatcher);
    irc_free(app);
    return 0;
}
