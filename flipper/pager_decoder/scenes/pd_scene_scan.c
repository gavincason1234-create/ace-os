// Live scan scene — radio listening, real-time POCSAG decode display
// Up/Down = freq, Left = baud cycle, OK = freeze/resume, Back = stop

#include "../pager_decoder.h"

// ─────────────────────────────────────────────────────────────────────────────
// View model
// ─────────────────────────────────────────────────────────────────────────────

typedef enum {
    ScanStateListening = 0,
    ScanStateDecoded,
    ScanStateFrozen,
} ScanState;

typedef struct {
    ScanState  state;
    uint8_t    tick;
    uint8_t    rssi_bar;     // 0-60 pixels wide
    uint32_t   freq;
    char       freq_label[16];
    char       baud_label[6];
    uint32_t   total_rx;

    // Last decoded message
    uint32_t   capcode;
    char       msg_type[6];
    char       text[PD_MSG_TEXT_LEN];
    bool       has_msg;
} ScanViewModel;

// ─────────────────────────────────────────────────────────────────────────────
// Tick timer — updates RSSI bar and blink animation
// ─────────────────────────────────────────────────────────────────────────────

static FuriTimer* s_scan_timer = NULL;

static void scan_tick_cb(void* ctx) {
    PdApp* app = ctx;
    // Read RSSI from radio (returns negative dBm; scale to 0-60)
    int8_t rssi_raw = (int8_t)furi_hal_subghz_get_rssi();
    // Typical range: -120 (nothing) to -40 (strong)
    // Map -120...-40 → 0...60 pixels
    int16_t rssi_norm = (int16_t)rssi_raw + 120;
    if(rssi_norm < 0)  rssi_norm = 0;
    if(rssi_norm > 60) rssi_norm = 60;
    // Apply to ~30-sample exponential smoothing for stable display
    // rssi_bar is uint8, blend new reading
    with_view_model(app->scan_view, ScanViewModel* m, {
        m->tick++;
        m->total_rx  = app->total_rx;
        m->rssi_bar  = (uint8_t)((m->rssi_bar * 3 + (uint8_t)rssi_norm) / 4);
    }, true);
}

// ─────────────────────────────────────────────────────────────────────────────
// Draw
// ─────────────────────────────────────────────────────────────────────────────

static void scan_draw(Canvas* canvas, void* model) {
    ScanViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // ── Header ───────────────────────────────────────────────────────────────
    canvas_draw_rbox(canvas, 0, 0, 128, 11, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, "PAGER DECODER");
    if(m->state == ScanStateFrozen)
        canvas_draw_str(canvas, 85, 8, "[FREEZE]");
    else if(m->total_rx > 0 && (m->tick & 1))
        canvas_draw_str(canvas, 91, 8, "[RX]");
    canvas_set_color(canvas, ColorBlack);

    // ── Freq + baud ───────────────────────────────────────────────────────────
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 20, m->freq_label);
    char baud_str[14];
    snprintf(baud_str, sizeof(baud_str), "%s baud", m->baud_label);
    canvas_draw_str(canvas, 80, 20, baud_str);

    // ── RSSI bar ─────────────────────────────────────────────────────────────
    canvas_draw_str(canvas, 2, 28, "RSSI");
    canvas_draw_frame(canvas, 26, 22, 62, 7);
    if(m->rssi_bar > 0) canvas_draw_box(canvas, 26, 22, m->rssi_bar, 7);
    canvas_draw_str(canvas, 92, 28, "dBm");

    // ── Message area ──────────────────────────────────────────────────────────
    if(!m->has_msg) {
        // Scanning animation: scrolling dots
        canvas_set_font(canvas, FontPrimary);
        const char* dots[] = {"LISTENING .", "LISTENING ..", "LISTENING ..."};
        canvas_draw_str_aligned(canvas, 64, 35, AlignCenter, AlignTop,
                                dots[m->tick % 3]);
    } else {
        // Decoded message
        char capcode_str[16];
        snprintf(capcode_str, sizeof(capcode_str), "[%07lu]",
                 (unsigned long)m->capcode);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 37, capcode_str);

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 82, 37, m->msg_type);

        // Word-wrap first 32 chars of text onto two lines
        char line1[33], line2[33];
        uint8_t tlen = (uint8_t)strlen(m->text);
        if(tlen <= 20) {
            strncpy(line1, m->text, 32);
            line2[0] = '\0';
        } else {
            strncpy(line1, m->text, 20);
            line1[20] = '\0';
            strncpy(line2, m->text + 20, 20);
            line2[20] = '\0';
        }
        canvas_draw_str(canvas, 2, 46, line1);
        if(line2[0]) canvas_draw_str(canvas, 2, 54, line2);
    }

    // ── RX count ─────────────────────────────────────────────────────────────
    char rx_str[16];
    snprintf(rx_str, sizeof(rx_str), "RX:%lu", (unsigned long)m->total_rx);

    canvas_draw_line(canvas, 0, 56, 127, 56);
    canvas_draw_str(canvas, 0, 63, "[^v]freq [<>]baud [OK]frz");
    canvas_draw_str(canvas, 96, 56, rx_str);
}

