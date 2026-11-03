// Sub-GHz Spectrum Analyzer — spectrum display scene
//
// Layout (128×64 px):
//   Row  0-10:  header bar (band label, sweep count)
//   Row 11-58:  spectrum bars, 128 columns × 48 px max height
//               bars grow upward from row 58 → row 11
//   Row 59-63:  frequency axis (start / mid / end labels + marker ticks)
//
// Controls:
//   Left/Right — move frequency cursor (shows exact freq + RSSI)
//   Up/Down    — change band (returns to range scene)
//   OK         — toggle peak hold on/off
//   Back       — back to range selector

#include "../subghz_spectrum.h"

#define SPEC_TOP    11   // top of spectrum area (y)
#define SPEC_BOT    58   // bottom of spectrum area (y, baseline)
#define SPEC_H      (SPEC_BOT - SPEC_TOP)   // 47px usable height

typedef struct {
    uint8_t  rssi_px[SS_BINS];
    uint8_t  peak_px[SS_BINS];
    uint8_t  cursor;
    int8_t   cursor_rssi;
    uint32_t cursor_freq_khz;
    uint32_t sweep_count;
    bool     peak_hold;
    char     band_label[14];
    uint8_t  scan_pos;   // blinking scan position indicator
    uint8_t  tick;
} SpecViewModel;

static void spec_sync(SsApp* app) {
    const SsRange* r = &SS_RANGES[app->range_idx];
    uint32_t span    = r->end_hz - r->start_hz;
    uint32_t bin_hz  = span / SS_BINS;
    uint32_t cfreq   = (r->start_hz + (uint32_t)app->cursor * bin_hz) / 1000;

    with_view_model(app->spectrum_view, SpecViewModel* m, {
        memcpy(m->rssi_px, app->rssi_px, SS_BINS);
        memcpy(m->peak_px, app->peak_px, SS_BINS);
        m->cursor          = app->cursor;
        m->cursor_rssi     = app->cursor_rssi;
        m->cursor_freq_khz = cfreq;
        m->sweep_count     = app->sweep_count;
        m->peak_hold       = app->peak_hold;
        m->scan_pos        = app->scan_pos;
        strncpy(m->band_label, r->label, 13);
    }, true);
}

// Map bin index to screen x (0-127)
static inline uint8_t bin_to_x(uint8_t bin) {
    return bin;   // 1:1 mapping — 128 bins = 128 pixels
}

// Map a frequency to its nearest bin index within current range
static uint8_t freq_to_bin(const SsRange* r, uint32_t freq_hz) {
    if(freq_hz <= r->start_hz) return 0;
    if(freq_hz >= r->end_hz)   return SS_BINS - 1;
    uint32_t span   = r->end_hz - r->start_hz;
    return (uint8_t)(((freq_hz - r->start_hz) * SS_BINS) / span);
}

static void spec_draw(Canvas* canvas, void* model) {
    SpecViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // ── Header ────────────────────────────────────────────────────────────────
    canvas_draw_rbox(canvas, 0, 0, 128, SPEC_TOP, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, m->band_label);
    char swp[12];
    snprintf(swp, sizeof(swp), "#%lu", (unsigned long)m->sweep_count);
    canvas_draw_str(canvas, 96, 8, swp);
    if(m->peak_hold) canvas_draw_str(canvas, 80, 8, "PK");
    canvas_set_color(canvas, ColorBlack);

    // ── Spectrum bars ──────────────────────────────────────────────────────────
    for(uint8_t x = 0; x < SS_BINS; x++) {
        uint8_t h = m->rssi_px[x];
        if(h > SPEC_H) h = (uint8_t)SPEC_H;
        if(h > 0)
            canvas_draw_line(canvas, x, SPEC_BOT, x, (uint8_t)(SPEC_BOT - h));

        // Peak hold dot (one pixel above bar)
        if(m->peak_hold && m->peak_px[x] > 0) {
            uint8_t ph = m->peak_px[x];
            if(ph > SPEC_H) ph = (uint8_t)SPEC_H;
            canvas_draw_dot(canvas, x, (uint8_t)(SPEC_BOT - ph));
        }
    }

    // ── Cursor line ────────────────────────────────────────────────────────────
    uint8_t cx = m->cursor;
    // Draw dashed vertical cursor
    for(uint8_t y = SPEC_TOP + 1; y <= SPEC_BOT; y += 2)
        canvas_draw_dot(canvas, cx, y);

    // Cursor info box (left or right side depending on cursor position)
    char cinfo[20];
    uint32_t mhz = m->cursor_freq_khz / 1000;
    uint32_t khz = (m->cursor_freq_khz % 1000) / 10;
    snprintf(cinfo, sizeof(cinfo), "%lu.%02luM %ddB",
             (unsigned long)mhz, (unsigned long)khz, (int)m->cursor_rssi);
    uint8_t box_x = (cx < 64) ? (uint8_t)(cx + 2) : (uint8_t)(cx - 52);
    canvas_draw_box(canvas, box_x, SPEC_TOP + 1, 50, 9);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str(canvas, (uint8_t)(box_x + 1), (uint8_t)(SPEC_TOP + 8), cinfo);
    canvas_set_color(canvas, ColorBlack);

    // ── Frequency axis ────────────────────────────────────────────────────────
    canvas_draw_line(canvas, 0, SPEC_BOT + 1, 127, SPEC_BOT + 1);
    canvas_set_font(canvas, FontSecondary);

    // Start freq label
    char fl[8];
    snprintf(fl, sizeof(fl), "%luM",
             (unsigned long)(((SpecViewModel*)model)->cursor_freq_khz / 1000 -
              (((SpecViewModel*)model)->cursor_freq_khz / 1000))); // placeholder
    // Just show static band start/end
    // (reuse m->band_label parsing is complex — use precomputed values)
    canvas_draw_str(canvas, 0, 63, "^");     // left edge marker

    // Scan position indicator — small arrow under scan pos
    if(m->tick & 1) {
        uint8_t sp = m->scan_pos;
        if(sp < 128)
            canvas_draw_dot(canvas, sp, SPEC_BOT + 3);
    }

    // ── Footer nav hint ───────────────────────────────────────────────────────
    // Tight: skip detailed hint to save space; only cursor info shown above
}

