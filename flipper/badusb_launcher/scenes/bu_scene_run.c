// BadUSB Launcher — run scene
//
// States:
//   Idle        → waiting for OK to start
//   Countdown   → 3-2-1 with 1s ticks, big digits, Back aborts
//   Injecting   → spinner + progress, USB icon blinks
//   Done        → result summary + bytes typed
//   Error       → error message
//   NoUsb       → "USB NOT CONNECTED" guidance
//
// Controls:
//   OK          → start countdown (from Idle) or run again (from Done/Error)
//   Back        → abort countdown / return to menu

#include "../badusb_launcher.h"

// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    BuRunState state;
    uint8_t    countdown;       // 3, 2, 1, 0
    uint8_t    tick;
    char       label[20];
    char       detail[48];
    uint32_t   bytes_typed;
} RunViewModel;

// ─────────────────────────────────────────────────────────────────────────────
// Draw USB plug icon at (x, y)
// ─────────────────────────────────────────────────────────────────────────────

static void draw_usb_icon(Canvas* canvas, uint8_t x, uint8_t y, bool active) {
    // Plug body
    canvas_draw_box(canvas, x, y + 4, 8, 12);
    // Prongs
    canvas_draw_box(canvas, (uint8_t)(x + 2), y, 1, 4);
    canvas_draw_box(canvas, (uint8_t)(x + 5), y, 1, 4);
    // Cable
    canvas_draw_line(canvas, (uint8_t)(x + 4), (uint8_t)(y + 16),
                     (uint8_t)(x + 4), (uint8_t)(y + 20));
    if(active) {
        // Lightning bolt overlay
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, (uint8_t)(x + 1), (uint8_t)(y + 14), "!");
        canvas_set_color(canvas, ColorBlack);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Draw callback
// ─────────────────────────────────────────────────────────────────────────────

static void run_draw(Canvas* canvas, void* model) {
    RunViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Header
    canvas_draw_rbox(canvas, 0, 0, 128, 11, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, "BADUSB");
    canvas_draw_str(canvas, 36, 8, m->label);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontSecondary);

    switch(m->state) {
    case BuRunStateIdle: {
        canvas_draw_str(canvas, 10, 28, "Connect Flipper to");
        canvas_draw_str(canvas, 10, 40, "target USB port.");
        canvas_draw_str(canvas, 10, 52, m->detail);
        // OK prompt
        canvas_draw_rbox(canvas, 32, 55, 64, 9, 2);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, 42, 62, "OK: LAUNCH");
        canvas_set_color(canvas, ColorBlack);
        // USB icon
        draw_usb_icon(canvas, 108, 16, false);
        break;
    }

    case BuRunStateCountdown: {
        // Big countdown digit centered
        canvas_set_font(canvas, FontBigNumbers);
        char cd[2] = {(char)('0' + m->countdown), '\0'};
        canvas_draw_str(canvas, 56, 48, cd);

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 18, 20, ">>> LAUNCHING IN <<<");
        canvas_draw_str(canvas, 20, 62, "Back: abort");

        // Blinking USB icon
        if(m->tick & 1)
            draw_usb_icon(canvas, 108, 16, true);
        break;
    }

    case BuRunStateInjecting: {
        // Spinner
        static const char* spin = "|/-\\";
        char sp[2] = {spin[m->tick % 4], '\0'};
        canvas_draw_str(canvas, 2, 30, sp);
        canvas_draw_str(canvas, 14, 30, "INJECTING...");
        draw_usb_icon(canvas, 108, 16, (m->tick & 1));
        canvas_draw_str(canvas, 4, 48, "DO NOT REMOVE USB");
        break;
    }

    case BuRunStateDone: {
        canvas_draw_rbox(canvas, 0, 12, 128, 14, 3);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, 14, 22, "[OK]  INJECTION DONE");
        canvas_set_color(canvas, ColorBlack);

        char info[32];
        snprintf(info, sizeof(info), "Keystrokes: %lu",
                 (unsigned long)m->bytes_typed);
        canvas_draw_str(canvas, 4, 38, info);
        canvas_draw_str(canvas, 4, 50, m->label);
        canvas_draw_str(canvas, 4, 62, "OK: run again   Back: menu");
        break;
    }

    case BuRunStateError: {
        canvas_draw_rbox(canvas, 0, 12, 128, 14, 3);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, 14, 22, "[ERR]  INJECT FAILED");
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_str(canvas, 4, 40, "HID error — check USB");
        canvas_draw_str(canvas, 4, 62, "OK: retry   Back: menu");
        break;
    }

    case BuRunStateNoUsb: {
        canvas_draw_rbox(canvas, 0, 12, 128, 14, 3);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, 6, 22, "USB NOT CONNECTED");
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_str(canvas, 4, 36, "Plug Flipper into");
        canvas_draw_str(canvas, 4, 48, "target machine first.");
        canvas_draw_str(canvas, 4, 62, "Back: menu");
        break;
    }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Input callback
// ─────────────────────────────────────────────────────────────────────────────

