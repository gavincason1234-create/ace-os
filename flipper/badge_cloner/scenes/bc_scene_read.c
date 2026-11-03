// Read scene — holds 125kHz field open, decodes first card found,
// displays full forensics, offers [OK]=Save  [<]=Back

#include "../badge_cloner.h"

// ─────────────────────────────────────────────────────────────────────────────
// View model
// ─────────────────────────────────────────────────────────────────────────────

typedef enum {
    ReadStateScanning = 0,
    ReadStateFound,
    ReadStateError,
} ReadState;

typedef struct {
    ReadState state;
    uint8_t   tick;          // blink counter
    BcCardType card_type;
    uint8_t   data[BC_DATA_MAX];
    uint8_t   data_len;
    // Decoded fields (for HID26: facility + card ID)
    uint32_t  facility;
    uint32_t  card_id;
    bool      has_decoded;
    char      hex[BC_DATA_MAX * 3 + 1];  // space-separated hex
} ReadViewModel;

// ─────────────────────────────────────────────────────────────────────────────
// LFRFID worker callback — called from RFID worker thread
// ─────────────────────────────────────────────────────────────────────────────

static void read_worker_cb(LFRFIDWorkerReadResult result, ProtocolId proto,
                           void* ctx) {
    BcApp* app = ctx;
    if(result != LFRFIDWorkerReadDone) return;

    // Read raw data from the protocol
    ProtocolDict* dict = app->worker->protocols;
    uint8_t buf[BC_DATA_MAX];
    memset(buf, 0, sizeof(buf));
    uint8_t len = protocol_dict_get_data_size(dict, proto);
    if(len > BC_DATA_MAX) len = BC_DATA_MAX;
    protocol_dict_get_data(dict, proto, buf, len);

    // Populate app->current
    app->current.type     = bc_proto_to_type(proto);
    app->current.data_len = len;
    memcpy(app->current.data, buf, len);
    app->current.valid = true;

    // Update view model
    with_view_model(app->read_view, ReadViewModel* m, {
        m->state     = ReadStateFound;
        m->card_type = app->current.type;
        m->data_len  = len;
        memcpy(m->data, buf, len);

        // Build hex string
        char* p = m->hex;
        for(uint8_t i = 0; i < len; i++) {
            if(i > 0) *p++ = ' ';
            p += snprintf(p, 4, "%02X", buf[i]);
        }
        *p = '\0';

        // Decode HID 26-bit: data is 4 bytes, bit layout:
        // [0]=parity MSB | facility[7:1] ... card_id bits ... parity LSB
        m->has_decoded = false;
        if(app->current.type == BcCardHIDProx && len >= 4) {
            uint32_t bits = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16)
                          | ((uint32_t)buf[2] <<  8) |  (uint32_t)buf[3];
            // 26-bit frame: bit25=EP, bits24-17=facility(8), bits16-1=card(16), bit0=OP
            m->facility  = (bits >> 17) & 0xFF;
            m->card_id   = (bits >>  1) & 0xFFFF;
            m->has_decoded = true;
        } else if(app->current.type == BcCardEM4100 && len >= 5) {
            // EM4100 5-byte: byte[0]=version/manufacturer, [1:4]=ID
            m->facility = buf[0];
            m->card_id  = ((uint32_t)buf[1] << 24) | ((uint32_t)buf[2] << 16)
                        | ((uint32_t)buf[3] <<  8) |  (uint32_t)buf[4];
            m->has_decoded = true;
        }
    }, true);

    view_dispatcher_send_custom_event(app->view_dispatcher, BcEventCardRead);
}

// ─────────────────────────────────────────────────────────────────────────────
// Periodic tick timer for scanning animation
// ─────────────────────────────────────────────────────────────────────────────

static void read_tick_cb(void* ctx) {
    BcApp* app = ctx;
    with_view_model(app->read_view, ReadViewModel* m, {
        if(m->state == ReadStateScanning) m->tick++;
    }, true);
}

// ─────────────────────────────────────────────────────────────────────────────
// Draw
// ─────────────────────────────────────────────────────────────────────────────

