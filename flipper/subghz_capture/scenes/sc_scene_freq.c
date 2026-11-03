// Sub-GHz Capture — frequency selector scene

#include "../subghz_capture.h"

typedef struct {
    uint8_t sel;
} FreqViewModel;

static void freq_draw(Canvas* canvas, void* model) {
    FreqViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Header
    canvas_draw_rbox(canvas, 0, 0, 128, 11, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, "SUBGHZ CAPTURE");
    canvas_draw_str(canvas, 90, 8, "OOK");
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 20, "Select target frequency:");

    for(uint8_t i = 0; i < SC_FREQ_COUNT; i++) {
        uint8_t ry  = (uint8_t)(24 + i * 9);
        bool    sel = (i == m->sel);

        if(sel) {
            canvas_draw_box(canvas, 0, ry - 1, 128, 9);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, 2,  (uint8_t)(ry + 6), SC_FREQS[i].label);
        canvas_draw_str(canvas, 76, (uint8_t)(ry + 6), SC_FREQS[i].use);
        if(sel) canvas_set_color(canvas, ColorBlack);
    }
}

static bool freq_input(InputEvent* ev, void* ctx) {
    ScApp* app = ctx;
    if(ev->type != InputTypeShort) return false;
    switch(ev->key) {
    case InputKeyUp:
        with_view_model(app->freq_view, FreqViewModel* m, {
            if(m->sel > 0) { m->sel--; app->freq_idx = m->sel; }
        }, true);
        return true;
    case InputKeyDown:
        with_view_model(app->freq_view, FreqViewModel* m, {
            if(m->sel < SC_FREQ_COUNT - 1) { m->sel++; app->freq_idx = m->sel; }
        }, true);
        return true;
    case InputKeyOk:
        view_dispatcher_send_custom_event(
            app->view_dispatcher, ScEventFreqSelected);
        return true;
    case InputKeyBack:
        view_dispatcher_send_custom_event(app->view_dispatcher, ScEventBack);
        return true;
    default:
        return false;
    }
}

void sc_scene_freq_on_enter(void* ctx) {
    ScApp* app = ctx;

    view_set_draw_callback(app->freq_view,  freq_draw);
    view_set_input_callback(app->freq_view, freq_input);
    view_set_context(app->freq_view, app);

    with_view_model(app->freq_view, FreqViewModel* m, {
        m->sel = app->freq_idx;
    }, true);

    view_dispatcher_switch_to_view(app->view_dispatcher, ScViewFreq);
}

bool sc_scene_freq_on_event(void* ctx, SceneManagerEvent ev) {
    ScApp* app = ctx;
    if(ev.type == SceneManagerEventTypeCustom) {
        if(ev.event == ScEventFreqSelected) {
            scene_manager_next_scene(app->scene_manager, ScSceneCapture);
            return true;
        }
        if(ev.event == ScEventBack) {
            view_dispatcher_stop(app->view_dispatcher);
            return true;
        }
    }
    if(ev.type == SceneManagerEventTypeBack) return false;
    return false;
}

void sc_scene_freq_on_exit(void* ctx) {
    UNUSED(ctx);
}
