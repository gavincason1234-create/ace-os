// Sub-GHz Spectrum Analyzer — live RF spectrum display
// Company: SignalStack  |  Authorized demonstrations only.
// Passive receive only — the CC1101 measures RSSI at each frequency,
// no signals are transmitted. Sweeps 128 frequency bins across the
// selected IoT band and paints a live bar-chart spectrum with peak hold.
// Demonstrates that 315/433/868/915 MHz bands are full of unencrypted
// transmissions from garage doors, keyfobs, sensors, and smart meters.

#include "subghz_spectrum.h"

// ─────────────────────────────────────────────────────────────────────────────
// Frequency sweep — called from timer, scans 8 bins per call
// ─────────────────────────────────────────────────────────────────────────────

// Advance scan by BINS_PER_TICK bins, measure RSSI at each
#define SS_BINS_PER_TICK 8

void ss_scan_tick(SsApp* app) {
    const SsRange* r = &SS_RANGES[app->range_idx];
    uint32_t span    = r->end_hz - r->start_hz;
    uint32_t bin_hz  = span / SS_BINS;

    for(uint8_t b = 0; b < SS_BINS_PER_TICK; b++) {
        uint8_t  idx  = (uint8_t)((app->scan_pos + b) % SS_BINS);
        uint32_t freq = r->start_hz + (uint32_t)idx * bin_hz;

        // Tune to frequency, brief settle, measure RSSI
        furi_hal_subghz_idle();
        furi_hal_subghz_set_frequency_and_path(freq);
        furi_hal_subghz_rx();
        furi_delay_us(600);   // CC1101 PLL settle + measurement
        float rssi_f = furi_hal_subghz_get_rssi();
        furi_hal_subghz_idle();

        int8_t rssi = (int8_t)(rssi_f < -120.0f ? -120 :
                               rssi_f >  -30.0f ?  -30 : (int8_t)rssi_f);
        uint8_t px  = ss_rssi_to_px(rssi);

        // Exponential smoothing: new = (old*3 + new) / 4
        app->rssi_px[idx] = (uint8_t)((app->rssi_px[idx] * 3 + px) >> 2);

        // Peak hold
        if(app->peak_hold && px > app->peak_px[idx])
            app->peak_px[idx] = px;
        else if(!app->peak_hold)
            app->peak_px[idx] = app->rssi_px[idx];

        // Track cursor RSSI
        if(idx == app->cursor)
            app->cursor_rssi = rssi;
    }

    app->scan_pos = (uint8_t)((app->scan_pos + SS_BINS_PER_TICK) % SS_BINS);

    // When scan_pos wraps, one full sweep is done
    if(app->scan_pos < SS_BINS_PER_TICK) {
        app->sweep_count++;
        view_dispatcher_send_custom_event(app->view_dispatcher, SsEventSweepDone);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Reset spectrum (on range change)
// ─────────────────────────────────────────────────────────────────────────────

void ss_reset(SsApp* app) {
    memset(app->rssi_px, 0, SS_BINS);
    memset(app->peak_px, 0, SS_BINS);
    app->scan_pos    = 0;
    app->sweep_count = 0;
    app->cursor      = SS_BINS / 2;
    app->cursor_rssi = SS_RSSI_FLOOR;
}

// ─────────────────────────────────────────────────────────────────────────────
// App lifecycle
// ─────────────────────────────────────────────────────────────────────────────

static SsApp* ss_alloc(void) {
    SsApp* app = malloc(sizeof(SsApp));
    memset(app, 0, sizeof(SsApp));

    app->gui           = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->scene_manager = scene_manager_alloc(&ss_scene_handlers, app);
    app->peak_hold     = true;
    app->range_idx     = 1;   // default: 400-450 MHz (EU keyfobs)
    app->cursor_rssi   = SS_RSSI_FLOOR;

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(app->view_dispatcher);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    app->range_view = view_alloc();
    view_allocate_model(app->range_view, ViewModelTypeLocking, 256);
    view_dispatcher_add_view(app->view_dispatcher, SsViewRange, app->range_view);

    app->spectrum_view = view_alloc();
    view_allocate_model(app->spectrum_view, ViewModelTypeLocking, 512);
    view_dispatcher_add_view(app->view_dispatcher, SsViewSpectrum, app->spectrum_view);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui,
                                  ViewDispatcherTypeFullscreen);
    return app;
}

static void ss_free(SsApp* app) {
    furi_hal_subghz_idle();
    furi_hal_subghz_sleep();

    view_dispatcher_remove_view(app->view_dispatcher, SsViewRange);
    view_dispatcher_remove_view(app->view_dispatcher, SsViewSpectrum);
    view_free(app->range_view);
    view_free(app->spectrum_view);
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    free(app);
}

int32_t subghz_spectrum_app(void* p) {
    UNUSED(p);
    SsApp* app = ss_alloc();
    scene_manager_next_scene(app->scene_manager, SsSceneRange);
    view_dispatcher_run(app->view_dispatcher);
    ss_free(app);
    return 0;
}