static void read_draw(Canvas* canvas, void* model) {
    ReadViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Header
    canvas_draw_rbox(canvas, 0, 0, 128, 11, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, "BADGE CLONER");
    canvas_draw_str(canvas, 85, 8, "[ READ ]");
    canvas_set_color(canvas, ColorBlack);

    if(m->state == ReadStateScanning) {
        // Animated scan field: concentric arcs pulse with tick
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 22, AlignCenter, AlignTop, "SCANNING");

        // Pulsing dots
        uint8_t phase = m->tick & 3;
        for(uint8_t i = 0; i < 4; i++) {
            uint8_t x = 44 + i * 10;
            if(i <= phase) canvas_draw_box(canvas, x, 36, 6, 6);
            else           canvas_draw_frame(canvas, x, 36, 6, 6);
        }

        // Scan field circles at center-right
        uint8_t cx = 100, cy = 38;
        uint8_t r = (m->tick % 16) + 4;
        canvas_draw_circle(canvas, cx, cy, r);
        if(r > 8)  canvas_draw_circle(canvas, cx, cy, r - 8);
        if(r > 14) canvas_draw_circle(canvas, cx, cy, r - 14);

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 48, AlignCenter, AlignTop,
                                "Hold badge to back of Flipper");
    } else if(m->state == ReadStateFound) {
        // Card type + raw hex
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 21, bc_card_name(m->card_type));

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 31, m->hex);

        if(m->has_decoded) {
            char fac[24], cid[28];
            snprintf(fac, sizeof(fac), "FAC: %lu",  (unsigned long)m->facility);
            snprintf(cid, sizeof(cid), "ID:  %lu",  (unsigned long)m->card_id);
            canvas_draw_str(canvas, 2, 41, fac);
            canvas_draw_str(canvas, 2, 51, cid);
        }

        // [OK]=Save blink on even ticks
        if(m->tick & 1)
            canvas_draw_str(canvas, 75, 41, "[OK] SAVE");
    }

    // Bottom hint
    canvas_draw_line(canvas, 0, 56, 127, 56);
    canvas_draw_str(canvas, 0, 63, "[OK]save  [<]back");
}

// ─────────────────────────────────────────────────────────────────────────────
// Input
// ─────────────────────────────────────────────────────────────────────────────

static bool read_input(InputEvent* ev, void* ctx) {
    BcApp* app = ctx;
    if(ev->type != InputTypeShort) return false;
    if(ev->key == InputKeyBack) {
        view_dispatcher_send_custom_event(app->view_dispatcher, BcEventBack);
        return true;
    }
    if(ev->key == InputKeyOk && app->current.valid) {
        view_dispatcher_send_custom_event(app->view_dispatcher, BcEventSaveOk);
        return true;
    }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Scene lifecycle
// ─────────────────────────────────────────────────────────────────────────────

// tick timer stored in app->worker (re-used slot) — actually a separate timer
static FuriTimer* s_tick_timer = NULL;

void bc_scene_read_on_enter(void* ctx) {
    BcApp* app = ctx;

    app->current.valid = false;

    with_view_model(app->read_view, ReadViewModel* m, {
        memset(m, 0, sizeof(ReadViewModel));
        m->state = ReadStateScanning;
    }, false);

    view_set_draw_callback(app->read_view,  read_draw);
    view_set_input_callback(app->read_view, read_input);
    view_set_context(app->read_view, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, BcViewRead);

    lfrfid_worker_start_thread(app->worker);
    lfrfid_worker_read_start(app->worker, LFRFIDFeatureASK | LFRFIDFeaturePSK,
                             read_worker_cb, app);

    s_tick_timer = furi_timer_alloc(read_tick_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(s_tick_timer, 250);

    notification_message(app->notifications, &sequence_single_vibro);
}

bool bc_scene_read_on_event(void* ctx, SceneManagerEvent ev) {
    BcApp* app = ctx;
    if(ev.type == SceneManagerEventTypeCustom) {
        if(ev.event == BcEventCardRead) {
            // Badge found — vibrate success, blink tick
            notification_message(app->notifications, &sequence_success);
            with_view_model(app->read_view, ReadViewModel* m, {
                m->tick = 1;
            }, true);
            return true;
        }
        if(ev.event == BcEventSaveOk) {
            bc_save_current(app);
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
        if(ev.event == BcEventBack) {
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
    }
    if(ev.type == SceneManagerEventTypeBack) return false;
    return false;
}

void bc_scene_read_on_exit(void* ctx) {
    BcApp* app = ctx;
    if(s_tick_timer) {
        furi_timer_stop(s_tick_timer);
        furi_timer_free(s_tick_timer);
        s_tick_timer = NULL;
    }
    lfrfid_worker_stop(app->worker);
    lfrfid_worker_stop_thread(app->worker);
}
