// UART Debug Sniffer — sniff scene
// Live scrolling terminal showing UART RX data.
// Lines containing credential keywords ("password", "login", "root", "$", …)
// are shown inverted with a CREDENTIAL ALERT flash.
// Up/Down = baud rate change on the fly
// OK = toggle ASCII ↔ HEX display mode
// Right = clear terminal
// Back = return to baud selector

#include "../uart_sniffer.h"

// ─────────────────────────────────────────────────────────────────────────────
// View model — snapshot of terminal lines for rendering
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    // Last US_LINES lines (most recent at bottom)
    char    lines[US_LINES][US_LINE_BUF];
    bool    alert[US_LINES];   // line is a credential hit
    uint8_t line_count;        // how many valid lines to show
    // Stats
    uint32_t total_bytes;
    uint32_t alert_count;
    uint32_t baud;
    UsDisplayMode display_mode;
    uint8_t  tick;
    bool     active;
} SniffViewModel;

static void sniff_sync(UsApp* app) {
    with_view_model(app->sniff_view, SniffViewModel* m, {
        m->total_bytes  = app->total_bytes;
        m->alert_count  = app->alert_count;
        m->baud         = US_BAUDS[app->baud_idx];
        m->display_mode = app->display_mode;
        m->active       = app->uart_active;

        // Populate last US_LINES from the ring buffer
        uint8_t avail = (uint8_t)(app->line_count < US_LINES ? app->line_count : US_LINES);
        m->line_count  = avail;
        uint16_t start = (uint16_t)(app->line_head >= avail
                                    ? app->line_head - avail
                                    : US_TOTAL_LINES + app->line_head - avail);
        for(uint8_t i = 0; i < avail; i++) {
            const UsLine* src = &app->lines[(start + i) % US_TOTAL_LINES];
            strncpy(m->lines[i], src->text, US_COLS);
            m->lines[i][US_COLS] = '\0';
            m->alert[i] = src->alert;
        }
    }, true);
}

