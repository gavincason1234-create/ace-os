// Live kill scene — blasts IR power codes with animated blast-wave UI

#include "../tv_killswitch.h"

// ─────────────────────────────────────────────────────────────────────────────
// View model
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    uint8_t  tick;
    bool     paused;
    uint32_t codes_sent;
    uint32_t range_total;
    uint8_t  ring_r;       // current blast ring radius 0-30
    char     brand[24];
    char     proto[12];
    char     addr_cmd[24];
    char     speed_name[10];
    char     category[20];
} KillViewModel;

// ─────────────────────────────────────────────────────────────────────────────
// External: TV code table (defined in tv_killswitch.c)
// ─────────────────────────────────────────────────────────────────────────────

extern const TvPowerCode TV_CODES[];
extern const uint32_t    TV_CODE_COUNT;

// Category ranges [first, last] into TV_CODES[]
// Order must match TvkCategory enum
extern const uint32_t TVK_CAT_FIRST[];
extern uint32_t TVK_CAT_LAST[];

// ─────────────────────────────────────────────────────────────────────────────
// Timer: fires every tvk_speed_ms(speed), sends one IR code
// ─────────────────────────────────────────────────────────────────────────────

static void kill_timer_cb(void* ctx) {
    TvKillApp* app = ctx;
    if(app->paused) return;

    // Pick current code
    const TvPowerCode* c = &TV_CODES[app->code_idx];

    // Transmit — infrared_send is synchronous, completes before return
    InfraredMessage msg = {
        .protocol = c->protocol,
        .address  = c->address,
        .command  = c->command,
        .repeat   = false,
    };
    infrared_send(&msg, 1);

    app->codes_sent++;

    // Advance index within category range (wrap)
    app->code_idx++;
    if(app->code_idx > app->range_last)
        app->code_idx = app->range_first;

    // Update view model
    const char* proto_name = infrared_get_protocol_name(c->protocol);

    with_view_model(app->kill_view, KillViewModel* m, {
        m->tick++;
        m->codes_sent  = app->codes_sent;
        m->range_total = app->range_last - app->range_first + 1;
        strncpy(m->brand, c->brand, 23);
        strncpy(m->proto, proto_name ? proto_name : "?", 11);
        snprintf(m->addr_cmd, sizeof(m->addr_cmd),
                 "%04lX:%04lX",
                 (unsigned long)c->address, (unsigned long)c->command);
        // Grow blast ring
        m->ring_r = (m->ring_r + 4) % 32;
    }, true);

    // Flash red LED on transmit
    notification_message_save_state(app->notifications);
}

// ─────────────────────────────────────────────────────────────────────────────
// Draw — hacker terminal with expanding blast rings
// ─────────────────────────────────────────────────────────────────────────────

static void kill_draw(Canvas* canvas, void* model) {
    KillViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // ── Header bar ───────────────────────────────────────────────────────────
    canvas_draw_rbox(canvas, 0, 0, 128, 11, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, "TV KILL SWITCH");
    if(!m->paused && (m->tick & 3))
        canvas_draw_str(canvas, 91, 8, "[ZAP]");
    else if(m->paused)
        canvas_draw_str(canvas, 88, 8, "[PAUSE]");
    canvas_set_color(canvas, ColorBlack);

    // ── Left panel: brand + details ──────────────────────────────────────────
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 22, m->brand);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 31, m->proto);
    canvas_draw_str(canvas, 2, 39, m->addr_cmd);

    // Progress bar
    char prog[20];
    snprintf(prog, sizeof(prog), "%lu/%lu",
             (unsigned long)(m->codes_sent % m->range_total + 1),
             (unsigned long)m->range_total);
    canvas_draw_str(canvas, 2, 48, prog);

    uint32_t bar_w = (m->range_total > 0)
        ? (uint32_t)(60UL * (m->codes_sent % m->range_total) / m->range_total)
        : 0;
    canvas_draw_frame(canvas, 2, 50, 62, 5);
    if(bar_w > 0) canvas_draw_box(canvas, 2, 50, bar_w, 5);

    // Total sent
    char total[20];
    snprintf(total, sizeof(total), "TOTAL:%lu", (unsigned long)m->codes_sent);
    canvas_draw_str(canvas, 2, 59, total);

    // ── Right panel: blast-wave rings ────────────────────────────────────────
    // Center at (104, 38)
    const uint8_t cx = 104, cy = 38;

    // Outer ring — solid when paused, animated when firing
    if(!m->paused) {
        uint8_t r1 = m->ring_r;
        uint8_t r2 = (m->ring_r + 12) % 32;
        uint8_t r3 = (m->ring_r + 22) % 32;
        if(r1 > 2) canvas_draw_circle(canvas, cx, cy, r1);
        if(r2 > 2) canvas_draw_circle(canvas, cx, cy, r2);
        if(r3 > 2) canvas_draw_circle(canvas, cx, cy, r3);
    } else {
        canvas_draw_circle(canvas, cx, cy, 14);
        canvas_draw_circle(canvas, cx, cy, 8);
    }
    // Center dot
    canvas_draw_box(canvas, cx - 2, cy - 2, 4, 4);

    // Speed label under rings
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, cx, 58, AlignCenter, AlignTop, m->speed_name);

    // ── Bottom hint ───────────────────────────────────────────────────────────
    canvas_draw_line(canvas, 0, 60, 127, 60);
    canvas_draw_str(canvas, 0, 64, "[^v]spd [OK]pause [<]stop");
}

