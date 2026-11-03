// Sub-GHz Spectrum Analyzer — range selection scene
// Choose which frequency band to scan.
// Each preset shows freq range + what's typically there.

#include "../subghz_spectrum.h"

typedef struct {
    uint8_t sel;
} RangeViewModel;

static void range_draw(Canvas* canvas, void* model) {
    RangeViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Header
    canvas_draw_rbox(canvas, 0, 0, 128, 11, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, "SPECTRUM ANALYZER");
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 20, "Select frequency band:");

    // Range list
    for(uint8_t i = 0; i < SS_RANGE_COUNT; i++) {
        uint8_t ry  = (uint8_t)(24 + i * 10);
        bool    sel = (i == m->sel);

        if(sel) {
            canvas_draw_box(canvas, 0, ry - 1, 128, 10);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, 2, (uint8_t)(ry + 7), SS_RANGES[i].label);
        canvas_draw_str(canvas, 72, (uint8_t)(ry + 7), SS_RANGES[i].desc);
        if(sel) canvas_set_color(canvas, ColorBlack);
    }
}

static bool range_input(InputEvent* ev, void* ctx) {
    SsApp* app = ctx;
    if(ev->type != InputTypeShort) return false;
    switch(ev->key) {
    case InputKeyUp:
        with_view_model(app->range_view, RangeViewModel* m, {
            if(m->sel > 0) { m->sel--; app->range_idx = m->sel; }
        }, true);
        return true;
    case InputKeyDown:
        with_view_model(app->range_view, RangeViewModel* m, {
            if(m->sel < SS_RANGE_COUNT - 1) { m->sel++; app->range_idx = m->sel; }
        }, true);
        return true;
    case InputKeyOk:
        view_dispatcher_send_custom_event(app->view_dispatcher, SsEventRangeSelected);
        return true;
    case InputKeyBack:
        view_dispatcher_send_custom_event(app->view_dispatcher, SsEventBack);
        return true;
    default:
        return false;
    }
}

void ss_scene_range_on_enter(void* ctx) {
    SsApp* app = ctx;

    view_set_draw_callback(app->range_view,  range_draw);
    view_set_input_callback(app->range_view, range_input);
    view_set_context(app->range_view, app);

    with_view_model(app->range_view, RangeViewModel* m, {
        m->sel = app->range_idx;
    }, true);

    view_dispatcher_switch_to_view(app->view_dispatcher, SsViewRange);
}

bool ss_scene_range_on_event(void* ctx, SceneManagerEvent ev) {
    SsApp* app = ctx;
    if(ev.type == SceneManagerEventTypeCustom) {
        if(ev.event == SsEventRangeSelected) {
            scene_manager_next_scene(app->scene_manager, SsSceneSpectrum);
            return true;
        }
        if(ev.event == SsEventBack) {
            view_dispatcher_stop(app->view_dispatcher);
            return true;
        }
    }
    if(ev.type == SceneManagerEventTypeBack) return false;
    return false;
}

void ss_scene_range_on_exit(void* ctx) {
    UNUSED(ctx);
}