static void sniff_draw(Canvas* canvas, void* model) {
    SniffViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Header — shows baud, mode, byte count
    canvas_draw_rbox(canvas, 0, 0, 128, 11, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    char hdr[28];
    snprintf(hdr, sizeof(hdr), "%lu %s",
             (unsigned long)m->baud,
             m->display_mode == UsDisplayAscii ? "ASC" : "HEX");
    canvas_draw_str(canvas, 2, 8, hdr);
    char bcnt[12];
    snprintf(bcnt, sizeof(bcnt), "%luB", (unsigned long)m->total_bytes);
    canvas_draw_str(canvas, 96, 8, bcnt);
    canvas_set_color(canvas, ColorBlack);

    // Terminal lines — 4 rows × 10px each
    canvas_set_font(canvas, FontSecondary);
    for(uint8_t i = 0; i < US_LINES; i++) {
        uint8_t row_y = (uint8_t)(13 + i * 11);

        if(i >= m->line_count) {
            // Empty line placeholder
            canvas_draw_str(canvas, 0, (uint8_t)(row_y + 8), "~");
            continue;
        }

        bool is_alert = m->alert[i];
        if(is_alert) {
            // Inverted highlight for credential line
            canvas_draw_box(canvas, 0, row_y, 128, 10);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, 0, (uint8_t)(row_y + 8), m->lines[i]);
        if(is_alert) canvas_set_color(canvas, ColorBlack);
    }

    // Status / alert bar
    canvas_draw_line(canvas, 0, 57, 127, 57);
    if(m->alert_count > 0 && (m->tick & 1)) {
        // Blinking CREDENTIAL ALERT when keywords found
        canvas_draw_box(canvas, 0, 58, 128, 6);
        canvas_set_color(canvas, ColorWhite);
        char alert_str[28];
        snprintf(alert_str, sizeof(alert_str), "!! CREDENTIAL HIT x%lu !!",
                 (unsigned long)m->alert_count);
        canvas_draw_str(canvas, 2, 63, alert_str);
        canvas_set_color(canvas, ColorBlack);
    } else {
        canvas_draw_str(canvas, 0, 63,
            m->display_mode == UsDisplayAscii
                ? "[^v]baud [OK]hex [>]clr"
                : "[^v]baud [OK]asc [>]clr");
    }

    // Blinking cursor on last partial line (bottom)
    if(m->tick & 1)
        canvas_draw_box(canvas, 125, 46, 2, 8);
}

static bool sniff_input(InputEvent* ev, void* ctx) {
    UsApp* app = ctx;
    if(ev->type != InputTypeShort) return false;
    switch(ev->key) {
    case InputKeyBack:
        view_dispatcher_send_custom_event(app->view_dispatcher, UsEventBack);
        return true;
    case InputKeyUp:
        // Increase baud rate
        if(app->baud_idx < US_BAUD_COUNT - 1) {
            us_uart_stop(app);
            app->baud_idx++;
            us_uart_start(app);
            us_clear(app);
            sniff_sync(app);
        }
        return true;
    case InputKeyDown:
        // Decrease baud rate
        if(app->baud_idx > 0) {
            us_uart_stop(app);
            app->baud_idx--;
            us_uart_start(app);
            us_clear(app);
            sniff_sync(app);
        }
        return true;
    case InputKeyOk:
        // Toggle ASCII ↔ HEX
        view_dispatcher_send_custom_event(app->view_dispatcher, UsEventToggleMode);
        return true;
    case InputKeyRight:
        view_dispatcher_send_custom_event(app->view_dispatcher, UsEventClear);
        return true;
    default:
        return false;
    }
}

// Tick timer — drain byte buffer and refresh display
static FuriTimer* s_sniff_timer = NULL;

static void sniff_tick_cb(void* ctx) {
    UsApp* app = ctx;

    // Always drain remaining bytes each tick (handles low-traffic streams)
    us_process_bytes(app);
    sniff_sync(app);
    with_view_model(app->sniff_view, SniffViewModel* m, { m->tick++; }, true);

    // Flash notification on first credential hit
    static uint32_t last_alert = 0;
    if(app->alert_count > last_alert) {
        last_alert = app->alert_count;
        notification_message(app->notifications, &sequence_double_vibro);
    }
}

void us_scene_sniff_on_enter(void* ctx) {
    UsApp* app = ctx;

    view_set_draw_callback(app->sniff_view,  sniff_draw);
    view_set_input_callback(app->sniff_view, sniff_input);
    view_set_context(app->sniff_view, app);

    us_clear(app);
    sniff_sync(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, UsViewSniff);

    us_uart_start(app);

    s_sniff_timer = furi_timer_alloc(sniff_tick_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(s_sniff_timer, 100);  // 100ms tick — fast enough for 115200
}

bool us_scene_sniff_on_event(void* ctx, SceneManagerEvent ev) {
    UsApp* app = ctx;
    if(ev.type == SceneManagerEventTypeCustom) {
        if(ev.event == UsEventDataReady) {
            us_process_bytes(app);
            sniff_sync(app);
            return true;
        }
        if(ev.event == UsEventToggleMode) {
            app->display_mode = (app->display_mode == UsDisplayAscii)
                                ? UsDisplayHex : UsDisplayAscii;
            us_clear(app);
            sniff_sync(app);
            return true;
        }
        if(ev.event == UsEventClear) {
            us_clear(app);
            sniff_sync(app);
            return true;
        }
        if(ev.event == UsEventBack) {
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
    }
    if(ev.type == SceneManagerEventTypeBack) return false;
    return false;
}

void us_scene_sniff_on_exit(void* ctx) {
    UsApp* app = ctx;
    if(s_sniff_timer) {
        furi_timer_stop(s_sniff_timer);
        furi_timer_free(s_sniff_timer);
        s_sniff_timer = NULL;
    }
    us_uart_stop(app);
}