static bool spec_input(InputEvent* ev, void* ctx) {
    SsApp* app = ctx;
    if(ev->type != InputTypeShort && ev->type != InputTypeRepeat) return false;
    switch(ev->key) {
    case InputKeyBack:
        view_dispatcher_send_custom_event(app->view_dispatcher, SsEventBack);
        return true;
    case InputKeyLeft:
        if(app->cursor > 0) {
            app->cursor--;
            view_dispatcher_send_custom_event(app->view_dispatcher, SsEventCursorLeft);
        }
        return true;
    case InputKeyRight:
        if(app->cursor < SS_BINS - 1) {
            app->cursor++;
            view_dispatcher_send_custom_event(app->view_dispatcher, SsEventCursorRight);
        }
        return true;
    case InputKeyOk:
        view_dispatcher_send_custom_event(app->view_dispatcher, SsEventTogglePeak);
        return true;
    case InputKeyUp:
        // Cycle to next range
        app->range_idx = (uint8_t)((app->range_idx + 1) % SS_RANGE_COUNT);
        ss_reset(app);
        spec_sync(app);
        return true;
    case InputKeyDown:
        app->range_idx = (uint8_t)((app->range_idx + SS_RANGE_COUNT - 1) % SS_RANGE_COUNT);
        ss_reset(app);
        spec_sync(app);
        return true;
    default:
        return false;
    }
}

// Timer: SS_BINS_PER_TICK bins per call at 8ms → full sweep ~128ms
static FuriTimer* s_spec_timer = NULL;

static void spec_tick_cb(void* ctx) {
    SsApp* app = ctx;
    ss_scan_tick(app);
    with_view_model(app->spectrum_view, SpecViewModel* m, { m->tick++; }, true);
    spec_sync(app);
}

void ss_scene_spectrum_on_enter(void* ctx) {
    SsApp* app = ctx;

    view_set_draw_callback(app->spectrum_view,  spec_draw);
    view_set_input_callback(app->spectrum_view, spec_input);
    view_set_context(app->spectrum_view, app);

    ss_reset(app);

    // Init radio for scanning
    furi_hal_subghz_reset();
    furi_hal_subghz_idle();
    furi_hal_subghz_load_preset(FuriHalSubGhzPreset2FSKDev238Async);

    app->scanning = true;
    spec_sync(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, SsViewSpectrum);

    s_spec_timer = furi_timer_alloc(spec_tick_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(s_spec_timer, 8);   // 8ms per tick → ~8 sweeps/sec
}

bool ss_scene_spectrum_on_event(void* ctx, SceneManagerEvent ev) {
    SsApp* app = ctx;
    if(ev.type == SceneManagerEventTypeCustom) {
        if(ev.event == SsEventSweepDone) {
            spec_sync(app);
            return true;
        }
        if(ev.event == SsEventCursorLeft || ev.event == SsEventCursorRight) {
            spec_sync(app);
            return true;
        }
        if(ev.event == SsEventTogglePeak) {
            app->peak_hold = !app->peak_hold;
            if(!app->peak_hold)
                memset(app->peak_px, 0, SS_BINS);
            spec_sync(app);
            return true;
        }
        if(ev.event == SsEventBack) {
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
    }
    if(ev.type == SceneManagerEventTypeBack) return false;
    return false;
}

void ss_scene_spectrum_on_exit(void* ctx) {
    SsApp* app = ctx;
    if(s_spec_timer) {
        furi_timer_stop(s_spec_timer);
        furi_timer_free(s_spec_timer);
        s_spec_timer = NULL;
    }
    app->scanning = false;
    furi_hal_subghz_idle();
    furi_hal_subghz_sleep();
}