// ─────────────────────────────────────────────────────────────────────────────
// Input
// ─────────────────────────────────────────────────────────────────────────────

static bool kill_input(InputEvent* ev, void* ctx) {
    TvKillApp* app = ctx;
    if(ev->type != InputTypeShort) return false;

    if(ev->key == InputKeyBack) {
        view_dispatcher_send_custom_event(app->view_dispatcher, TvkEventBack);
        return true;
    }
    if(ev->key == InputKeyOk) {
        app->paused = !app->paused;
        with_view_model(app->kill_view, KillViewModel* m, {
            m->paused = app->paused;
        }, true);
        return true;
    }
    if(ev->key == InputKeyUp && app->speed < TvkSpeedBlast) {
        app->speed = (TvkSpeed)(app->speed + 1);
        furi_timer_stop(app->timer);
        furi_timer_start(app->timer, tvk_speed_ms(app->speed));
        with_view_model(app->kill_view, KillViewModel* m, {
            strncpy(m->speed_name, tvk_speed_name(app->speed), 9);
        }, true);
        return true;
    }
    if(ev->key == InputKeyDown && app->speed > TvkSpeedSlow) {
        app->speed = (TvkSpeed)(app->speed - 1);
        furi_timer_stop(app->timer);
        furi_timer_start(app->timer, tvk_speed_ms(app->speed));
        with_view_model(app->kill_view, KillViewModel* m, {
            strncpy(m->speed_name, tvk_speed_name(app->speed), 9);
        }, true);
        return true;
    }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Scene lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void tvk_scene_kill_on_enter(void* ctx) {
    TvKillApp* app = ctx;

    // Resolve category range
    app->range_first = TVK_CAT_FIRST[app->category];
    app->range_last  = TVK_CAT_LAST[app->category];
    app->code_idx    = app->range_first;
    app->codes_sent  = 0;
    app->paused      = false;

    with_view_model(app->kill_view, KillViewModel* m, {
        memset(m, 0, sizeof(KillViewModel));
        m->range_total = app->range_last - app->range_first + 1;
        strncpy(m->speed_name,  tvk_speed_name(app->speed), 9);
        strncpy(m->category,    tvk_category_name(app->category), 19);
        strncpy(m->brand,       TV_CODES[app->range_first].brand, 23);
        strncpy(m->proto,       "...", 11);
        strncpy(m->addr_cmd,    "...", 23);
    }, false);

    view_set_draw_callback(app->kill_view, kill_draw);
    view_set_input_callback(app->kill_view, kill_input);
    view_set_context(app->kill_view, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, TvkViewKill);

    app->timer = furi_timer_alloc(kill_timer_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(app->timer, tvk_speed_ms(app->speed));

    notification_message(app->notifications, &sequence_single_vibro);
}

bool tvk_scene_kill_on_event(void* ctx, SceneManagerEvent ev) {
    TvKillApp* app = ctx;
    if(ev.type == SceneManagerEventTypeCustom && ev.event == TvkEventBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    if(ev.type == SceneManagerEventTypeBack) return false;
    return false;
}

void tvk_scene_kill_on_exit(void* ctx) {
    TvKillApp* app = ctx;
    if(app->timer) {
        furi_timer_stop(app->timer);
        furi_timer_free(app->timer);
        app->timer = NULL;
    }
}
