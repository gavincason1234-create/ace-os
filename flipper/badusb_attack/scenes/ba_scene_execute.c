// Execute scene — countdown → inject keystrokes → show result
// Payload runs in a FuriThread so the canvas stays live during execution.

#include "../badusb_attack.h"

// ExecuteViewModel is defined in badusb_attack.h (shared with payload fns)

static FuriTimer* count_timer = NULL;

// ─────────────────────────────────────────────────────────────────────────────
// Countdown timer — fires every 1 second
// ─────────────────────────────────────────────────────────────────────────────

static void count_timer_cb(void* ctx) {
    BaBadUsbApp* app = ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher, BaEventCountTick);
}

// ─────────────────────────────────────────────────────────────────────────────
// Payload worker thread — runs blocking HID sequence off the UI thread
// ─────────────────────────────────────────────────────────────────────────────

static int32_t exec_worker(void* ctx) {
    BaBadUsbApp* app = ctx;

    // Switch USB to HID keyboard mode
    app->saved_usb_cfg = furi_hal_usb_get_config();
    furi_hal_usb_unlock();
    furi_hal_usb_set(&usb_hid);
    furi_delay_ms(700);  // wait for host OS to enumerate

    // Run the selected payload
    BA_PAYLOADS[app->selected].execute_fn(app);

    app->thread_done = true;
    view_dispatcher_send_custom_event(app->view_dispatcher, BaEventExecDone);
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Draw
// ─────────────────────────────────────────────────────────────────────────────

static void exec_draw(Canvas* canvas, void* model) {
    ExecuteViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Header
    canvas_draw_rbox(canvas, 0, 0, 128, 11, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, "BADUSB ATTACK");
    // Risk badge top-right
    if(m->risk[0] == 'C')      canvas_draw_str(canvas, 80, 8, "[CRITICAL]");
    else if(m->risk[0] == 'H') canvas_draw_str(canvas, 90, 8, "[HIGH]");
    else                       canvas_draw_str(canvas, 92, 8, "[DEMO]");
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontSecondary);

    switch(m->substate) {

    case BaExecCountdown: {
        // Giant countdown number
        canvas_set_font(canvas, FontBigNumbers);
        char cnt[4];
        snprintf(cnt, sizeof(cnt), "%u", m->countdown);
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignTop, cnt);

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignTop, m->payload_name);
        if(!m->usb_ok)
            canvas_draw_str_aligned(canvas, 64, 57, AlignCenter, AlignTop, "! NO USB HOST !");
        else
            canvas_draw_str_aligned(canvas, 64, 57, AlignCenter, AlignTop, "[BACK] abort");
        break;
    }

    case BaExecRunning: {
        // Blinking pulse bar
        uint8_t pulse = (m->tick % 20) * 6;
        if(pulse > 124) pulse = 124;
        canvas_draw_box(canvas, 2, 12, pulse, 5);
        canvas_draw_str(canvas, 2, 25, "INJECTING KEYSTROKES");
        if(m->tick & 2) canvas_draw_str(canvas, 110, 25, "...");

        canvas_set_font(canvas, FontSecondary);
        for(uint8_t i = 0; i < m->log_count && i < 4; i++) {
            canvas_draw_str(canvas, 2, 33 + i * 8, m->log[i]);
        }
        break;
    }

    case BaExecDone: {
        // Inverted success bar
        canvas_draw_rbox(canvas, 0, 12, 128, 13, 2);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, 64, 15, AlignCenter, AlignTop,
            m->success ? "PAYLOAD DELIVERED" : "EXECUTION FAILED");
        canvas_set_color(canvas, ColorBlack);

        for(uint8_t i = 0; i < m->log_count && i < 4; i++) {
            canvas_draw_str(canvas, 2, 30 + i * 8, m->log[i]);
        }
        canvas_draw_str(canvas, 2, 62, "[BACK] return to menu");
        break;
    }

    case BaExecError: {
        canvas_draw_str(canvas, 2, 20, "ERROR:");
        canvas_draw_str(canvas, 2, 30, "USB not connected or");
        canvas_draw_str(canvas, 2, 39, "HID mode unavailable.");
        canvas_draw_str(canvas, 2, 52, "Plug in USB cable first.");
        canvas_draw_str(canvas, 2, 62, "[BACK] return");
        break;
    }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Input
