// Waveform scene — oscilloscope-style view of captured signal
// Shows pulse/gap durations as a digital logic waveform with time axis,
// pulse width histogram, and security verdict.

#include "../subghz_replay.h"

// ─────────────────────────────────────────────────────────────────────────────
// View model — holds a rendered waveform bitmap row
// ─────────────────────────────────────────────────────────────────────────────

#define WF_W   120   // waveform canvas width (px)
#define WF_H    18   // waveform height (px)
#define WF_Y    16   // top of waveform on screen
#define WF_COLS 120  // one column per pixel

typedef struct {
    // Rendered waveform: wf[x] = 1 (high) or 0 (low) for each pixel column
    uint8_t  wf[WF_COLS];
    char     freq_label[14];
    char     label[20];
    uint16_t duration_count;
    uint32_t total_ms;
    int32_t  min_pulse_us;
    int32_t  max_pulse_us;
    uint16_t scroll;         // x-scroll offset in waveform units
    uint8_t  tick;
    // Histogram: 8 buckets of pulse width occurrence
    uint8_t  hist[8];        // normalized 0-8 height
    // Security verdict
    bool     likely_fixed;   // true = fixed code (all pulses similar width)
    bool     likely_rolling; // true = rolling code (varied widths)
} WaveformViewModel;

// ─────────────────────────────────────────────────────────────────────────────
// Build waveform render from raw durations
// ─────────────────────────────────────────────────────────────────────────────

static void wf_render(SrApp* app) {
    if(app->vault_count == 0) return;
    const SrCapture* cap = &app->vault[app->vault_sel];
    if(!cap->valid || cap->duration_count == 0) return;

    // Find total duration to normalize
    uint32_t total_us = cap->total_us;
    if(total_us == 0) return;

    // Map duration array onto WF_COLS pixel columns
    // Each pixel column covers (total_us / WF_COLS) microseconds
    uint32_t us_per_col = total_us / WF_COLS;
    if(us_per_col == 0) us_per_col = 1;

    with_view_model(app->waveform_view, WaveformViewModel* m, {
        memset(m->wf, 0, sizeof(m->wf));
        strncpy(m->freq_label, cap->freq_label,  13);
        strncpy(m->label,      cap->label,        19);
        m->duration_count = cap->duration_count;
        m->total_ms       = cap->total_us / 1000;
        m->min_pulse_us   = cap->min_pulse_us;
        m->max_pulse_us   = cap->max_pulse_us;

        // Render waveform pixels
        uint32_t cursor_us = 0;
        for(uint16_t i = 0; i < cap->duration_count; i++) {
            int32_t  d     = cap->durations[i];
            bool     high  = (d > 0);
            uint32_t dur   = (uint32_t)(d < 0 ? -d : d);
            uint16_t x0    = (uint16_t)(cursor_us / us_per_col);
            uint16_t x1    = (uint16_t)((cursor_us + dur) / us_per_col);
            if(x0 >= WF_COLS) break;
            if(x1 >= WF_COLS) x1 = WF_COLS - 1;
            for(uint16_t x = x0; x <= x1 && x < WF_COLS; x++)
                m->wf[x] = high ? 1 : 0;
            cursor_us += dur;
        }

        // Pulse width histogram (8 buckets spanning min..max)
        memset(m->hist, 0, sizeof(m->hist));
        int32_t range = m->max_pulse_us - m->min_pulse_us;
        if(range < 8) range = 8;
        uint8_t bucket_counts[8] = {0};
        uint8_t max_count = 1;
        for(uint16_t i = 0; i < cap->duration_count; i++) {
            int32_t d = cap->durations[i];
            if(d <= 0) continue;  // skip gaps
            uint8_t bucket = (uint8_t)(((d - m->min_pulse_us) * 7) / range);
            if(bucket >= 8) bucket = 7;
            bucket_counts[bucket]++;
            if(bucket_counts[bucket] > max_count) max_count = bucket_counts[bucket];
        }
        for(uint8_t b = 0; b < 8; b++)
            m->hist[b] = (uint8_t)((bucket_counts[b] * 8) / max_count);

        // Security verdict: fixed code = narrow pulse distribution
        // If all pulses fall in ≤2 buckets → likely fixed code
        uint8_t occupied = 0;
        for(uint8_t b = 0; b < 8; b++)
            if(bucket_counts[b] > 0) occupied++;
        m->likely_fixed   = (occupied <= 2);
        m->likely_rolling = (occupied > 4);
    }, true);
}

// ─────────────────────────────────────────────────────────────────────────────
// Draw — oscilloscope + histogram + verdict
// ─────────────────────────────────────────────────────────────────────────────

