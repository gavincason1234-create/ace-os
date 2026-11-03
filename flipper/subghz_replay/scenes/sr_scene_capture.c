// Capture scene — live RSSI bar, OOK receive, real-time waveform preview
// Up/Down = frequency, OK = arm capture, Back = stop

#include "../subghz_replay.h"

// ─────────────────────────────────────────────────────────────────────────────
// View model
// ─────────────────────────────────────────────────────────────────────────────

typedef enum {
    CapStateIdle = 0,     // radio on, showing RSSI, waiting for OK
    CapStateArmed,        // armed — next burst triggers capture
    CapStateCapturing,    // actively accumulating pulses
    CapStateDone,         // burst complete
    CapStateError,        // nothing captured / too short
} CapState;

typedef struct {
    CapState state;
    uint8_t  tick;
    char     freq_label[14];
    char     freq_hint[24];
    uint8_t  rssi_bar;          // 0-60 pixels
    uint16_t duration_count;    // pulses captured so far
    uint32_t total_us;          // microseconds captured
    // Mini live waveform (last 60 samples, normalized 0-20px tall)
    uint8_t  wave[60];
    uint8_t  wave_head;
} CaptureViewModel;

// ─────────────────────────────────────────────────────────────────────────────
// Tick timer — RSSI + waveform update
// ─────────────────────────────────────────────────────────────────────────────

static FuriTimer* s_cap_timer = NULL;

static void cap_tick_cb(void* ctx) {
    SrApp* app = ctx;

    int8_t  rssi_raw  = (int8_t)furi_hal_subghz_get_rssi();
    int16_t rssi_norm = (int16_t)rssi_raw + 120;
    if(rssi_norm < 0)  rssi_norm = 0;
    if(rssi_norm > 60) rssi_norm = 60;

    with_view_model(app->capture_view, CaptureViewModel* m, {
        m->tick++;
        m->rssi_bar = (uint8_t)((m->rssi_bar * 3 + (uint8_t)rssi_norm) / 4);

        // Feed live pulse count into mini waveform
        uint16_t cnt = app->raw_count;
        uint8_t wval = (cnt > 0 && m->state == CapStateCapturing)
                     ? (uint8_t)(cnt % 20) : 0;
        m->wave[m->wave_head % 60] = wval;
        m->wave_head++;

        if(m->state == CapStateCapturing) {
            m->duration_count = cnt;
            m->total_us       = 0;
            for(uint16_t i = 0; i < cnt; i++) {
                int32_t d = app->raw_buf[i];
                m->total_us += (uint32_t)(d < 0 ? -d : d);
            }
        }
    }, true);
}

// ─────────────────────────────────────────────────────────────────────────────
// Draw
// ─────────────────────────────────────────────────────────────────────────────

static void cap_draw(Canvas* canvas, void* model) {
    CaptureViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Header
    canvas_draw_rbox(canvas, 0, 0, 128, 11, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, "SUBGHZ REPLAY");
    const char* state_label[] = {
        "  IDLE ", "  ARMED", " RX... ", " SAVED!", " ERROR "
    };
    canvas_draw_str(canvas, 83, 8, state_label[m->state]);
    canvas_set_color(canvas, ColorBlack);

    // Frequency
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 21, m->freq_label);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 29, m->freq_hint);

    // RSSI bar
    canvas_draw_str(canvas, 2, 37, "SIG");
    canvas_draw_frame(canvas, 18, 31, 62, 7);
    if(m->rssi_bar > 0)
        canvas_draw_box(canvas, 18, 31, m->rssi_bar, 7);

    // Signal strength text
    const char* sig_str;
    if(m->rssi_bar > 45)      sig_str = "STRONG";
    else if(m->rssi_bar > 25) sig_str = "MEDIUM";
    else if(m->rssi_bar > 10) sig_str = "WEAK";
    else                       sig_str = "NONE";
    canvas_draw_str(canvas, 84, 37, sig_str);

    // Mini waveform / capture progress
    if(m->state == CapStateIdle || m->state == CapStateArmed) {
        // Scrolling RSSI wave
        for(uint8_t i = 0; i < 60; i++) {
            uint8_t idx = (uint8_t)((m->wave_head + i) % 60);
            uint8_t h = (uint8_t)(m->wave[idx] * 10 / 20 + 1);
            canvas_draw_box(canvas, 2 + i * 2, 54 - h, 1, h);
        }
    } else if(m->state == CapStateCapturing || m->state == CapStateDone) {
        // Pulse count + duration
        char info[32];
        snprintf(info, sizeof(info), "%u pulses  %lums",
                 (unsigned)m->duration_count,
                 (unsigned long)(m->total_us / 1000));
        canvas_draw_str(canvas, 2, 45, info);

        // Progress fill
        uint16_t pct = (m->duration_count * 60) / SR_RAW_MAX_PULSES;
        canvas_draw_frame(canvas, 2, 48, 62, 5);
        if(pct > 0) canvas_draw_box(canvas, 2, 48, pct, 5);

        if(m->state == CapStateDone)
            canvas_draw_str(canvas, 68, 52, "[OK] save");
    } else if(m->state == CapStateError) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignTop,
                                "Too short — retry");
    }

    canvas_draw_line(canvas, 0, 56, 127, 56);
    if(m->state == CapStateIdle)
        canvas_draw_str(canvas, 0, 63, "[^v]freq  [OK]arm  [<]back");
    else if(m->state == CapStateArmed)
        canvas_draw_str(canvas, 0, 63, "Press remote NOW  [<]cancel");
    else if(m->state == CapStateDone)
        canvas_draw_str(canvas, 0, 63, "[OK]save  [<]discard");
    else
        canvas_draw_str(canvas, 0, 63, "[<]cancel");
}

