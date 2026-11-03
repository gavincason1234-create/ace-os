// IR Remote Cloner — learn scene
// Point any IR remote at the Flipper's IR receiver.
// Shows animated receiver beam, captures raw signal, displays decoded overlay
// (protocol / address / command) when NEC/Samsung/etc. is recognized.
// OK = save to vault, Right = browse vault, Back = exit.

#include "../ir_cloner.h"

typedef enum {
    LearnStateWaiting = 0,
    LearnStateReceiving,
    LearnStateDone,
} LearnState;

typedef struct {
    LearnState state;
    uint32_t   timing_count;
    uint32_t   duration_ms;    // approx signal length
    bool       has_decoded;
    char       protocol[12];
    uint32_t   address;
    uint32_t   command;
    uint8_t    tick;
    uint8_t    beam_phase;     // animation counter
} LearnViewModel;

static void learn_sync(IrcApp* app, LearnState state) {
    with_view_model(app->learn_view, LearnViewModel* m, {
        m->state         = state;
        m->timing_count  = app->raw_count;
        // Each timing is ~100us on average → rough ms estimate
        uint32_t total_us = 0;
        for(uint32_t i = 0; i < app->raw_count; i++)
            total_us += app->raw_timings[i];
        m->duration_ms   = total_us / 1000;
        m->has_decoded   = app->last_decoded;
        if(app->last_decoded) {
            strncpy(m->protocol, app->last_protocol, 11);
            m->address = app->last_address;
            m->command = app->last_command;
        }
    }, true);
}

// Draw animated IR beam — radiating arcs from bottom-center
static void draw_ir_beam(Canvas* canvas, uint8_t phase) {
    uint8_t cx = 64;
    uint8_t cy = 48;

    // Central IR LED dot
    canvas_draw_box(canvas, cx - 2, cy - 2, 5, 5);

    // Concentric arcs, fading with phase offset
    for(uint8_t r = 0; r < 4; r++) {
        uint8_t rad = (uint8_t)(6 + r * 7);
        uint8_t age = (uint8_t)((phase + r * 2) % 8);
        if(age < 6) {  // visible for 6 of 8 ticks
            // Draw partial arc (top semicircle)
            for(int16_t a = -60; a <= 60; a += 5) {
                // Map angle to x,y using integer approximation
                // Simple scan: draw box/dot on upper arc
                int16_t ax = (int16_t)(cx + (rad * a) / 90);
                int16_t ay = (int16_t)(cy - (int16_t)rad + (rad * (a < 0 ? -a : a)) / 180);
                if(ax >= 0 && ax < 128 && ay >= 0 && ay < 64)
                    canvas_draw_dot(canvas, (uint8_t)ax, (uint8_t)ay);
            }
        }
    }
}

static void learn_draw(Canvas* canvas, void* model) {
    LearnViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Header
    canvas_draw_rbox(canvas, 0, 0, 128, 11, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, "IR REMOTE CLONER");
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontSecondary);

    if(m->state == LearnStateWaiting) {
        // Animated beam pointing up from Flipper IR sensor
        draw_ir_beam(canvas, m->beam_phase);

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignTop, "Point remote HERE");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignTop,
                                "then press button");

    } else if(m->state == LearnStateReceiving) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 16, AlignCenter, AlignTop, "RECEIVING...");
        canvas_set_font(canvas, FontSecondary);
        const char* dots[] = {"[  ]", "[- ]", "[--]", "[---]"};
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignTop,
                                dots[m->tick % 4]);

    } else {  // LearnStateDone
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignTop, "CAPTURED!");
        canvas_set_font(canvas, FontSecondary);

        char info[28];
        snprintf(info, sizeof(info), "%lu timings  ~%lums",
                 (unsigned long)m->timing_count, (unsigned long)m->duration_ms);
        canvas_draw_str(canvas, 2, 30, info);

        if(m->has_decoded) {
            char dec[32];
            snprintf(dec, sizeof(dec), "PROTO: %s", m->protocol);
            canvas_draw_str(canvas, 2, 40, dec);
            snprintf(dec, sizeof(dec), "ADDR: %04lX  CMD: %04lX",
                     (unsigned long)m->address, (unsigned long)m->command);
            canvas_draw_str(canvas, 2, 49, dec);
        } else {
            canvas_draw_str(canvas, 2, 40, "Protocol: RAW (unknown)");
            canvas_draw_str(canvas, 2, 49, "Zero authentication!");
        }

        // Footer
        canvas_draw_line(canvas, 0, 56, 127, 56);
        canvas_draw_str(canvas, 0, 63, "[OK]save [>]vault [<]redo");
    }
}

static bool learn_input(InputEvent* ev, void* ctx) {
    IrcApp* app = ctx;
    if(ev->type != InputTypeShort) return false;

    switch(ev->key) {
    case InputKeyBack:
        view_dispatcher_send_custom_event(app->view_dispatcher, IrcEventBack);
        return true;
    case InputKeyOk:
        // Save current capture to vault
        if(app->raw_count > 0) {
            irc_save_current(app);
            notification_message(app->notifications, &sequence_success);
            // Reset for next capture
            app->raw_count = 0;
            learn_sync(app, LearnStateWaiting);
        }
        return true;
    case InputKeyRight:
        view_dispatcher_send_custom_event(app->view_dispatcher, IrcEventNext);
        return true;
    default:
        return false;
    }
}

static FuriTimer* s_learn_timer = NULL;

static void learn_tick_cb(void* ctx) {
    IrcApp* app = ctx;
    with_view_model(app->learn_view, LearnViewModel* m, {
        m->tick++;
        m->beam_phase = (uint8_t)((m->beam_phase + 1) % 8);
    }, true);
}

void irc_scene_learn_on_enter(void* ctx) {
    IrcApp* app = ctx;

    view_set_draw_callback(app->learn_view,  learn_draw);
    view_set_input_callback(app->learn_view, learn_input);
    view_set_context(app->learn_view, app);

    // Reset capture state
    app->raw_count    = 0;
    app->last_decoded = false;

    learn_sync(app, LearnStateWaiting);
    view_dispatcher_switch_to_view(app->view_dispatcher, IrcViewLearn);

    // Start IR receiver — raw + decoded callbacks
    infrared_worker_rx_set_received_signal_callback(app->worker, irc_signal_received, app);
    infrared_worker_rx_start(app->worker);

    s_learn_timer = furi_timer_alloc(learn_tick_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(s_learn_timer, 150);
}

bool irc_scene_learn_on_event(void* ctx, SceneManagerEvent ev) {
    IrcApp* app = ctx;
    if(ev.type == SceneManagerEventTypeCustom) {
        if(ev.event == IrcEventSignalCaptured) {
            learn_sync(app, LearnStateDone);
            notification_message(app->notifications, &sequence_single_vibro);
            return true;
        }
        if(ev.event == IrcEventNext) {
            scene_manager_next_scene(app->scene_manager, IrcSceneVault);
            return true;
        }
        if(ev.event == IrcEventBack) {
            view_dispatcher_stop(app->view_dispatcher);
            return true;
        }
    }
    if(ev.type == SceneManagerEventTypeBack) return false;
    return false;
}

void irc_scene_learn_on_exit(void* ctx) {
    IrcApp* app = ctx;
    if(s_learn_timer) {
        furi_timer_stop(s_learn_timer);
        furi_timer_free(s_learn_timer);
        s_learn_timer = NULL;
    }
    infrared_worker_rx_stop(app->worker);
}