static bool run_input(InputEvent* ev, void* ctx) {
    BuApp* app = ctx;
    if(ev->type != InputTypeShort) return false;

    switch(ev->key) {
    case InputKeyBack:
        view_dispatcher_send_custom_event(app->view_dispatcher, BuEventBack);
        return true;
    case InputKeyOk:
        if(app->run_state == BuRunStateIdle ||
           app->run_state == BuRunStateDone ||
           app->run_state == BuRunStateError) {
            view_dispatcher_send_custom_event(
                app->view_dispatcher, BuEventStartCountdown);
        }
        return true;
    default:
        return false;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 1-second countdown timer
// ─────────────────────────────────────────────────────────────────────────────

static FuriTimer* s_run_timer = NULL;

static void run_tick_cb(void* ctx) {
    BuApp* app = ctx;

    with_view_model(app->run_view, RunViewModel* m, {
        m->tick++;

        if(app->run_state == BuRunStateCountdown) {
            if(m->countdown > 0) {
                m->countdown--;
                app->countdown = m->countdown;
            }
            if(m->countdown == 0) {
                // Fire injection
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, BuEventCountdownTick);
            }
        } else if(app->run_state == BuRunStateInjecting ||
                  app->run_state == BuRunStateDone) {
            m->bytes_typed = app->bytes_typed;
            m->state       = app->run_state;
        }
    }, true);
}

// ─────────────────────────────────────────────────────────────────────────────
// Scene lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void bu_scene_run_on_enter(void* ctx) {
    BuApp* app = ctx;

    view_set_draw_callback(app->run_view,  run_draw);
    view_set_input_callback(app->run_view, run_input);
    view_set_context(app->run_view, app);

    const BuPreset* preset = &BU_PRESETS[app->payload_type];
    app->run_state  = BuRunStateIdle;
    app->countdown  = 3;
    app->bytes_typed = 0;

    with_view_model(app->run_view, RunViewModel* m, {
        m->state     = BuRunStateIdle;
        m->countdown = 3;
        m->tick      = 0;
        m->bytes_typed = 0;
        strncpy(m->label,  preset->label,  sizeof(m->label)  - 1);
        strncpy(m->detail, preset->detail, sizeof(m->detail) - 1);
        m->label[sizeof(m->label)   - 1] = '\0';
        m->detail[sizeof(m->detail) - 1] = '\0';
    }, true);

    view_dispatcher_switch_to_view(app->view_dispatcher, BuViewRun);

    s_run_timer = furi_timer_alloc(run_tick_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(s_run_timer, 1000);   // 1 tick/sec for countdown
}

bool bu_scene_run_on_event(void* ctx, SceneManagerEvent ev) {
    BuApp* app = ctx;

    if(ev.type == SceneManagerEventTypeCustom) {

        if(ev.event == BuEventStartCountdown) {
            app->run_state = BuRunStateCountdown;
            app->countdown = 3;
            with_view_model(app->run_view, RunViewModel* m, {
                m->state     = BuRunStateCountdown;
                m->countdown = 3;
                m->tick      = 0;
            }, true);
            notification_message(app->notifications, &sequence_single_vibro);
            return true;
        }

        if(ev.event == BuEventCountdownTick) {
            // Countdown reached 0 — start injection
            app->run_state = BuRunStateInjecting;
            with_view_model(app->run_view, RunViewModel* m, {
                m->state = BuRunStateInjecting;
            }, true);
            bu_inject_start(app);
            return true;
        }

        if(ev.event == BuEventInjectDone) {
            app->run_state = BuRunStateDone;
            with_view_model(app->run_view, RunViewModel* m, {
                m->state       = BuRunStateDone;
                m->bytes_typed = app->bytes_typed;
            }, true);
            bu_inject_stop(app);
            notification_message(app->notifications, &sequence_double_vibro);
            notification_message(app->notifications, &sequence_success);
            return true;
        }

        if(ev.event == BuEventInjectError) {
            app->run_state = BuRunStateError;
            with_view_model(app->run_view, RunViewModel* m, {
                m->state = BuRunStateError;
            }, true);
            bu_inject_stop(app);
            notification_message(app->notifications, &sequence_error);
            return true;
        }

        if(ev.event == BuEventNoUsb) {
            app->run_state = BuRunStateNoUsb;
            with_view_model(app->run_view, RunViewModel* m, {
                m->state = BuRunStateNoUsb;
            }, true);
            bu_inject_stop(app);
            return true;
        }

        if(ev.event == BuEventBack) {
            // Abort if mid-countdown — injection not yet started, safe to abort
            if(app->run_state == BuRunStateCountdown ||
               app->run_state == BuRunStateIdle     ||
               app->run_state == BuRunStateDone     ||
               app->run_state == BuRunStateError    ||
               app->run_state == BuRunStateNoUsb) {
                scene_manager_previous_scene(app->scene_manager);
            }
            return true;
        }
    }

    if(ev.type == SceneManagerEventTypeBack) return false;
    return false;
}

void bu_scene_run_on_exit(void* ctx) {
    BuApp* app = ctx;
    if(s_run_timer) {
        furi_timer_stop(s_run_timer);
        furi_timer_free(s_run_timer);
        s_run_timer = NULL;
    }
    bu_inject_stop(app);
}
