// Result scene — full card summary with security grade assessment

#include "../nfc_inspector.h"

typedef struct {
    char card_name[24];
    char uid_str[24];
    char atqa_sak[16];
    char cracked[24];
    char grade[36];
    bool is_classic;
    uint8_t tick;
} NiResultViewModel;

static void result_sync(NiApp* app) {
    const NiCard* c = &app->card;
    char uid_str[24] = "";
    char* p = uid_str;
    for(uint8_t i = 0; i < c->uid_len; i++) {
        if(i > 0) *p++ = ':';
        p += snprintf(p, 4, "%02X", c->uid[i]);
    }

    with_view_model(app->result_view, NiResultViewModel* m, {
        strncpy(m->card_name, ni_card_name(c->type), 23);
        strncpy(m->uid_str, uid_str, 23);
        snprintf(m->atqa_sak, sizeof(m->atqa_sak), "ATQA:%02X%02X SAK:%02X",
                 c->atqa[0], c->atqa[1], c->sak);
        m->is_classic = (c->type == NiCardMifareClassic1K ||
                         c->type == NiCardMifareClassic4K);
        if(m->is_classic)
            snprintf(m->cracked, sizeof(m->cracked), "Keys: %u/%u default",
                     (unsigned)c->cracked_sectors, (unsigned)c->total_sectors);
        else
            snprintf(m->cracked, sizeof(m->cracked), "Sectors: N/A");
        strncpy(m->grade, ni_security_grade(c), 35);
    }, true);
}

static void result_draw(Canvas* canvas, void* model) {
    NiResultViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_draw_rbox(canvas, 0, 0, 128, 11, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, "NFC INSPECTOR");
    canvas_draw_str(canvas, 80, 8, "[ RESULT ]");
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 22, m->card_name);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 31, m->uid_str);
    canvas_draw_str(canvas, 2, 39, m->atqa_sak);
    canvas_draw_str(canvas, 2, 47, m->cracked);

    // Grade — invert box for impact
    uint8_t gw = (uint8_t)(strlen(m->grade) * 5 + 4);
    if(gw > 124) gw = 124;
    canvas_draw_box(canvas, 0, 54, gw, 10);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str(canvas, 2, 62, m->grade);
    canvas_set_color(canvas, ColorBlack);

    if(m->is_classic && (m->tick & 1))
        canvas_draw_str(canvas, 90, 47, "[OK]dump");
}

static bool result_input(InputEvent* ev, void* ctx) {
    NiApp* app = ctx;
    if(ev->type != InputTypeShort) return false;
    if(ev->key == InputKeyBack) {
        view_dispatcher_send_custom_event(app->view_dispatcher, NiEventBack);
        return true;
    }
    if(ev->key == InputKeyOk) {
        bool is_classic = (app->card.type == NiCardMifareClassic1K ||
                           app->card.type == NiCardMifareClassic4K);
        if(is_classic)
            view_dispatcher_send_custom_event(app->view_dispatcher, NiEventNext);
        return true;
    }
    return false;
}

// Tick timer for blinking
static FuriTimer* s_result_timer = NULL;
static void result_tick_cb(void* ctx) {
    NiApp* app = ctx;
    with_view_model(app->result_view, NiResultViewModel* m, { m->tick++; }, true);
}

void ni_scene_result_on_enter(void* ctx) {
    NiApp* app = ctx;
    result_sync(app);

    view_set_draw_callback(app->result_view,  result_draw);
    view_set_input_callback(app->result_view, result_input);
    view_set_context(app->result_view, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, NiViewResult);

    s_result_timer = furi_timer_alloc(result_tick_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(s_result_timer, 500);
}

bool ni_scene_result_on_event(void* ctx, SceneManagerEvent ev) {
    NiApp* app = ctx;
    if(ev.type == SceneManagerEventTypeCustom) {
        if(ev.event == NiEventNext) {
            scene_manager_next_scene(app->scene_manager, NiSceneSectors);
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

void ni_scene_result_on_exit(void* ctx) {
    UNUSED(ctx);
    if(s_result_timer) {
        furi_timer_stop(s_result_timer);
        furi_timer_free(s_result_timer);
        s_result_timer = NULL;
    }
}
