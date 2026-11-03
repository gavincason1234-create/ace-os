// IR Remote Cloner — vault scene
// Browse stored IR signals, view details (protocol / timings / duration),
// and replay on demand. Up/Down = select, OK = transmit, Left = back to learn.

#include "../ir_cloner.h"

typedef struct {
    uint8_t  count;
    uint8_t  sel;
    char     label[20];
    char     protocol_line[28];   // "PROTO: NEC  ADDR:0x0D CMD:0x20"
    uint32_t timing_count;
    uint32_t duration_ms;
    bool     transmitting;
    uint8_t  tick;
} VaultViewModel;

static void vault_sync(IrcApp* app) {
    with_view_model(app->vault_view, VaultViewModel* m, {
        m->count        = app->vault_count;
        m->sel          = app->vault_sel;
        m->transmitting = app->transmitting;

        if(app->vault_count == 0) {
            strncpy(m->label, "(empty)", 19);
            m->timing_count = 0;
            m->duration_ms  = 0;
            m->protocol_line[0] = '\0';
            return;
        }

        const IrcSignal* s = &app->vault[app->vault_sel];
        strncpy(m->label, s->label, 19);
        m->timing_count = s->timing_count;

        // Reconstruct duration from timings array
        uint32_t total_us = 0;
        for(uint32_t i = 0; i < s->timing_count; i++)
            total_us += s->timings[i];
        m->duration_ms = total_us / 1000;

        if(s->has_decoded) {
            snprintf(m->protocol_line, sizeof(m->protocol_line),
                     "%s  A:%04lX C:%04lX",
                     s->protocol,
                     (unsigned long)s->address,
                     (unsigned long)s->command);
        } else {
            snprintf(m->protocol_line, sizeof(m->protocol_line),
                     "RAW (no decode)");
        }
    }, true);
}

static void vault_draw(Canvas* canvas, void* model) {
    VaultViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Header
    canvas_draw_rbox(canvas, 0, 0, 128, 11, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    char hdr[20];
    snprintf(hdr, sizeof(hdr), "VAULT [%u/%u]",
             (unsigned)(m->count ? m->sel + 1 : 0), (unsigned)m->count);
    canvas_draw_str(canvas, 2, 8, "IR CLONER");
    canvas_draw_str(canvas, 80, 8, hdr + 6);
    canvas_set_color(canvas, ColorBlack);

    if(m->count == 0) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignTop, "Vault Empty");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignTop,
                                "Capture a signal first");
        canvas_draw_line(canvas, 0, 56, 127, 56);
        canvas_draw_str(canvas, 0, 63, "[<]back to learn");
        return;
    }

    // Signal label
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 22, m->label);

    canvas_set_font(canvas, FontSecondary);

    // Protocol + address/command
    canvas_draw_str(canvas, 2, 32, m->protocol_line);

    // Timing stats
    char stats[28];
    snprintf(stats, sizeof(stats), "%lu timings  ~%lums",
             (unsigned long)m->timing_count, (unsigned long)m->duration_ms);
    canvas_draw_str(canvas, 2, 41, stats);

    // Security note
    canvas_draw_str(canvas, 2, 50, "NO AUTH — replay works every time");

    // TX indicator / button hint
    canvas_draw_line(canvas, 0, 56, 127, 56);
    if(m->transmitting && (m->tick & 1)) {
        canvas_draw_box(canvas, 0, 57, 128, 7);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, ">>> TRANSMITTING IR <<<");
        canvas_set_color(canvas, ColorBlack);
    } else if(m->transmitting) {
        canvas_draw_str(canvas, 0, 63, ">>> TRANSMITTING IR <<<");
    } else {
        if(m->count > 1)
            canvas_draw_str(canvas, 0, 63, "[^v]sel [OK]TX [<]learn");
        else
            canvas_draw_str(canvas, 0, 63, "[OK]TX  [<]back to learn");
    }
}

static bool vault_input(InputEvent* ev, void* ctx) {
    IrcApp* app = ctx;
    if(ev->type != InputTypeShort) return false;

    switch(ev->key) {
    case InputKeyBack:
    case InputKeyLeft:
        view_dispatcher_send_custom_event(app->view_dispatcher, IrcEventBack);
        return true;
    case InputKeyUp:
        if(app->vault_count > 0 && app->vault_sel > 0) {
            app->vault_sel--;
            vault_sync(app);
        }
        return true;
    case InputKeyDown:
        if(app->vault_count > 0 && app->vault_sel < app->vault_count - 1) {
            app->vault_sel++;
            vault_sync(app);
        }
        return true;
    case InputKeyOk:
        if(app->vault_count > 0 && !app->transmitting) {
            const IrcSignal* sig = &app->vault[app->vault_sel];
            irc_start_tx(app, sig);
            notification_message(app->notifications, &sequence_single_vibro);
            vault_sync(app);
        }
        return true;
    default:
        return false;
    }
}

static FuriTimer* s_vault_timer = NULL;

static void vault_tick_cb(void* ctx) {
    IrcApp* app = ctx;
    // Check if TX completed
    if(app->transmitting) {
        // infrared_worker_tx fires and completes on its own;
        // poll once per tick to clean up after the signal ends.
        // Worker signals completion via its own callback; we stop it manually
        // after a generous timeout (one timer tick = 200ms, signal < 100ms typical)
        irc_stop_tx(app);
        vault_sync(app);
        notification_message(app->notifications, &sequence_success);
    }
    with_view_model(app->vault_view, VaultViewModel* m, { m->tick++; }, true);
}

void irc_scene_vault_on_enter(void* ctx) {
    IrcApp* app = ctx;

    if(app->vault_sel >= app->vault_count && app->vault_count > 0)
        app->vault_sel = app->vault_count - 1;

    view_set_draw_callback(app->vault_view,  vault_draw);
    view_set_input_callback(app->vault_view, vault_input);
    view_set_context(app->vault_view, app);

    vault_sync(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, IrcViewVault);

    s_vault_timer = furi_timer_alloc(vault_tick_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(s_vault_timer, 200);
}

bool irc_scene_vault_on_event(void* ctx, SceneManagerEvent ev) {
    IrcApp* app = ctx;
    if(ev.type == SceneManagerEventTypeCustom) {
        if(ev.event == IrcEventBack) {
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
    }
    if(ev.type == SceneManagerEventTypeBack) return false;
    return false;
}

void irc_scene_vault_on_exit(void* ctx) {
    IrcApp* app = ctx;
    if(s_vault_timer) {
        furi_timer_stop(s_vault_timer);
        furi_timer_free(s_vault_timer);
        s_vault_timer = NULL;
    }
    irc_stop_tx(app);
}
