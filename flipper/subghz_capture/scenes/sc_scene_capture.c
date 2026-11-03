// Sub-GHz Capture — live capture scene
//
// Armed    → waiting for first pulse, blinking antenna icon
// Receiving → progress bar grows as transitions fill buffer
// Done     → signal stats: pulses, min/max timing, duration
//            OK saves to vault; Right → vault browser; Back rearms
// Overflow  → "BUFFER FULL" — OK saves partial capture
// TooShort  → "TOO SHORT / NOISE?" — Back rearms

#include "../subghz_capture.h"

#define CAP_UPDATE_MS  120   // timer period

typedef struct {
    ScCaptureState state;
    uint8_t        tick;
    uint16_t       pulse_count;
    uint32_t       total_us;
    uint32_t       min_pulse_us;
    uint32_t       max_pulse_us;
    uint32_t       freq_hz;
    char           freq_label[16];
    uint8_t        vault_count;
} CaptureViewModel;

// ─────────────────────────────────────────────────────────────────────────────
// Miniature waveform — shows last 64 high/low durations as mini bars
// ─────────────────────────────────────────────────────────────────────────────

static void draw_waveform(Canvas* canvas, const ScApp* app,
                          uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
    uint16_t n = (uint16_t)app->cap_count;
    if(n == 0) return;

    // Show up to w transitions, rightmost = most recent
    uint16_t start = (n > w) ? (uint16_t)(n - w) : 0;
    uint16_t disp  = n - start;

    // Find max duration for scaling (cap at 10ms to ignore long gaps)
    uint32_t dmax = 1;
    for(uint16_t i = start; i < n; i++) {
        uint32_t d = app->cap_buf[i].duration_us;
        if(d < 10000 && d > dmax) dmax = d;
    }

    for(uint16_t i = 0; i < disp; i++) {
        uint16_t idx = start + i;
        uint32_t d   = app->cap_buf[idx].duration_us;
        bool     lv  = app->cap_buf[idx].level;
        if(d > 10000) d = 10000;
        uint8_t bar_h = (uint8_t)((d * h) / dmax);
        if(bar_h == 0) bar_h = 1;
        uint8_t bx = (uint8_t)(x + i);
        if(lv)
            canvas_draw_line(canvas, bx, (uint8_t)(y + h - bar_h), bx, (uint8_t)(y + h));
        else
            canvas_draw_dot(canvas, bx, (uint8_t)(y + h));
    }

    // Waveform border
    canvas_draw_frame(canvas, x, y, w, (uint8_t)(h + 1));
}

static void cap_draw(Canvas* canvas, void* model) {
    CaptureViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Header
    canvas_draw_rbox(canvas, 0, 0, 128, 11, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, m->freq_label);
    char vc[8];
    snprintf(vc, sizeof(vc), "[%u]", (unsigned)m->vault_count);
    canvas_draw_str(canvas, 100, 8, vc);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontSecondary);

    switch(m->state) {
    case ScCaptureStateIdle:
    case ScCaptureStateArmed: {
        // Blinking antenna
        if(m->tick & 1) {
            canvas_draw_str(canvas, 42, 32, "((( o )))");
        } else {
            canvas_draw_str(canvas, 42, 32, "         ");
        }
        canvas_draw_str(canvas, 18, 46, "Waiting for RF signal...");
        canvas_draw_str(canvas, 10, 58, "Right: vault   Back: freq");
        break;
    }

    case ScCaptureStateReceiving: {
        // Progress bar: transitions / max
        uint8_t pct = (uint8_t)((m->pulse_count * 64) / SC_TRANSITION_MAX);
        canvas_draw_frame(canvas, 2, 14, 64, 6);
        canvas_draw_box(canvas, 2, 14, pct, 6);
        char pcnt[24];
        snprintf(pcnt, sizeof(pcnt), "%u pulses", (unsigned)m->pulse_count);
        canvas_draw_str(canvas, 70, 20, pcnt);
        canvas_draw_str(canvas, 2, 32, ">>> RECEIVING <<<");
        break;
    }

    case ScCaptureStateDone: {
        // Stats
        char line1[24], line2[24], line3[24];
        snprintf(line1, sizeof(line1), "Pulses: %u", (unsigned)m->pulse_count);
        snprintf(line2, sizeof(line2), "Min:%luus Max:%luus",
                 (unsigned long)m->min_pulse_us,
                 (unsigned long)m->max_pulse_us);
        uint32_t ms = m->total_us / 1000;
        snprintf(line3, sizeof(line3), "Duration: %lums", (unsigned long)ms);

        canvas_draw_rbox(canvas, 0, 12, 128, 9, 2);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, 20, 19, "[OK]  SIGNAL CAPTURED");
        canvas_set_color(canvas, ColorBlack);

        canvas_draw_str(canvas, 2, 30, line1);
        canvas_draw_str(canvas, 2, 40, line2);
        canvas_draw_str(canvas, 2, 50, line3);
        canvas_draw_str(canvas, 2, 62, "OK:save  Right:vault  Back:rearm");
        break;
    }

    case ScCaptureStateOverflow: {
        canvas_draw_str(canvas, 10, 26, "BUFFER FULL");
        char l[24];
        snprintf(l, sizeof(l), "%d transitions", SC_TRANSITION_MAX);
        canvas_draw_str(canvas, 10, 38, l);
        canvas_draw_str(canvas, 2, 52, "OK: save partial  Back: rearm");
        break;
    }

    case ScCaptureStateTooShort: {
        canvas_draw_str(canvas, 10, 30, "TOO SHORT / NOISE?");
        canvas_draw_str(canvas, 10, 44, "Press transmit again");
        canvas_draw_str(canvas, 10, 58, "Back: rearm");
        break;
    }
    }
}

