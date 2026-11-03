// UART Debug Sniffer — baud rate selection scene
// Presents a scrollable list of common baud rates.
// Up/Down navigate, OK confirms and starts sniffing.
// Shows wiring diagram hint at the bottom.

#include "../uart_sniffer.h"

typedef struct {
    uint8_t sel;
    uint8_t tick;
} BaudViewModel;

static void baud_draw(Canvas* canvas, void* model) {
    BaudViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Header
    canvas_draw_rbox(canvas, 0, 0, 128, 11, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, "UART DEBUG SNIFFER");
    canvas_set_color(canvas, ColorBlack);

    // Baud rate list — show 4 rows centered on selection
    canvas_set_font(canvas, FontSecondary);
    int8_t top = (int8_t)m->sel - 2;
    if(top < 0) top = 0;
    if(top + 4 > US_BAUD_COUNT) top = US_BAUD_COUNT - 4;
    if(top < 0) top = 0;

    for(uint8_t r = 0; r < 4 && (top + r) < US_BAUD_COUNT; r++) {
        uint8_t idx = (uint8_t)(top + r);
        uint8_t ry  = (uint8_t)(14 + r * 11);
        bool    sel = (idx == m->sel);

        if(sel) {
            canvas_draw_box(canvas, 20, ry - 1, 88, 11);
            canvas_set_color(canvas, ColorWhite);
        }
        char label[16];
        snprintf(label, sizeof(label), "%lu baud", (unsigned long)US_BAUDS[idx]);
        canvas_draw_str_aligned(canvas, 64, (uint8_t)(ry + 7),
                                AlignCenter, AlignBottom, label);
        if(sel) canvas_set_color(canvas, ColorBlack);
    }

    // Wiring hint
    canvas_draw_line(canvas, 0, 57, 127, 57);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 63, "GPIO13(RX)->TgtTX  GND->GND");
}

static bool baud_input(InputEvent* ev, void* ctx) {
    UsApp* app = ctx;
    if(ev->type != InputTypeShort) return false;
    switch(ev->key) {
    case InputKeyUp:
        with_view_model(app->baud_view, BaudViewModel* m, {
            if(m->sel > 0) { m->sel--; app->baud_idx = m->sel; }
        }, true);
        return true;
    case InputKeyDown:
        with_view_model(app->baud_view, BaudViewModel* m, {
            if(m->sel < US_BAUD_COUNT - 1) { m->sel++; app->baud_idx = m->sel; }
        }, true);
        return true;
    case InputKeyOk:
        view_dispatcher_send_custom_event(app->view_dispatcher, UsEventBaudSelected);
        return true;
    case InputKeyBack:
        view_dispatcher_send_custom_event(app->view_dispatcher, UsEventBack);
        return true;
    default:
        return false;
    }
}

void us_scene_baud_on_enter(void* ctx) {
    UsApp* app = ctx;

    view_set_draw_callback(app->baud_view,  baud_draw);
    view_set_input_callback(app->baud_view, baud_input);
    view_set_context(app->baud_view, app);

    with_view_model(app->baud_view, BaudViewModel* m, {
        m->sel = app->baud_idx;
    }, true);

    view_dispatcher_switch_to_view(app->view_dispatcher, UsViewBaud);
}

bool us_scene_baud_on_event(void* ctx, SceneManagerEvent ev) {
    UsApp* app = ctx;
    if(ev.type == SceneManagerEventTypeCustom) {
        if(ev.event == UsEventBaudSelected) {
            scene_manager_next_scene(app->scene_manager, UsSceneSniff);
            return true;
        }
        if(ev.event == UsEventBack) {
            view_dispatcher_stop(app->view_dispatcher);
            return true;
        }
    }
    if(ev.type == SceneManagerEventTypeBack) return false;
    return false;
}

void us_scene_baud_on_exit(void* ctx) {
    UNUSED(ctx);
}
