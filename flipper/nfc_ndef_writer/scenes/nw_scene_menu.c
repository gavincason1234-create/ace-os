// NFC NDEF Writer — payload selector menu scene
//
// 4-row list, each showing:
//   [TAG]  Label
//          Description (small)
// OK → transitions to write scene with selected payload

#include "../nfc_ndef_writer.h"

typedef struct {
    uint8_t sel;
    uint8_t tick;
} MenuViewModel;

static void menu_draw(Canvas* canvas, void* model) {
    MenuViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Header
    canvas_draw_rbox(canvas, 0, 0, 128, 11, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, "NFC NDEF WRITER");
    canvas_draw_str(canvas, 96, 8, "v1.0");
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontSecondary);

    for(uint8_t i = 0; i < NW_PRESET_COUNT; i++) {
        uint8_t ry  = (uint8_t)(13 + i * 13);
        bool    sel = (i == m->sel);

        if(sel) {
            canvas_draw_box(canvas, 0, ry, 128, 12);
            canvas_set_color(canvas, ColorWhite);
        }

        // Icon tag
        canvas_draw_str(canvas, 2, (uint8_t)(ry + 9), NW_PRESETS[i].icon_str);

        // Label
        canvas_draw_str(canvas, 20, (uint8_t)(ry + 9), NW_PRESETS[i].label);

        if(sel) canvas_set_color(canvas, ColorBlack);
    }

    // Selected description at bottom
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 63, NW_PRESETS[m->sel].desc);

    // Scroll indicator
    uint8_t dot_y = (uint8_t)(19 + m->sel * 13);
    canvas_draw_dot(canvas, 126, dot_y);
    canvas_draw_dot(canvas, 126, (uint8_t)(dot_y + 1));
    canvas_draw_dot(canvas, 127, dot_y);
}

static bool menu_input(InputEvent* ev, void* ctx) {
    NwApp* app = ctx;
    if(ev->type != InputTypeShort) return false;
    switch(ev->key) {
    case InputKeyUp:
        with_view_model(app->menu_view, MenuViewModel* m, {
            if(m->sel > 0) m->sel--;
            app->payload_type = (NwPayloadType)m->sel;
        }, true);
        return true;
    case InputKeyDown:
        with_view_model(app->menu_view, MenuViewModel* m, {
            if(m->sel < NW_PRESET_COUNT - 1) m->sel++;
            app->payload_type = (NwPayloadType)m->sel;
        }, true);
        return true;
    case InputKeyOk:
        view_dispatcher_send_custom_event(
            app->view_dispatcher, NwEventPayloadSelected);
        return true;
    case InputKeyBack:
        view_dispatcher_send_custom_event(app->view_dispatcher, NwEventBack);
        return true;
    default:
        return false;
    }
}

void nw_scene_menu_on_enter(void* ctx) {
    NwApp* app = ctx;

    view_set_draw_callback(app->menu_view,  menu_draw);
    view_set_input_callback(app->menu_view, menu_input);
    view_set_context(app->menu_view, app);

    with_view_model(app->menu_view, MenuViewModel* m, {
        m->sel  = (uint8_t)app->payload_type;
        m->tick = 0;
    }, true);

    view_dispatcher_switch_to_view(app->view_dispatcher, NwViewMenu);
}

bool nw_scene_menu_on_event(void* ctx, SceneManagerEvent ev) {
    NwApp* app = ctx;
    if(ev.type == SceneManagerEventTypeCustom) {
        if(ev.event == NwEventPayloadSelected) {
            // Build NDEF payload before entering write scene
            app->ndef_len = nw_build_ndef(
                app->payload_type, app->ndef_buf, NW_NDEF_MAX);
            app->write_state = NwWriteStateWaiting;
            scene_manager_next_scene(app->scene_manager, NwSceneWrite);
            return true;
        }
        if(ev.event == NwEventBack) {
            view_dispatcher_stop(app->view_dispatcher);
            return true;
        }
    }
    if(ev.type == SceneManagerEventTypeBack) return false;
    return false;
}

void nw_scene_menu_on_exit(void* ctx) {
    UNUSED(ctx);
}