// ─────────────────────────────────────────────────────────────────────────────

static bool exec_input(InputEvent* ev, void* ctx) {
    BaBadUsbApp* app = ctx;
    if(ev->type != InputTypeShort) return false;
    if(ev->key == InputKeyBack) {
        view_dispatcher_send_custom_event(app->view_dispatcher, BaEventBack);
        return true;
    }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Scene lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void ba_scene_execute_on_enter(void* ctx) {
    BaBadUsbApp* app = ctx;
    app->thread_done = false;

    bool usb_present = furi_hal_power_is_usb_connected();

    with_view_model(app->execute_view, ExecuteViewModel* m, {
        memset(m, 0, sizeof(ExecuteViewModel));
        m->substate = BaExecCountdown;
        m->countdown = 3;
        m->usb_ok    = usb_present;
        strncpy(m->payload_name, BA_PAYLOADS[app->selected].name, 23);
        strncpy(m->risk,         BA_PAYLOADS[app->selected].risk_level, 9);
        strncpy(m->target_os,    BA_PAYLOADS[app->selected].target_os, 11);
    }, false);

    view_set_draw_callback(app->execute_view, exec_draw);
    view_set_input_callback(app->execute_view, exec_input);
    view_set_context(app->execute_view, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, BaViewExecute);

    if(!usb_present) {
        with_view_model(app->execute_view, ExecuteViewModel* m, {
            m->substate = BaExecError;
        }, true);
        return;
    }

    // Start 1-second countdown timer
    count_timer = furi_timer_alloc(count_timer_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(count_timer, 1000);

    notification_message(app->notifications, &sequence_single_vibro);
}

bool ba_scene_execute_on_event(void* ctx, SceneManagerEvent ev) {
    BaBadUsbApp* app = ctx;

    if(ev.type == SceneManagerEventTypeCustom) {

        if(ev.event == BaEventBack) {
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }

        if(ev.event == BaEventCountTick) {
            bool launch = false;
            with_view_model(app->execute_view, ExecuteViewModel* m, {
                m->tick++;
                if(m->countdown > 0) {
                    m->countdown--;
                } else {
                    // Countdown hit 0 — stop timer, switch to running
                    m->substate = BaExecRunning;
                    launch = true;
                }
            }, true);

            if(launch) {
                furi_timer_stop(count_timer);
                furi_timer_free(count_timer);
                count_timer = NULL;

                // Spawn worker thread
                app->exec_thread = furi_thread_alloc_ex(
                    "badusb_worker", 2048, exec_worker, app);
                furi_thread_start(app->exec_thread);
            }
            return true;
        }

        if(ev.event == BaEventExecDone) {
            // Wait for thread to actually finish
            if(app->exec_thread) {
                furi_thread_join(app->exec_thread);
                furi_thread_free(app->exec_thread);
                app->exec_thread = NULL;
            }
            // Restore USB
            if(app->saved_usb_cfg) {
                furi_hal_usb_unlock();
                furi_hal_usb_set(app->saved_usb_cfg);
                app->saved_usb_cfg = NULL;
            }
            with_view_model(app->execute_view, ExecuteViewModel* m, {
                m->substate = BaExecDone;
                m->success  = app->thread_done;
            }, true);
            notification_message(app->notifications, &sequence_success);
            return true;
        }
    }

    if(ev.type == SceneManagerEventTypeBack) {
        return false;
    }
    return false;
}

void ba_scene_execute_on_exit(void* ctx) {
    BaBadUsbApp* app = ctx;

    // Kill countdown timer if still running
    if(count_timer) {
        furi_timer_stop(count_timer);
        furi_timer_free(count_timer);
        count_timer = NULL;
    }

    // If thread still running, abort it (best-effort)
    if(app->exec_thread) {
        furi_thread_join(app->exec_thread);
        furi_thread_free(app->exec_thread);
        app->exec_thread = NULL;
    }

    // Restore USB mode to what it was before
    if(app->saved_usb_cfg) {
        furi_hal_usb_unlock();
        furi_hal_usb_set(app->saved_usb_cfg);
        app->saved_usb_cfg = NULL;
    }
    furi_hal_hid_kb_release_all();
}
