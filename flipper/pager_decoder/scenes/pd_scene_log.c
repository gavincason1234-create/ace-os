// Message log scene — scrollable history of decoded pager messages

#include "../pager_decoder.h"

// ─────────────────────────────────────────────────────────────────────────────
// View model
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    uint8_t  count;
    uint8_t  sel;
    // Detail of selected entry
    uint32_t capcode;
    char     msg_type[6];
    char     freq_label[16];
    char     text[PD_MSG_TEXT_LEN];
    bool     has_entry;
} LogViewModel;

static void log_sync(PdApp* app) {
    with_view_model(app->log_view, LogViewModel* m, {
        uint8_t cnt = app->log_count > PD_MSG_LOG_SIZE
                    ? PD_MSG_LOG_SIZE : app->log_count;
        m->count = cnt;
        m->sel   = app->log_sel;
        if(cnt == 0) {
            m->has_entry = false;
        } else {
            uint8_t slot = app->log_sel % PD_MSG_LOG_SIZE;
            const PdMessage* msg = &app->log[slot];
            m->capcode  = msg->capcode;
            strncpy(m->msg_type,   pd_msg_type_label(msg->msg_type), 5);
            // Find frequency label
            m->freq_label[0] = '\0';
            for(uint8_t i = 0; i < PD_FREQ_COUNT; i++) {
                if(PD_FREQUENCIES[i] == msg->freq) {
                    strncpy(m->freq_label, PD_FREQ_LABELS[i], 15);
                    break;
                }
            }
            strncpy(m->text, msg->text, PD_MSG_TEXT_LEN - 1);
            m->has_entry = true;
        }
    }, true);
}

// ─────────────────────────────────────────────────────────────────────────────
// Draw
// ─────────────────────────────────────────────────────────────────────────────

static void log_draw(Canvas* canvas, void* model) {
    LogViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_draw_rbox(canvas, 0, 0, 128, 11, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    char hdr[28];
    if(m->count > 0)
        snprintf(hdr, sizeof(hdr), "LOG [%u/%u]",
                 (unsigned)m->sel + 1, (unsigned)m->count);
    else
        strncpy(hdr, "LOG [0/0]", 27);
    canvas_draw_str(canvas, 2, 8, "PAGER DECODER");
    canvas_draw_str(canvas, 80, 8, hdr + 4);
    canvas_set_color(canvas, ColorBlack);

    if(!m->has_entry) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignTop, "No messages");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignTop, "Scan to capture");
    } else {
        char capstr[20];
        snprintf(capstr, sizeof(capstr), "CAP: %07lu",
                 (unsigned long)m->capcode);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 22, capstr);

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 30, m->freq_label);
        canvas_draw_str(canvas, 80, 30, m->msg_type);

        // Text with word wrap
        char line1[22], line2[22];
        uint8_t tlen = (uint8_t)strlen(m->text);
        if(tlen <= 21) {
            strncpy(line1, m->text, 21); line1[21] = '\0';
            line2[0] = '\0';
        } else {
            strncpy(line1, m->text, 21); line1[21] = '\0';
            strncpy(line2, m->text + 21, 21); line2[21] = '\0';
        }
        canvas_draw_str(canvas, 2, 39, line1);
        if(line2[0]) canvas_draw_str(canvas, 2, 48, line2);
    }

    canvas_draw_line(canvas, 0, 56, 127, 56);
    canvas_draw_str(canvas, 0, 63, "[^v]scroll  [<]back");
}

// ─────────────────────────────────────────────────────────────────────────────
// Input
// ─────────────────────────────────────────────────────────────────────────────

static bool log_input(InputEvent* ev, void* ctx) {
    PdApp* app = ctx;
    if(ev->type != InputTypeShort) return false;

    uint8_t cnt = app->log_count > PD_MSG_LOG_SIZE
                ? PD_MSG_LOG_SIZE : app->log_count;

    if(ev->key == InputKeyBack) {
        view_dispatcher_send_custom_event(app->view_dispatcher, PdEventBack);
        return true;
    }
    if(ev->key == InputKeyUp && cnt > 0 && app->log_sel > 0) {
        app->log_sel--;
        log_sync(app);
        return true;
    }
    if(ev->key == InputKeyDown && cnt > 0 && app->log_sel < cnt - 1) {
        app->log_sel++;
        log_sync(app);
        return true;
    }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Scene lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void pd_scene_log_on_enter(void* ctx) {
    PdApp* app = ctx;
    uint8_t cnt = app->log_count > PD_MSG_LOG_SIZE
                ? PD_MSG_LOG_SIZE : app->log_count;
    if(cnt > 0) app->log_sel = cnt - 1;  // show most recent first

    view_set_draw_callback(app->log_view,  log_draw);
    view_set_input_callback(app->log_view, log_input);
    view_set_context(app->log_view, app);

    log_sync(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, PdViewLog);
}

bool pd_scene_log_on_event(void* ctx, SceneManagerEvent ev) {
    PdApp* app = ctx;
    if(ev.type == SceneManagerEventTypeCustom) {
        if(ev.event == PdEventBack) {
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
    }
    if(ev.type == SceneManagerEventTypeBack) return false;
    return false;
}

void pd_scene_log_on_exit(void* ctx) {
    UNUSED(ctx);
}
