// BadUSB Launcher — payload selector menu scene
//
// 4-row list: [TAG] Label     (OS badge)
//                   Description
// OK → run scene with 3-second countdown before injection

#include "../badusb_launcher.h"

typedef struct {
    uint8_t sel;
} MenuViewModel;

static void menu_draw(Canvas* canvas, void* model) {
    MenuViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Header
    canvas_draw_rbox(canvas, 0, 0, 128, 11, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, "BADUSB LAUNCHER");
    canvas_draw_str(canvas, 96, 8, "HID");
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontSecondary);

    for(uint8_t i = 0; i < BU_PRESET_COUNT; i++) {
        uint8_t ry  = (uint8_t)(13 + i * 13);
        bool    sel = (i == m->sel);

        if(sel) {
            canvas_draw_box(canvas, 0, ry, 128, 12);
            canvas_set_color(canvas, ColorWhite);
        }

        // Icon tag in small box
        canvas_draw_str(canvas, 2, (uint8_t)(ry + 9), BU_PRESETS[i].icon);

        // Label
        canvas_draw_str(canvas, 20, (uint8_t)(ry + 9), BU_PRESETS[i].label);

        // OS badge right-aligned
        canvas_draw_str(canvas, 104, (uint8_t)(ry + 9), BU_PRESETS[i].target_os);

        if(sel) canvas_set_color(canvas, ColorBlack);
    }

    // Description of selected item at bottom
    canvas_draw_str(canvas, 0, 63, BU_PRESETS[m->sel].desc);
}

static bool menu_input(InputEvent* ev, void* ctx) {
    BuApp* app = ctx;
    if(ev->type != InputTypeShort) return false;
    switch(ev->key) {
    case InputKeyUp:
        with_view_model(app->menu_view, MenuViewModel* m, {
            if(m->sel > 0) m->sel--;
            app->payload_type = (BuPayloadType)m->sel;
        }, true);
        return true;
    case InputKeyDown:
        with_view_model(app->menu_view, MenuViewModel* m, {
            if(m->sel < BU_PRESET_COUNT - 1) m->sel++;
            app->payload_type = (BuPayloadType)m->sel;
        }, true);
        return true;
    case InputKeyOk:
        view_dispatcher_send_custom_event(
            app->view_dispatcher, BuEventPayloadSelected);
        return true;
    case InputKeyBack:
        view_dispatcher_send_custom_event(app->view_dispatcher, BuEventBack);
        return true;
    default:
        return false;
    }
}

void bu_scene_menu_on_enter(void* ctx) {
    BuApp* app = ctx;

    view_set_draw_callback(app->menu_view,  menu_draw);
    view_set_input_callback(app->menu_view, menu_input);
    view_set_context(app->menu_view, app);

    with_view_model(app->menu_view, MenuViewModel* m, {
        m->sel = (uint8_t)app->payload_type;
    }, true);

    view_dispatcher_switch_to_view(app->view_dispatcher, BuViewMenu);
}

bool bu_scene_menu_on_event(void* ctx, SceneManagerEvent ev) {
    BuApp* app = ctx;
    if(ev.type == SceneManagerEventTypeCustom) {
        if(ev.event == BuEventPayloadSelected) {
            scene_manager_next_scene(app->scene_manager, BuSceneRun);
            return true;
        }
        if(ev.event == BuEventBack) {
            view_dispatcher_stop(app->view_dispatcher);
            return true;
        }
    }
    if(ev.type == SceneManagerEventTypeBack) return false;
    return false;
}

void bu_scene_menu_on_exit(void* ctx) {
    UNUSED(ctx);
}
