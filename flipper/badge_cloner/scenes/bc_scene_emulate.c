// Emulate scene — broadcasts selected 125kHz badge continuously
// Security grade display shows how trivially a badge can be spoofed.

#include "../badge_cloner.h"

// ─────────────────────────────────────────────────────────────────────────────
// View model
// ─────────────────────────────────────────────────────────────────────────────

typedef enum {
    EmulStateStarting = 0,
    EmulStateActive,
    EmulStateStopped,
} EmulState;

typedef struct {
    EmulState  state;
    uint8_t    tick;
    BcCardType card_type;
    char       label[BC_LABEL_LEN];
    char       hex[BC_DATA_MAX * 3 + 1];
    uint32_t   facility;
    uint32_t   card_id;
    bool       has_decoded;
    uint32_t   tx_count;    // increments each second while active
} EmulViewModel;

// ─────────────────────────────────────────────────────────────────────────────
// LFRFID emulation callback
// ─────────────────────────────────────────────────────────────────────────────

static void emul_worker_cb(LFRFIDWorkerEmulateResult result, void* ctx) {
    UNUSED(result);
    BcApp* app = ctx;
    // Worker signals each full transmission cycle
    with_view_model(app->emulate_view, EmulViewModel* m, {
        m->tx_count++;
        m->tick++;
        m->state = EmulStateActive;
    }, true);
}

// ─────────────────────────────────────────────────────────────────────────────
// Draw — security grade assessment panel
// ─────────────────────────────────────────────────────────────────────────────

static void emul_draw(Canvas* canvas, void* model) {
    EmulViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Header — red/inverted when active
    canvas_draw_rbox(canvas, 0, 0, 128, 11, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, "EMULATING");

    if(m->state == EmulStateActive && (m->tick & 1))
        canvas_draw_str(canvas, 72, 8, ">>> TX <<<");
    else
        canvas_draw_str(canvas, 72, 8, ">>> TX <<<");

    canvas_set_color(canvas, ColorBlack);

    // Badge label and type
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 22, m->label);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 31, bc_card_name(m->card_type));
    canvas_draw_str(canvas, 2, 39, m->hex);

    // Decoded fields
    if(m->has_decoded) {
        char fac[20], cid[24];
        snprintf(fac, sizeof(fac), "FAC:%lu", (unsigned long)m->facility);
        snprintf(cid, sizeof(cid), "ID:%lu",  (unsigned long)m->card_id);
        canvas_draw_str(canvas, 2, 47, fac);
        canvas_draw_str(canvas, 55, 47, cid);
    }

    // Security grade panel
    // 125kHz = INSECURE: no encryption, no rolling code, trivial to clone
    const char* grade;
    const char* verdict;
    switch(m->card_type) {
    case BcCardEM4100:
        grade   = "GRADE: F";
        verdict = "No security";
        break;
    case BcCardHIDProx:
        grade   = "GRADE: F";
        verdict = "No encryption";
        break;
    case BcCardHIDCorp1000:
        grade   = "GRADE: D";
        verdict = "Still clonable";
        break;
    case BcCardIOProx:
        grade   = "GRADE: F";
        verdict = "Plain broadcast";
        break;
    default:
        grade   = "GRADE: F";
        verdict = "No security";
        break;
    }
    canvas_draw_str(canvas, 75, 22, grade);
    canvas_draw_str(canvas, 75, 31, verdict);

    // TX count
    char txbuf[16];
    snprintf(txbuf, sizeof(txbuf), "TX: %lu", (unsigned long)m->tx_count);
    canvas_draw_str(canvas, 75, 39, txbuf);

    canvas_draw_line(canvas, 0, 56, 127, 56);
    canvas_draw_str(canvas, 0, 63, "[OK]stop  [<]back");
}

// ─────────────────────────────────────────────────────────────────────────────
// Input
// ─────────────────────────────────────────────────────────────────────────────

static bool emul_input(InputEvent* ev, void* ctx) {
    BcApp* app = ctx;
    if(ev->type != InputTypeShort) return false;
    if(ev->key == InputKeyOk || ev->key == InputKeyBack) {
        view_dispatcher_send_custom_event(app->view_dispatcher, BcEventEmulStop);
        return true;
    }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Scene lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void bc_scene_emulate_on_enter(void* ctx) {
    BcApp* app = ctx;
    const BcBadge* b = &app->current;

    // Initialize view model from current badge
    with_view_model(app->emulate_view, EmulViewModel* m, {
        memset(m, 0, sizeof(EmulViewModel));
        m->state     = EmulStateStarting;
        m->card_type = b->type;
        strncpy(m->label, b->label, BC_LABEL_LEN - 1);

        char* p = m->hex;
        for(uint8_t i = 0; i < b->data_len; i++) {
            if(i > 0) *p++ = ' ';
            p += snprintf(p, 4, "%02X", b->data[i]);
        }
        *p = '\0';

        m->has_decoded = false;
        if(b->type == BcCardHIDProx && b->data_len >= 4) {
            uint32_t bits = ((uint32_t)b->data[0] << 24) | ((uint32_t)b->data[1] << 16)
                          | ((uint32_t)b->data[2] <<  8) |  (uint32_t)b->data[3];
            m->facility  = (bits >> 17) & 0xFF;
            m->card_id   = (bits >>  1) & 0xFFFF;
            m->has_decoded = true;
        } else if(b->type == BcCardEM4100 && b->data_len >= 5) {
            m->facility = b->data[0];
            m->card_id  = ((uint32_t)b->data[1] << 24) | ((uint32_t)b->data[2] << 16)
                        | ((uint32_t)b->data[3] <<  8) |  (uint32_t)b->data[4];
            m->has_decoded = true;
        }
    }, false);

    view_set_draw_callback(app->emulate_view,  emul_draw);
    view_set_input_callback(app->emulate_view, emul_input);
    view_set_context(app->emulate_view, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, BcViewEmulate);

    // Load badge data into worker protocol dict, then start emulation
    LFRFIDProtocol proto = bc_type_to_proto(b->type);
    lfrfid_worker_start_thread(app->worker);
    protocol_dict_set_data(app->worker->protocols, proto, b->data, b->data_len);
    lfrfid_worker_emulate_start(app->worker, proto, emul_worker_cb, app);

    app->emulating = true;
    notification_message(app->notifications, &sequence_single_vibro);
}

bool bc_scene_emulate_on_event(void* ctx, SceneManagerEvent ev) {
    BcApp* app = ctx;
    if(ev.type == SceneManagerEventTypeCustom) {
        if(ev.event == BcEventEmulStop) {
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
    }
    if(ev.type == SceneManagerEventTypeBack) return false;
    return false;
}

void bc_scene_emulate_on_exit(void* ctx) {
    BcApp* app = ctx;
    if(app->emulating) {
        lfrfid_worker_stop(app->worker);
        lfrfid_worker_stop_thread(app->worker);
        app->emulating = false;
    }
}
