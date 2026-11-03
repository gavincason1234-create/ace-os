// Read scene — NFC field on, waiting for ISO14443-A card

#include "../nfc_inspector.h"

typedef enum {
    ReadPhaseWaiting = 0,
    ReadPhaseFound,
    ReadPhaseAnalyzing,
    ReadPhaseDone,
    ReadPhaseError,
} ReadPhase;

typedef struct {
    ReadPhase phase;
    uint8_t   tick;
    char      card_name[24];
    char      uid_str[24];
    char      status[32];
} NiReadViewModel;

// Tick timer for animation
static FuriTimer* s_read_timer = NULL;

static void read_tick_cb(void* ctx) {
    NiApp* app = ctx;
    with_view_model(app->read_view, NiReadViewModel* m, {
        m->tick++;
    }, true);
}

// Draw
static void read_draw(Canvas* canvas, void* model) {
    NiReadViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_draw_rbox(canvas, 0, 0, 128, 11, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, "NFC INSPECTOR");
    canvas_draw_str(canvas, 88, 8, "[ SCAN ]");
    canvas_set_color(canvas, ColorBlack);

    if(m->phase == ReadPhaseWaiting) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignTop, "WAITING");

        // Ripple rings from card icon at right
        uint8_t cx = 104, cy = 38;
        uint8_t r = (m->tick % 20) + 3;
        canvas_draw_circle(canvas, cx, cy, r);
        if(r > 8)  canvas_draw_circle(canvas, cx, cy, r - 7);
        if(r > 15) canvas_draw_circle(canvas, cx, cy, r - 14);
        canvas_draw_box(canvas, cx - 2, cy - 2, 4, 4);

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 56, 33, AlignCenter, AlignTop,
                                "Hold card to back");
        canvas_draw_str_aligned(canvas, 56, 42, AlignCenter, AlignTop,
                                "of Flipper Zero");
    } else if(m->phase == ReadPhaseFound || m->phase == ReadPhaseAnalyzing) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 22, m->card_name);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 31, "UID:");
        canvas_draw_str(canvas, 22, 31, m->uid_str);
        canvas_draw_str(canvas, 2, 42, m->status);

        // Spin indicator
        const char* spin[] = { "|", "/", "-", "\\" };
        canvas_draw_str(canvas, 120, 42, spin[m->tick & 3]);
    } else if(m->phase == ReadPhaseDone) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 22, m->card_name);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 31, "UID:");
        canvas_draw_str(canvas, 22, 31, m->uid_str);
        canvas_draw_str(canvas, 2, 42, m->status);
        // Checkmark
        canvas_draw_str(canvas, 110, 42, "[OK]");
    } else {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignTop, "ERROR");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignTop, m->status);
    }

    canvas_draw_line(canvas, 0, 56, 127, 56);
    if(m->phase == ReadPhaseDone)
        canvas_draw_str(canvas, 0, 63, "[OK]details  [<]back");
    else
        canvas_draw_str(canvas, 0, 63, "[<]back");
}

// Input
static bool read_input(InputEvent* ev, void* ctx) {
    NiApp* app = ctx;
    if(ev->type != InputTypeShort) return false;
    if(ev->key == InputKeyBack) {
        view_dispatcher_send_custom_event(app->view_dispatcher, NiEventBack);
        return true;
    }
    if(ev->key == InputKeyOk && app->card.valid) {
        view_dispatcher_send_custom_event(app->view_dispatcher, NiEventNext);
        return true;
    }
    return false;
}

// Scene lifecycle
void ni_scene_read_on_enter(void* ctx) {
    NiApp* app = ctx;

    app->card.valid = false;
    memset(&app->card, 0, sizeof(NiCard));

    with_view_model(app->read_view, NiReadViewModel* m, {
        memset(m, 0, sizeof(NiReadViewModel));
        m->phase = ReadPhaseWaiting;
    }, false);

    view_set_draw_callback(app->read_view,  read_draw);
    view_set_input_callback(app->read_view, read_input);
    view_set_context(app->read_view, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, NiViewRead);

    // Start NFC poller
    nfc_start(app->nfc, ni_nfc_callback, app);

    s_read_timer = furi_timer_alloc(read_tick_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(s_read_timer, 200);

    notification_message(app->notifications, &sequence_single_vibro);
}

bool ni_scene_read_on_event(void* ctx, SceneManagerEvent ev) {
    NiApp* app = ctx;
    if(ev.type == SceneManagerEventTypeCustom) {
        if(ev.event == NiEventCardDetected) {
            notification_message(app->notifications, &sequence_success);

            // Build UID string
            char uid_str[24] = "";
            char* p = uid_str;
            for(uint8_t i = 0; i < app->card.uid_len; i++) {
                if(i > 0) *p++ = ':';
                p += snprintf(p, 4, "%02X", app->card.uid[i]);
            }

            with_view_model(app->read_view, NiReadViewModel* m, {
                m->phase = ReadPhaseAnalyzing;
                strncpy(m->card_name, ni_card_name(app->card.type), 23);
                strncpy(m->uid_str,   uid_str,                       23);
                strncpy(m->status,    "Analyzing sectors...",         31);
            }, true);

            // Run MIFARE sector scan (synchronous, from event handler)
            if(app->card.type == NiCardMifareClassic1K ||
               app->card.type == NiCardMifareClassic4K) {
                ni_scan_mifare(app);
            }

            with_view_model(app->read_view, NiReadViewModel* m, {
                m->phase = ReadPhaseDone;
                if(app->card.type == NiCardMifareClassic1K ||
                   app->card.type == NiCardMifareClassic4K) {
                    snprintf(m->status, 32, "Cracked: %u/%u sectors",
                             (unsigned)app->card.cracked_sectors,
                             (unsigned)app->card.total_sectors);
                } else {
                    strncpy(m->status, "Scan complete", 31);
                }
            }, true);

            return true;
        }
        if(ev.event == NiEventNext) {
            scene_manager_next_scene(app->scene_manager, NiSceneResult);
            return true;
        }
        if(ev.event == NiEventBack) {
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
    }
    if(ev.type == SceneManagerEventTypeBack) return false;
    return false;
}

void ni_scene_read_on_exit(void* ctx) {
    NiApp* app = ctx;
    if(s_read_timer) {
        furi_timer_stop(s_read_timer);
        furi_timer_free(s_read_timer);
        s_read_timer = NULL;
    }
    nfc_stop(app->nfc);
}
