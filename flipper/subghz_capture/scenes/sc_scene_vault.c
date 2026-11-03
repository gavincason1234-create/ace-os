// Sub-GHz Capture — vault browser + replay scene
//
// List of stored signals: label, freq, pulse count
// OK  → replay selected signal (blinking TX indicator)
// Left → back to capture scene
// Up/Down → navigate list

#include "../subghz_capture.h"

#define VAULT_ROWS    4    // visible rows
#define VAULT_ROW_H   11   // px per row

typedef struct {
    uint8_t  sel;
    uint8_t  scroll;
    uint8_t  count;
    uint8_t  tick;
    ScReplayState replay;
    // Cached display rows
    char     labels[SC_VAULT_MAX][20];
    uint32_t freqs[SC_VAULT_MAX];
    uint16_t pulses[SC_VAULT_MAX];
} VaultViewModel;

static void vault_draw(Canvas* canvas, void* model) {
    VaultViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Header
    canvas_draw_rbox(canvas, 0, 0, 128, 11, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, "SIGNAL VAULT");
    char cnt[8];
    snprintf(cnt, sizeof(cnt), "%u/%u", (unsigned)m->sel + 1, (unsigned)m->count);
    canvas_draw_str(canvas, 96, 8, cnt);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontSecondary);

    if(m->count == 0) {
        canvas_draw_str(canvas, 16, 36, "Vault is empty.");
        canvas_draw_str(canvas, 8, 52, "Capture a signal first.");
        return;
    }

    // Visible window
    uint8_t top = m->scroll;
    uint8_t end = (uint8_t)(top + VAULT_ROWS);
    if(end > m->count) end = m->count;

    for(uint8_t i = top; i < end; i++) {
        uint8_t ry  = (uint8_t)(12 + (i - top) * VAULT_ROW_H);
        bool    sel = (i == m->sel);

        if(sel) {
            canvas_draw_box(canvas, 0, ry, 128, (uint8_t)(VAULT_ROW_H - 1));
            canvas_set_color(canvas, ColorWhite);
        }

        // Label
        canvas_draw_str(canvas, 2, (uint8_t)(ry + 8), m->labels[i]);

        // Freq in MHz
        char fstr[10];
        snprintf(fstr, sizeof(fstr), "%luM",
                 (unsigned long)(m->freqs[i] / 1000000));
        canvas_draw_str(canvas, 80, (uint8_t)(ry + 8), fstr);

        // Pulse count
        char pstr[8];
        snprintf(pstr, sizeof(pstr), "%up", (unsigned)m->pulses[i]);
        canvas_draw_str(canvas, 106, (uint8_t)(ry + 8), pstr);

        if(sel) canvas_set_color(canvas, ColorBlack);
    }

    // Replay status bar at bottom
    if(m->replay == ScReplayStateActive) {
        canvas_draw_box(canvas, 0, 56, 128, 8);
        canvas_set_color(canvas, ColorWhite);
        if(m->tick & 1)
            canvas_draw_str(canvas, 14, 63, ">>> TRANSMITTING RF <<<");
        canvas_set_color(canvas, ColorBlack);
    } else if(m->replay == ScReplayStateDone) {
        canvas_draw_str(canvas, 18, 63, "TX DONE  OK:replay");
    } else {
        canvas_draw_str(canvas, 4, 63, "OK:replay  Left:capture");
    }
}

static bool vault_input(InputEvent* ev, void* ctx) {
    ScApp* app = ctx;
    if(ev->type != InputTypeShort) return false;
    switch(ev->key) {
    case InputKeyUp:
        with_view_model(app->vault_view, VaultViewModel* m, {
            if(m->sel > 0) {
                m->sel--;
                if(m->sel < m->scroll) m->scroll = m->sel;
                app->vault_sel = m->sel;
            }
        }, true);
        return true;
    case InputKeyDown:
        with_view_model(app->vault_view, VaultViewModel* m, {
            if(m->sel + 1 < m->count) {
                m->sel++;
                if(m->sel >= m->scroll + VAULT_ROWS)
                    m->scroll = (uint8_t)(m->sel - VAULT_ROWS + 1);
                app->vault_sel = m->sel;
            }
        }, true);
        return true;
    case InputKeyOk:
        if(app->vault_count > 0) {
            if(app->replay_state == ScReplayStateActive) {
                sc_stop_tx(app);
            } else {
                sc_start_tx(app, app->vault_sel);
                notification_message(app->notifications, &sequence_single_vibro);
            }
            with_view_model(app->vault_view, VaultViewModel* m, {
                m->replay = app->replay_state;
            }, true);
        }
        return true;
    case InputKeyLeft:
    case InputKeyBack:
        view_dispatcher_send_custom_event(app->view_dispatcher, ScEventBack);
        return true;
    default:
        return false;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 200ms timer — poll replay completion
// ─────────────────────────────────────────────────────────────────────────────

static FuriTimer* s_vault_timer = NULL;

static void vault_tick_cb(void* ctx) {
    ScApp* app = ctx;

    // Check TX completion
    if(app->replay_state == ScReplayStateActive) {
        // sc_tx_callback sets replay_state=Done when buffer exhausted
        if(app->tx_pos >= app->vault[app->vault_sel].count) {
            sc_stop_tx(app);
        }
    }

    with_view_model(app->vault_view, VaultViewModel* m, {
        m->tick++;
        m->replay = app->replay_state;
        m->count  = app->vault_count;
    }, true);
}

void sc_scene_vault_on_enter(void* ctx) {
    ScApp* app = ctx;

    view_set_draw_callback(app->vault_view,  vault_draw);
    view_set_input_callback(app->vault_view, vault_input);
    view_set_context(app->vault_view, app);

    // Clamp selection
    if(app->vault_count > 0 && app->vault_sel >= app->vault_count)
        app->vault_sel = (uint8_t)(app->vault_count - 1);

    with_view_model(app->vault_view, VaultViewModel* m, {
        m->sel    = app->vault_sel;
        m->scroll = 0;
        m->count  = app->vault_count;
        m->tick   = 0;
        m->replay = ScReplayStateIdle;
        for(uint8_t i = 0; i < app->vault_count; i++) {
            strncpy(m->labels[i], app->vault[i].label, 19);
            m->labels[i][19] = '\0';
            m->freqs[i]  = app->vault[i].freq_hz;
            m->pulses[i] = app->vault[i].count;
        }
        // Initial scroll so selection is visible
        if(m->sel >= VAULT_ROWS)
            m->scroll = (uint8_t)(m->sel - VAULT_ROWS + 1);
    }, true);

    view_dispatcher_switch_to_view(app->view_dispatcher, ScViewVault);

    s_vault_timer = furi_timer_alloc(vault_tick_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(s_vault_timer, 200);
}

bool sc_scene_vault_on_event(void* ctx, SceneManagerEvent ev) {
    ScApp* app = ctx;
    if(ev.type == SceneManagerEventTypeCustom) {
        if(ev.event == ScEventBack) {
            sc_stop_tx(app);
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
    }
    if(ev.type == SceneManagerEventTypeBack) return false;
    return false;
}

void sc_scene_vault_on_exit(void* ctx) {
    ScApp* app = ctx;
    if(s_vault_timer) {
        furi_timer_stop(s_vault_timer);
        furi_timer_free(s_vault_timer);
        s_vault_timer = NULL;
    }
    sc_stop_tx(app);
}