// ─────────────────────────────────────────────────────────────────────────────
// Input
// ─────────────────────────────────────────────────────────────────────────────

static bool scan_input(InputEvent* ev, void* ctx) {
    PdApp* app = ctx;
    if(ev->type != InputTypeShort) return false;

    switch(ev->key) {
    case InputKeyBack:
        view_dispatcher_send_custom_event(app->view_dispatcher, PdEventBack);
        return true;
    case InputKeyOk:
        // Freeze / resume
        with_view_model(app->scan_view, ScanViewModel* m, {
            m->state = (m->state == ScanStateFrozen) ? ScanStateListening : ScanStateFrozen;
        }, true);
        return true;
    case InputKeyUp:
        if(app->freq_idx < PD_FREQ_COUNT - 1) {
            app->freq_idx++;
            view_dispatcher_send_custom_event(app->view_dispatcher, PdEventFreqUp);
        }
        return true;
    case InputKeyDown:
        if(app->freq_idx > 0) {
            app->freq_idx--;
            view_dispatcher_send_custom_event(app->view_dispatcher, PdEventFreqDown);
        }
        return true;
    case InputKeyLeft:
    case InputKeyRight:
        app->baud = (PdBaud)((app->baud + 1) % PdBaudCount);
        view_dispatcher_send_custom_event(app->view_dispatcher, PdEventBaudCycle);
        return true;
    default:
        return false;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Scene lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void pd_scene_scan_on_enter(void* ctx) {
    PdApp* app = ctx;

    with_view_model(app->scan_view, ScanViewModel* m, {
        memset(m, 0, sizeof(ScanViewModel));
        m->state     = ScanStateListening;
        m->freq      = PD_FREQUENCIES[app->freq_idx];
        strncpy(m->freq_label, PD_FREQ_LABELS[app->freq_idx], 15);
        strncpy(m->baud_label, pd_baud_label(app->baud), 5);
    }, false);

    view_set_draw_callback(app->scan_view,  scan_draw);
    view_set_input_callback(app->scan_view, scan_input);
    view_set_context(app->scan_view, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, PdViewScan);

    pd_radio_start(app);

    s_scan_timer = furi_timer_alloc(scan_tick_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(s_scan_timer, 150);

    notification_message(app->notifications, &sequence_single_vibro);
}

bool pd_scene_scan_on_event(void* ctx, SceneManagerEvent ev) {
    PdApp* app = ctx;
    if(ev.type == SceneManagerEventTypeCustom) {
        if(ev.event == PdEventMsgReceived) {
            notification_message(app->notifications, &sequence_success);
            with_view_model(app->scan_view, ScanViewModel* m, {
                if(m->state != ScanStateFrozen) {
                    m->state    = ScanStateDecoded;
                    m->capcode  = app->last.capcode;
                    m->total_rx = app->total_rx;
                    strncpy(m->msg_type, pd_msg_type_label(app->last.msg_type), 5);
                    strncpy(m->text, app->last.text, PD_MSG_TEXT_LEN - 1);
                    m->has_msg = true;
                }
            }, true);
            return true;
        }
        if(ev.event == PdEventFreqUp || ev.event == PdEventFreqDown) {
            pd_radio_stop(app);
            with_view_model(app->scan_view, ScanViewModel* m, {
                m->freq = PD_FREQUENCIES[app->freq_idx];
                strncpy(m->freq_label, PD_FREQ_LABELS[app->freq_idx], 15);
                m->has_msg = false;
                m->state   = ScanStateListening;
            }, true);
            pd_radio_start(app);
            return true;
        }
        if(ev.event == PdEventBaudCycle) {
            with_view_model(app->scan_view, ScanViewModel* m, {
                strncpy(m->baud_label, pd_baud_label(app->baud), 5);
            }, true);
            return true;
        }
        if(ev.event == PdEventBack) {
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
    }
    if(ev.type == SceneManagerEventTypeBack) return false;
    return false;
}

void pd_scene_scan_on_exit(void* ctx) {
    PdApp* app = ctx;
    if(s_scan_timer) {
        furi_timer_stop(s_scan_timer);
        furi_timer_free(s_scan_timer);
        s_scan_timer = NULL;
    }
    pd_radio_stop(app);
}
