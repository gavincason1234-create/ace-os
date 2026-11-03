// NFC NDEF Writer — write scene
//
// States:
//   Waiting    → "PRESENT NFC TAG" + animated RF rings
//   Detected   → "TAG FOUND" blip (brief)
//   Writing    → "WRITING..." spinner
//   Done       → "[OK] NDEF WRITTEN" + page count
//   Error      → "[ERR] WRITE FAILED"
//   Protected  → "[ERR] TAG PROTECTED"
//
// Controls:
//   Back       → stop poller, return to menu
//   OK (Done)  → return to menu (write again)

#include "../nfc_ndef_writer.h"

// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    NwWriteState state;
    uint8_t      tick;
    char         label[24];       // payload label
    char         preview[44];     // payload preview string
    uint32_t     write_count;
    uint16_t     ndef_len;
} WriteViewModel;

// ─────────────────────────────────────────────────────────────────────────────
// Animated RF rings — 3 concentric ovals, phase-shifted
// ─────────────────────────────────────────────────────────────────────────────

static void draw_rf_rings(Canvas* canvas, uint8_t cx, uint8_t cy, uint8_t phase) {
    for(uint8_t r = 0; r < 3; r++) {
        uint8_t offset = (uint8_t)((r * 8 + phase) % 24);
        uint8_t rw = (uint8_t)(6 + r * 8);
        uint8_t rh = (uint8_t)(4 + r * 5);
        // Fade: only draw ring if active in phase
        if(offset < 16) {
            canvas_draw_rframe(
                canvas,
                (uint8_t)(cx - rw),
                (uint8_t)(cy - rh),
                (uint8_t)(rw * 2),
                (uint8_t)(rh * 2),
                2);
        }
    }
    // NFC chip square in center
    canvas_draw_box(canvas, (uint8_t)(cx - 5), (uint8_t)(cy - 4), 10, 8);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str(canvas, (uint8_t)(cx - 4), (uint8_t)(cy + 3), "NFC");
    canvas_set_color(canvas, ColorBlack);
}

// ─────────────────────────────────────────────────────────────────────────────
// Draw callback
// ─────────────────────────────────────────────────────────────────────────────

static void write_draw(Canvas* canvas, void* model) {
    WriteViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Header
    canvas_draw_rbox(canvas, 0, 0, 128, 11, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, "NDEF WRITER");
    canvas_draw_str(canvas, 64, 8, m->label);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontSecondary);

    switch(m->state) {
    case NwWriteStateWaiting: {
        // Animated RF rings centered at (64, 40)
        draw_rf_rings(canvas, 64, 42, (uint8_t)(m->tick % 24));
        // Blinking instruction at top of animation area
        if(m->tick & 1)
            canvas_draw_str(canvas, 10, 22, ">>> PRESENT NFC TAG <<<");
        // Payload preview at bottom
        canvas_draw_str(canvas, 0, 63, m->preview);
        break;
    }

    case NwWriteStateDetected: {
        draw_rf_rings(canvas, 64, 35, 0);
        canvas_draw_rbox(canvas, 20, 48, 88, 11, 2);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, 28, 56, "TAG DETECTED...");
        canvas_set_color(canvas, ColorBlack);
        break;
    }

    case NwWriteStateWriting: {
        // Spinner
        static const char* spin = "|/-\\";
        char sp[3] = {spin[m->tick % 4], ' ', '\0'};
        canvas_draw_str(canvas, 2, 30, sp);
        canvas_draw_str(canvas, 14, 30, "WRITING NDEF...");

        // Progress indicator
        char pg[32];
        snprintf(pg, sizeof(pg), "%u bytes / %u pages",
                 (unsigned)m->ndef_len,
                 (unsigned)((m->ndef_len + 3) / 4 + 1)); // +1 for CC
        canvas_draw_str(canvas, 2, 45, pg);
        break;
    }

    case NwWriteStateDone: {
        // Big OK
        canvas_draw_rbox(canvas, 0, 12, 128, 14, 3);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, 28, 22, "[OK]  NDEF WRITTEN");
        canvas_set_color(canvas, ColorBlack);

        char info[40];
        snprintf(info, sizeof(info), "%u pages written  (%u bytes)",
                 (unsigned)m->write_count,
                 (unsigned)m->ndef_len);
        canvas_draw_str(canvas, 0, 38, info);
        canvas_draw_str(canvas, 0, 50, m->preview);
        canvas_draw_str(canvas, 14, 63, "OK: write again   Back: menu");
        break;
    }

    case NwWriteStateError: {
        canvas_draw_rbox(canvas, 0, 12, 128, 14, 3);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, 16, 22, "[ERR]  WRITE FAILED");
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_str(canvas, 4, 40, "Check tag placement");
        canvas_draw_str(canvas, 4, 52, "and try again");
        canvas_draw_str(canvas, 14, 63, "OK: retry   Back: menu");
        break;
    }

    case NwWriteStateProtected: {
        canvas_draw_rbox(canvas, 0, 12, 128, 14, 3);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, 8, 22, "[ERR]  TAG PROTECTED");
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_str(canvas, 4, 40, "Tag is write-protected");
        canvas_draw_str(canvas, 4, 52, "or locked");
        canvas_draw_str(canvas, 14, 63, "Back: menu");
        break;
    }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Input callback
