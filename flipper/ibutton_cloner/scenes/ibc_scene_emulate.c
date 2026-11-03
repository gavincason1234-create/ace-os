// iButton Key Cloner — emulate scene
// Flipper presents itself as the cloned iButton key on its 1-Wire port.
// Any reader that touched the original key will accept the Flipper.
// Shows pulsing "EMULATING" indicator, touch-count, and security verdict.
// Up/Down cycles through vault if multiple keys stored.
// Back = return to read scene.

#include "../ibutton_cloner.h"

typedef struct {
    // Displayed key info
    char     label[20];
    char     hex_line1[20];
    char     hex_line2[20];
    char     type_str[16];
    char     use_case[24];
    char     grade[24];
    // Live state
    uint32_t touch_count;   // times reader accepted our emulated key
    uint8_t  vault_count;
    uint8_t  vault_sel;
    uint8_t  tick;
    bool     active;
} EmulateViewModel;

static void emulate_sync(IbcApp* app) {
    with_view_model(app->emulate_view, EmulateViewModel* m, {
        m->vault_count = app->vault_count;
        m->vault_sel   = app->vault_sel;
        m->touch_count = app->emulate_count;

        if(app->vault_count == 0) {
            strncpy(m->label,    "(no keys)", 19);
            m->hex_line1[0] = '\0';
            m->hex_line2[0] = '\0';
            m->type_str[0]  = '\0';
            m->use_case[0]  = '\0';
            strncpy(m->grade, "No key to emulate", 23);
            m->active = false;
            return;
        }

        const IbcKey* k = &app->vault[app->vault_sel];
        strncpy(m->label,    k->label,    19);
        strncpy(m->type_str, k->type_str, 15);
        strncpy(m->use_case, k->use_case, 23);
        strncpy(m->grade,    k->grade,    23);

        if(k->data_len >= 4)
            snprintf(m->hex_line1, sizeof(m->hex_line1),
                     "FC:%02X  %02X:%02X:%02X",
                     k->data[0], k->data[1], k->data[2], k->data[3]);
        if(k->data_len >= 8)
            snprintf(m->hex_line2, sizeof(m->hex_line2),
                     "%02X:%02X:%02X  CRC:%02X",
                     k->data[4], k->data[5], k->data[6], k->data[7]);
        m->active = true;
    }, true);
}

// Load vault key into the IButtonKey buffer and start emulation
static void emulate_start(IbcApp* app) {
    if(app->vault_count == 0) return;
    const IbcKey* k = &app->vault[app->vault_sel];

    ibutton_worker_stop(app->worker);

    // Repopulate the key buffer with vault entry
    ibutton_key_set_data(app->key, k->data, k->data_len);
    ibutton_key_set_type_str(app->key, k->type_str);

    app->emulate_count = 0;
    ibutton_worker_emulate_start(app->worker, ibc_emulate_callback, app);
}

static void emulate_draw(Canvas* canvas, void* model) {
    EmulateViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Header — pulsing invert when active
    bool blink = (m->tick & 1) && m->active;
    if(blink) {
        canvas_draw_rbox(canvas, 0, 0, 128, 11, 2);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, "EMULATING KEY");
    char hdr[16];
    snprintf(hdr, sizeof(hdr), "[%u/%u]",
             (unsigned)(m->vault_count ? m->vault_sel + 1 : 0),
             (unsigned)m->vault_count);
    canvas_draw_str(canvas, 92, 8, hdr);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontSecondary);

    if(!m->active) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignTop, "No Key Stored");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignTop,
                                "Read a key first");
        canvas_draw_line(canvas, 0, 56, 127, 56);
        canvas_draw_str(canvas, 0, 63, "[<]back to read");
        return;
    }

    // Label + hex
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 22, m->label);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 31, m->hex_line1);
    canvas_draw_str(canvas, 2, 39, m->hex_line2);

    // Touch counter — the key moment of the demo
    char touch[28];
    snprintf(touch, sizeof(touch), "DOOR TOUCHES: %lu",
             (unsigned long)m->touch_count);
    if(m->touch_count > 0) {
        // Invert the touch counter when it increments
        uint8_t tw = (uint8_t)(strlen(touch) * 5 + 4);
        canvas_draw_box(canvas, 0, 41, tw, 9);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, 2, 49, touch);
        canvas_set_color(canvas, ColorBlack);
    } else {
        canvas_draw_str(canvas, 2, 49, touch);
        canvas_draw_str(canvas, 2, 49, "Touch reader with Flipper");
    }

    // Grade verdict
    canvas_draw_line(canvas, 0, 51, 127, 51);
    canvas_draw_str(canvas, 2, 59, m->grade);

    // Nav hint
    canvas_draw_line(canvas, 0, 60, 127, 60);
    if(m->vault_count > 1)
        canvas_draw_str(canvas, 0, 63, "[^v]switch key  [<]back");
    else
        canvas_draw_str(canvas, 0, 63, "[<]back to read");
}

static bool emulate_input(InputEvent* ev, void* ctx) {
    IbcApp* app = ctx;
    if(ev->type != InputTypeShort) return false;
    switch(ev->key) {
    case InputKeyBack:
        view_dispatcher_send_custom_event(app->view_dispatcher, IbcEventBack);
        return true;
    case InputKeyUp:
        if(app->vault_count > 0 && app->vault_sel > 0) {
            app->vault_sel--;
            emulate_start(app);
            emulate_sync(app);
        }
        return true;
    case InputKeyDown:
        if(app->vault_count > 0 && app->vault_sel < app->vault_count - 1) {
            app->vault_sel++;
            emulate_start(app);
            emulate_sync(app);
        }
        return true;
    default:
        return false;
    }
}

static FuriTimer* s_emulate_timer = NULL;

static void emulate_tick_cb(void* ctx) {
    IbcApp* app = ctx;
    emulate_sync(app);
    with_view_model(app->emulate_view, EmulateViewModel* m, { m->tick++; }, true);
}

void ibc_scene_emulate_on_enter(void* ctx) {
    IbcApp* app = ctx;

    // Default to most recently added key
    if(app->vault_count > 0)
        app->vault_sel = app->vault_count - 1;
    app->emulate_count = 0;

    view_set_draw_callback(app->emulate_view,  emulate_draw);
    view_set_input_callback(app->emulate_view, emulate_input);
    view_set_context(app->emulate_view, app);

    emulate_sync(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, IbcViewEmulate);

    emulate_start(app);

    s_emulate_timer = furi_timer_alloc(emulate_tick_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(s_emulate_timer, 300);
}

bool ibc_scene_emulate_on_event(void* ctx, SceneManagerEvent ev) {
    IbcApp* app = ctx;
    if(ev.type == SceneManagerEventTypeCustom) {
        if(ev.event == IbcEventBack) {
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
    }
    if(ev.type == SceneManagerEventTypeBack) return false;
    return false;
}

void ibc_scene_emulate_on_exit(void* ctx) {
    IbcApp* app = ctx;
    if(s_emulate_timer) {
        furi_timer_stop(s_emulate_timer);
        furi_timer_free(s_emulate_timer);
        s_emulate_timer = NULL;
    }
    ibutton_worker_stop(app->worker);
}