static void wf_draw(Canvas* canvas, void* model) {
    WaveformViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Header
    canvas_draw_rbox(canvas, 0, 0, 128, 11, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, m->label);
    canvas_draw_str(canvas, 76, 8, m->freq_label);
    canvas_set_color(canvas, ColorBlack);

    // Oscilloscope frame
    canvas_draw_frame(canvas, 4, WF_Y, WF_W + 4, WF_H + 2);

    // Waveform signal — draw as digital logic
    uint8_t prev = 0;
    for(uint16_t x = 0; x < WF_COLS; x++) {
        uint8_t level = m->wf[(x + m->scroll) % WF_COLS];
        uint8_t px    = (uint8_t)(x + 5);
        uint8_t y_hi  = (uint8_t)(WF_Y + 2);
        uint8_t y_lo  = (uint8_t)(WF_Y + WF_H - 1);

        if(level) {
            canvas_draw_dot(canvas, px, y_hi);  // top line = HIGH
        } else {
            canvas_draw_dot(canvas, px, y_lo);  // bottom line = LOW
        }
        // Draw vertical transitions
        if(x > 0 && level != prev) {
            canvas_draw_line(canvas, px, y_hi, px, y_lo);
        }
        prev = level;
    }

    // Stats row
    canvas_set_font(canvas, FontSecondary);
    char stats[32];
    snprintf(stats, sizeof(stats), "%ums  %upls", (unsigned)m->total_ms,
             (unsigned)m->duration_count);
    canvas_draw_str(canvas, 4, 38, stats);

    // Pulse histogram (8 bars, each 7px wide, at bottom-right)
    for(uint8_t b = 0; b < 8; b++) {
        uint8_t h   = m->hist[b];
        uint8_t bx  = (uint8_t)(68 + b * 8);
        uint8_t by  = 55;
        canvas_draw_frame(canvas, bx, by - 8, 6, 8);
        if(h > 0) canvas_draw_box(canvas, bx, by - h, 6, h);
    }
    canvas_draw_str(canvas, 68, 63, "pw dist");

    // Security verdict
    const char* verdict;
    if(m->likely_fixed)
        verdict = "FIXED CODE  [VULN]";
    else if(m->likely_rolling)
        verdict = "ROLLING? [SAFER]";
    else
        verdict = "MIXED CODE";
    canvas_draw_str(canvas, 4, 47, verdict);

    // Blink cursor on verdict line
    if(m->tick & 1)
        canvas_draw_box(canvas, 0, 42, 2, 6);
}

// ─────────────────────────────────────────────────────────────────────────────
// Input
// ─────────────────────────────────────────────────────────────────────────────

static bool wf_input(InputEvent* ev, void* ctx) {
    SrApp* app = ctx;
    if(ev->type != InputTypeShort) return false;
    if(ev->key == InputKeyBack) {
        view_dispatcher_send_custom_event(app->view_dispatcher, SrEventBack);
        return true;
    }
    if(ev->key == InputKeyLeft || ev->key == InputKeyRight) {
        with_view_model(app->waveform_view, WaveformViewModel* m, {
            if(ev->key == InputKeyLeft)
                m->scroll = (uint16_t)((m->scroll + WF_COLS - 4) % WF_COLS);
            else
                m->scroll = (uint16_t)((m->scroll + 4) % WF_COLS);
        }, true);
        return true;
    }
    return false;
}

// Tick timer for blink
static FuriTimer* s_wf_timer = NULL;
static void wf_tick_cb(void* ctx) {
    SrApp* app = ctx;
    with_view_model(app->waveform_view, WaveformViewModel* m, { m->tick++; }, true);
}

// ─────────────────────────────────────────────────────────────────────────────
// Scene lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void sr_scene_waveform_on_enter(void* ctx) {
    SrApp* app = ctx;

    view_set_draw_callback(app->waveform_view,  wf_draw);
    view_set_input_callback(app->waveform_view, wf_input);
    view_set_context(app->waveform_view, app);

    wf_render(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, SrViewWaveform);

    s_wf_timer = furi_timer_alloc(wf_tick_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(s_wf_timer, 500);
}

bool sr_scene_waveform_on_event(void* ctx, SceneManagerEvent ev) {
    SrApp* app = ctx;
    if(ev.type == SceneManagerEventTypeCustom) {
        if(ev.event == SrEventBack) {
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
    }
    if(ev.type == SceneManagerEventTypeBack) return false;
    return false;
}

void sr_scene_waveform_on_exit(void* ctx) {
    UNUSED(ctx);
    if(s_wf_timer) {
        furi_timer_stop(s_wf_timer);
        furi_timer_free(s_wf_timer);
        s_wf_timer = NULL;
    }
}
