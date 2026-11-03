// Sub-GHz Raw Signal Capture & Replay — app entry, radio engine, vault I/O
//
// RX flow:
//   furi_hal_subghz_load_preset(OOK650) → set_frequency_and_path(freq) →
//   start_async_rx(sc_rx_callback, app) → callback stores transitions →
//   gap >50ms → cap_active=false → scene fires ScEventCaptureDone
//
// TX flow:
//   idle() → set_frequency_and_path(freq) → start_async_tx(sc_tx_callback, app)
//   → callback returns LevelDuration from stored array →
//   return level_duration_reset() when done

#include "subghz_capture.h"

// ─────────────────────────────────────────────────────────────────────────────
// RX callback — called from Sub-GHz DMA ISR
// ─────────────────────────────────────────────────────────────────────────────

static bool sc_rx_callback(bool level, uint32_t duration, void* ctx) {
    ScApp* app = ctx;
    if(!app->cap_active) return false;

    // End-of-signal gap
    if(!level && duration >= SC_GAP_US) {
        app->cap_active = false;
        return false;
    }

    // Overflow guard
    if(app->cap_count >= SC_TRANSITION_MAX) {
        app->cap_active = false;
        return false;
    }

    app->cap_buf[app->cap_count].duration_us = duration;
    app->cap_buf[app->cap_count].level = level;
    app->cap_count++;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// TX callback — called by Sub-GHz DMA to get next level+duration
// ─────────────────────────────────────────────────────────────────────────────

LevelDuration sc_tx_callback(void* ctx) {
    ScApp* app = ctx;
    if(app->tx_pos >= app->vault[app->vault_sel].count) {
        app->replay_state = ScReplayStateDone;
        return level_duration_reset();
    }
    const ScSignal* sig = &app->vault[app->vault_sel];
    bool     lv  = sig->transitions[app->tx_pos].level;
    uint32_t dur = sig->transitions[app->tx_pos].duration_us;
    app->tx_pos++;
    return level_duration_make(lv, dur);
}

// ─────────────────────────────────────────────────────────────────────────────
// Capture helpers
// ─────────────────────────────────────────────────────────────────────────────

void sc_start_rx(ScApp* app) {
    furi_hal_subghz_reset();
    furi_hal_subghz_idle();
    furi_hal_subghz_load_preset(FuriHalSubGhzPresetOok650Async);
    furi_hal_subghz_set_frequency_and_path(SC_FREQS[app->freq_idx].hz);
    app->cap_count  = 0;
    app->cap_active = true;
    furi_hal_subghz_start_async_rx(sc_rx_callback, app);
}

void sc_stop_rx(ScApp* app) {
    furi_hal_subghz_stop_async_rx();
    furi_hal_subghz_idle();
    app->cap_active = false;
}

// Finalize capture: copy from volatile buffer, compute stats
void sc_finalize_capture(ScApp* app) {
    uint16_t n = (uint16_t)app->cap_count;
    if(n > SC_TRANSITION_MAX) n = SC_TRANSITION_MAX;

    app->current.freq_hz = SC_FREQS[app->freq_idx].hz;
    app->current.count   = n;
    app->current.valid   = (n >= SC_MIN_PULSES);

    uint32_t total = 0, mn = UINT32_MAX, mx = 0;
    for(uint16_t i = 0; i < n; i++) {
        app->current.transitions[i].duration_us = app->cap_buf[i].duration_us;
        app->current.transitions[i].level       = app->cap_buf[i].level;
        uint32_t d = app->cap_buf[i].duration_us;
        total += d;
        if(d < mn) mn = d;
        if(d > mx && d < SC_GAP_US) mx = d;
    }
    app->current.total_us     = total;
    app->current.min_pulse_us = (mn == UINT32_MAX) ? 0 : mn;
    app->current.max_pulse_us = mx;

    // Auto-label
    snprintf(app->current.label, sizeof(app->current.label),
             "%luM #%u",
             (unsigned long)(app->current.freq_hz / 1000000),
             (unsigned)(app->vault_count + 1));
}

void sc_save_to_vault(ScApp* app) {
    if(!app->current.valid) return;
    if(app->vault_count >= SC_VAULT_MAX) {
        // Evict oldest
        memmove(&app->vault[0], &app->vault[1],
                sizeof(ScSignal) * (SC_VAULT_MAX - 1));
        app->vault_count = SC_VAULT_MAX - 1;
    }
    memcpy(&app->vault[app->vault_count], &app->current, sizeof(ScSignal));
    app->vault_count++;
    sc_vault_save(app);
}

// ─────────────────────────────────────────────────────────────────────────────
// TX helpers
// ─────────────────────────────────────────────────────────────────────────────

void sc_start_tx(ScApp* app, uint8_t vault_idx) {
    if(vault_idx >= app->vault_count) return;
    app->vault_sel   = vault_idx;
    app->tx_pos      = 0;
    app->replay_state = ScReplayStateActive;

    furi_hal_subghz_reset();
    furi_hal_subghz_idle();
    furi_hal_subghz_load_preset(FuriHalSubGhzPresetOok650Async);
    furi_hal_subghz_set_frequency_and_path(
        app->vault[vault_idx].freq_hz);
    furi_hal_subghz_start_async_tx(sc_tx_callback, app);
}

void sc_stop_tx(ScApp* app) {
    furi_hal_subghz_stop_async_tx();
    furi_hal_subghz_idle();
    app->replay_state = ScReplayStateIdle;
}

// ─────────────────────────────────────────────────────────────────────────────
// Vault save / load (RC4-encrypted)
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    uint32_t salt;
    uint8_t  count;
    uint8_t  _pad[3];
} ScVaultHeader;