// ─────────────────────────────────────────────────────────────────────────────

static bool write_input(InputEvent* ev, void* ctx) {
    NwApp* app = ctx;
    if(ev->type != InputTypeShort) return false;

    switch(ev->key) {
    case InputKeyBack:
        view_dispatcher_send_custom_event(app->view_dispatcher, NwEventBack);
        return true;
    case InputKeyOk:
        if(app->write_state == NwWriteStateDone ||
           app->write_state == NwWriteStateError) {
            // Restart write
            app->write_state = NwWriteStateWaiting;
            nw_poller_stop(app);
            nw_poller_start(app);
            with_view_model(app->write_view, WriteViewModel* m, {
                m->state = NwWriteStateWaiting;
            }, true);
        }
        return true;
    default:
        return false;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 300ms animation timer
// ─────────────────────────────────────────────────────────────────────────────

static FuriTimer* s_write_timer = NULL;

static void write_tick_cb(void* ctx) {
    NwApp* app = ctx;
    with_view_model(app->write_view, WriteViewModel* m, {
        m->tick++;
        // Transition Detected → Writing after 2 ticks (~600ms)
        if(m->state == NwWriteStateDetected && m->tick > 2)
            m->state = NwWriteStateWriting;
        // Sync result states from app
        if(app->write_state == NwWriteStateDone ||
           app->write_state == NwWriteStateError ||
           app->write_state == NwWriteStateProtected) {
            m->state       = app->write_state;
            m->write_count = app->write_count;
        }
    }, true);
}

// ─────────────────────────────────────────────────────────────────────────────
// Scene lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void nw_scene_write_on_enter(void* ctx) {
    NwApp* app = ctx;

    view_set_draw_callback(app->write_view,  write_draw);
    view_set_input_callback(app->write_view, write_input);
    view_set_context(app->write_view, app);

    const NwPreset* preset = &NW_PRESETS[app->payload_type];

    with_view_model(app->write_view, WriteViewModel* m, {
        m->state       = NwWriteStateWaiting;
        m->tick        = 0;
        m->write_count = 0;
        m->ndef_len    = app->ndef_len;
        strncpy(m->label,   preset->label,   sizeof(m->label)   - 1);
        strncpy(m->preview, preset->preview, sizeof(m->preview) - 1);
        m->label[sizeof(m->label)   - 1] = '\0';
        m->preview[sizeof(m->preview) - 1] = '\0';
    }, true);

    app->write_state = NwWriteStateWaiting;
    app->write_count = 0;

    view_dispatcher_switch_to_view(app->view_dispatcher, NwViewWrite);

    nw_poller_start(app);

    s_write_timer = furi_timer_alloc(write_tick_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(s_write_timer, 300);
}

bool nw_scene_write_on_event(void* ctx, SceneManagerEvent ev) {
    NwApp* app = ctx;
    if(ev.type == SceneManagerEventTypeCustom) {
        if(ev.event == NwEventTagDetected) {
            app->write_state = NwWriteStateDetected;
            with_view_model(app->write_view, WriteViewModel* m, {
                m->state = NwWriteStateDetected;
                m->tick  = 0;
            }, true);
            notification_message(app->notifications, &sequence_single_vibro);
            return true;
        }
        if(ev.event == NwEventWriteDone) {
            app->write_state = NwWriteStateDone;
            nw_poller_stop(app);
            notification_message(app->notifications, &sequence_double_vibro);
            notification_message(app->notifications, &sequence_success);
            return true;
        }
        if(ev.event == NwEventWriteError) {
            app->write_state = NwWriteStateError;
            nw_poller_stop(app);
            notification_message(app->notifications, &sequence_error);
            return true;
        }
        if(ev.event == NwEventWriteProtected) {
            app->write_state = NwWriteStateProtected;
            nw_poller_stop(app);
            notification_message(app->notifications, &sequence_error);
            return true;
        }
        if(ev.event == NwEventBack) {
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
    }
    if(ev.type == SceneManagerEventTypeBack) return false;
    return false;
}

void nw_scene_write_on_exit(void* ctx) {
    NwApp* app = ctx;
    if(s_write_timer) {
        furi_timer_stop(s_write_timer);
        furi_timer_free(s_write_timer);
        s_write_timer = NULL;
    }
    nw_poller_stop(app);
}