static bool cap_input(InputEvent* ev, void* ctx) {
    ScApp* app = ctx;
    if(ev->type != InputTypeShort) return false;
    switch(ev->key) {
    case InputKeyBack:
        view_dispatcher_send_custom_event(app->view_dispatcher, ScEventBack);
        return true;
    case InputKeyOk:
        if(app->cap_state == ScCaptureStateDone ||
           app->cap_state == ScCaptureStateOverflow) {
            // Save to vault
            sc_finalize_capture(app);
            sc_save_to_vault(app);
            notification_message(app->notifications, &sequence_double_vibro);
            // Rearm
            app->cap_state = ScCaptureStateArmed;
            sc_start_rx(app);
            with_view_model(app->capture_view, CaptureViewModel* m, {
                m->state       = ScCaptureStateArmed;
                m->pulse_count = 0;
                m->vault_count = app->vault_count;
            }, true);
        }
        return true;
    case InputKeyRight:
        view_dispatcher_send_custom_event(app->view_dispatcher, ScEventVaultSelected);
        return true;
    default:
        return false;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Timer: poll cap_active + cap_count, update state
// ─────────────────────────────────────────────────────────────────────────────

static FuriTimer* s_cap_timer = NULL;

static void cap_tick_cb(void* ctx) {
    ScApp* app = ctx;

    // Detect end of signal (cap_active went false from ISR)
    if(app->cap_state == ScCaptureStateArmed ||
       app->cap_state == ScCaptureStateReceiving) {

        uint16_t n = (uint16_t)app->cap_count;

        if(n > 0 && app->cap_state == ScCaptureStateArmed)
            app->cap_state = ScCaptureStateReceiving;

        if(!app->cap_active && n > 0) {
            sc_stop_rx(app);
            sc_finalize_capture(app);
            if(n < SC_MIN_PULSES)
                app->cap_state = ScCaptureStateTooShort;
            else if(n >= SC_TRANSITION_MAX)
                app->cap_state = ScCaptureStateOverflow;
            else
                app->cap_state = ScCaptureStateDone;

            if(app->cap_state == ScCaptureStateDone)
                notification_message(app->notifications, &sequence_single_vibro);
        }
    }

    with_view_model(app->capture_view, CaptureViewModel* m, {
        m->tick++;
        m->state       = app->cap_state;
        m->pulse_count = (uint16_t)app->cap_count;
        if(app->cap_state >= ScCaptureStateDone) {
            m->total_us      = app->current.total_us;
            m->min_pulse_us  = app->current.min_pulse_us;
            m->max_pulse_us  = app->current.max_pulse_us;
        }
        m->vault_count = app->vault_count;
    }, true);
}

void sc_scene_capture_on_enter(void* ctx) {
    ScApp* app = ctx;

    view_set_draw_callback(app->capture_view,  cap_draw);
    view_set_input_callback(app->capture_view, cap_input);
    view_set_context(app->capture_view, app);

    app->cap_state = ScCaptureStateArmed;

    with_view_model(app->capture_view, CaptureViewModel* m, {
        m->state       = ScCaptureStateArmed;
        m->tick        = 0;
        m->pulse_count = 0;
        m->total_us    = 0;
        m->freq_hz     = SC_FREQS[app->freq_idx].hz;
        m->vault_count = app->vault_count;
        strncpy(m->freq_label, SC_FREQS[app->freq_idx].label,
                sizeof(m->freq_label) - 1);
        m->freq_label[sizeof(m->freq_label) - 1] = '\0';
    }, true);

    view_dispatcher_switch_to_view(app->view_dispatcher, ScViewCapture);
    sc_start_rx(app);

    s_cap_timer = furi_timer_alloc(cap_tick_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(s_cap_timer, CAP_UPDATE_MS);
}

bool sc_scene_capture_on_event(void* ctx, SceneManagerEvent ev) {
    ScApp* app = ctx;
    if(ev.type == SceneManagerEventTypeCustom) {
        if(ev.event == ScEventVaultSelected) {
            scene_manager_next_scene(app->scene_manager, ScSceneVault);
            return true;
        }
        if(ev.event == ScEventBack) {
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
    }
    if(ev.type == SceneManagerEventTypeBack) return false;
    return false;
}

void sc_scene_capture_on_exit(void* ctx) {
    ScApp* app = ctx;
    if(s_cap_timer) {
        furi_timer_stop(s_cap_timer);
        furi_timer_free(s_cap_timer);
        s_cap_timer = NULL;
    }
    sc_stop_rx(app);
}