void sc_vault_save(ScApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, "/ext/.system");
    storage_common_mkdir(storage, "/ext/.system/calibration");

    File* f = storage_file_alloc(storage);
    if(!storage_file_open(f, SC_VAULT_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_free(f);
        furi_record_close(RECORD_STORAGE);
        return;
    }

    // Build plaintext blob
    uint32_t blob_sz = sizeof(ScVaultHeader) +
                       app->vault_count * sizeof(ScSignal);
    uint8_t* blob = malloc(blob_sz);
    ScVaultHeader* hdr = (ScVaultHeader*)blob;
    hdr->salt  = SC_VAULT_SALT;
    hdr->count = app->vault_count;
    memcpy(blob + sizeof(ScVaultHeader), app->vault,
           app->vault_count * sizeof(ScSignal));

    // Derive key from device UID
    uint32_t uid = 0;
    const uint8_t* hw_uid = furi_hal_version_get_uid();
    for(uint8_t i = 0; i < 8; i++) uid ^= (uint32_t)hw_uid[i] << ((i % 4) * 8);
    uid ^= SC_VAULT_SALT;

    sc_rc4(blob, blob_sz, uid);
    storage_file_write(f, blob, blob_sz);
    free(blob);

    storage_file_close(f);
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
}

void sc_vault_load(ScApp* app) {
    app->vault_count = 0;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(storage);

    if(!storage_file_open(f, SC_VAULT_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(f);
        furi_record_close(RECORD_STORAGE);
        return;
    }

    uint64_t fsz = storage_file_size(f);
    if(fsz < sizeof(ScVaultHeader)) {
        storage_file_close(f);
        storage_file_free(f);
        furi_record_close(RECORD_STORAGE);
        return;
    }

    uint8_t* blob = malloc((uint32_t)fsz);
    storage_file_read(f, blob, (uint32_t)fsz);
    storage_file_close(f);
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);

    uint32_t uid = 0;
    const uint8_t* hw_uid = furi_hal_version_get_uid();
    for(uint8_t i = 0; i < 8; i++) uid ^= (uint32_t)hw_uid[i] << ((i % 4) * 8);
    uid ^= SC_VAULT_SALT;

    sc_rc4(blob, (uint32_t)fsz, uid);

    ScVaultHeader* hdr = (ScVaultHeader*)blob;
    if(hdr->salt != SC_VAULT_SALT || hdr->count > SC_VAULT_MAX) {
        free(blob);
        return;
    }

    app->vault_count = hdr->count;
    uint32_t copy_sz = hdr->count * sizeof(ScSignal);
    if(sizeof(ScVaultHeader) + copy_sz <= fsz)
        memcpy(app->vault, blob + sizeof(ScVaultHeader), copy_sz);

    free(blob);
}

// ─────────────────────────────────────────────────────────────────────────────
// ViewDispatcher callbacks
// ─────────────────────────────────────────────────────────────────────────────

static bool sc_nav_callback(void* ctx) {
    ScApp* app = ctx;
    scene_manager_handle_back_event(app->scene_manager);
    return true;
}

static bool sc_custom_event_callback(void* ctx, uint32_t event) {
    ScApp* app = ctx;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

// ─────────────────────────────────────────────────────────────────────────────
// App alloc / free
// ─────────────────────────────────────────────────────────────────────────────

static ScApp* sc_app_alloc(void) {
    ScApp* app = malloc(sizeof(ScApp));
    memset(app, 0, sizeof(ScApp));

    app->gui           = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->scene_manager = scene_manager_alloc(&sc_scene_handlers, app);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, sc_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, sc_nav_callback);
    view_dispatcher_attach_to_gui(
        app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->freq_view    = view_alloc();
    app->capture_view = view_alloc();
    app->vault_view   = view_alloc();

    view_allocate_model(app->freq_view,    ViewModelTypeLocking, 256);
    view_allocate_model(app->capture_view, ViewModelTypeLocking, 256);
    view_allocate_model(app->vault_view,   ViewModelTypeLocking, 256);

    view_dispatcher_add_view(app->view_dispatcher, ScViewFreq,    app->freq_view);
    view_dispatcher_add_view(app->view_dispatcher, ScViewCapture, app->capture_view);
    view_dispatcher_add_view(app->view_dispatcher, ScViewVault,   app->vault_view);

    app->freq_idx     = 0;
    app->cap_state    = ScCaptureStateIdle;
    app->replay_state = ScReplayStateIdle;

    sc_vault_load(app);
    return app;
}

static void sc_app_free(ScApp* app) {
    sc_stop_rx(app);
    sc_stop_tx(app);
    furi_hal_subghz_sleep();

    view_dispatcher_remove_view(app->view_dispatcher, ScViewFreq);
    view_dispatcher_remove_view(app->view_dispatcher, ScViewCapture);
    view_dispatcher_remove_view(app->view_dispatcher, ScViewVault);
    view_free(app->freq_view);
    view_free(app->capture_view);
    view_free(app->vault_view);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    free(app);
}

// ─────────────────────────────────────────────────────────────────────────────
// Entry point
// ─────────────────────────────────────────────────────────────────────────────

int32_t subghz_capture_app(void* p) {
    UNUSED(p);
    ScApp* app = sc_app_alloc();

    scene_manager_next_scene(app->scene_manager, ScSceneFreq);
    view_dispatcher_run(app->view_dispatcher);

    sc_app_free(app);
    return 0;
}