// ─────────────────────────────────────────────────────────────────────────────
// Input
// ─────────────────────────────────────────────────────────────────────────────

static bool cap_input(InputEvent* ev, void* ctx) {
    SrApp* app = ctx;
    if(ev->type != InputTypeShort) return false;

    switch(ev->key) {
    case InputKeyBack:
        view_dispatcher_send_custom_event(app->view_dispatcher, SrEventBack);
        return true;
    case InputKeyOk: {
        bool is_done = false;
        bool is_idle = false;
        with_view_model(app->capture_view, CaptureViewModel* m, {
            is_done = (m->state == CapStateDone);
            is_idle = (m->state == CapStateIdle || m->state == CapStateError);
        }, false);
        if(is_done) {
            // Save capture to vault
            sr_vault_save(app);
            notification_message(app->notifications, &sequence_success);
            scene_manager_next_scene(app->scene_manager, SrSceneWaveform);
        } else if(is_idle) {
            // Arm — next burst triggers capture
            app->raw_count = 0;
            app->capturing = true;
            with_view_model(app->capture_view, CaptureViewModel* m, {
                m->state            = CapStateArmed;
                m->duration_count   = 0;
                m->total_us         = 0;
            }, true);
        }
        return true;
    }
    case InputKeyUp:
        if(app->freq_idx < SR_FREQ_COUNT - 1) {
            app->freq_idx++;
            view_dispatcher_send_custom_event(app->view_dispatcher, SrEventFreqUp);
        }
        return true;
    case InputKeyDown:
        if(app->freq_idx > 0) {
            app->freq_idx--;
            view_dispatcher_send_custom_event(app->view_dispatcher, SrEventFreqDown);
        }
        return true;
    default:
        return false;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Scene lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void sr_scene_capture_on_enter(void* ctx) {
    SrApp* app = ctx;

    app->raw_count  = 0;
    app->capturing  = false;

    with_view_model(app->capture_view, CaptureViewModel* m, {
        memset(m, 0, sizeof(CaptureViewModel));
        m->state = CapStateIdle;
        strncpy(m->freq_label, SR_FREQ_LABELS[app->freq_idx], 13);
        strncpy(m->freq_hint,  SR_FREQ_HINTS[app->freq_idx],  23);
    }, false);

    view_set_draw_callback(app->capture_view,  cap_draw);
    view_set_input_callback(app->capture_view, cap_input);
    view_set_context(app->capture_view, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, SrViewCapture);

    sr_radio_start_rx(app);

    s_cap_timer = furi_timer_alloc(cap_tick_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(s_cap_timer, 100);

    notification_message(app->notifications, &sequence_single_vibro);
}

bool sr_scene_capture_on_event(void* ctx, SceneManagerEvent ev) {
    SrApp* app = ctx;
    if(ev.type == SceneManagerEventTypeCustom) {
        if(ev.event == SrEventSignalCaptured) {
            app->capturing = false;
            notification_message(app->notifications, &sequence_success);

            with_view_model(app->capture_view, CaptureViewModel* m, {
                if(app->raw_count >= 8) {
                    m->state          = CapStateDone;
                    m->duration_count = app->raw_count;
                } else {
                    m->state = CapStateError;
                }
            }, true);
            return true;
        }
        if(ev.event == SrEventFreqUp || ev.event == SrEventFreqDown) {
            sr_radio_stop_rx(app);
            app->raw_count = 0;
            app->capturing = false;
            with_view_model(app->capture_view, CaptureViewModel* m, {
                m->state = CapStateIdle;
                strncpy(m->freq_label, SR_FREQ_LABELS[app->freq_idx], 13);
                strncpy(m->freq_hint,  SR_FREQ_HINTS[app->freq_idx],  23);
                memset(m->wave, 0, sizeof(m->wave));
            }, true);
            sr_radio_start_rx(app);
            return true;
        }
        if(ev.event == SrEventBack) {
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
    }
    // Also handle: while armed, the raw capture fires asynchronously — done above
    if(ev.type == SceneManagerEventTypeBack) return false;
    return false;
}

void sr_scene_capture_on_exit(void* ctx) {
    SrApp* app = ctx;
    if(s_cap_timer) {
        furi_timer_stop(s_cap_timer);
        furi_timer_free(s_cap_timer);
        s_cap_timer = NULL;
    }
    app->capturing = false;
    sr_radio_stop_rx(app);
}
