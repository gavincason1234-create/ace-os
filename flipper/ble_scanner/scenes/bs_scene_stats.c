// BLE Scanner — statistics scene
// Aggregate view: total devices, advertisement count,
// public vs random MAC split, and category breakdown bar chart.

#include "../ble_scanner.h"

typedef struct {
    uint8_t  device_count;
    uint32_t total_seen;
    uint32_t public_count;
    uint32_t random_count;
    // Category bar chart: counts per category (BsDevCount entries)
    uint8_t  cat_counts[BsDevCount];
    uint8_t  tick;
} StatsViewModel;

static void stats_sync(BsApp* app) {
    with_view_model(app->stats_view, StatsViewModel* m, {
        m->device_count = app->device_count;
        m->total_seen   = app->total_seen;
        m->public_count = app->public_count;
        m->random_count = app->random_count;
        for(uint8_t i = 0; i < BsDevCount; i++)
            m->cat_counts[i] = app->category_counts[i];
    }, true);
}

static void stats_draw(Canvas* canvas, void* model) {
    StatsViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Header
    canvas_draw_rbox(canvas, 0, 0, 128, 11, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, "BLE STATS");
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontSecondary);

    // Row 1: device + adv counts
    char row1[28];
    snprintf(row1, sizeof(row1), "Devices: %u   Adv: %lu",
             (unsigned)m->device_count, (unsigned long)m->total_seen);
    canvas_draw_str(canvas, 2, 21, row1);

    // Row 2: MAC type split with visual bar
    uint32_t total = m->public_count + m->random_count;
    if(total == 0) total = 1;
    uint8_t pub_px = (uint8_t)(m->public_count * 60 / total);
    // Public bar (filled) + random bar (outline)
    canvas_draw_str(canvas, 2, 30, "PUB");
    canvas_draw_box(canvas, 20, 23, pub_px, 8);          // public portion
    canvas_draw_frame(canvas, 20, 23, 60, 8);            // full outline
    canvas_draw_str(canvas, 84, 30, "RND");

    char split[16];
    snprintf(split, sizeof(split), "%lu/%lu",
             (unsigned long)m->public_count,
             (unsigned long)m->random_count);
    canvas_draw_str(canvas, 94, 30, split);

    // Category breakdown — horizontal bars for each category
    // BsDevCount = 8 entries (Unknown + 7 categories)
    // Find max for normalization
    uint8_t max_cat = 1;
    for(uint8_t i = 1; i < BsDevCount; i++)
        if(m->cat_counts[i] > max_cat) max_cat = m->cat_counts[i];

    static const char* const CAT_ABBR[BsDevCount] = {
        "UNK", "PHN", "LAP", "EAR", "WCH", "BCN", "SNS", " TV"
    };

    uint8_t cy = 36;
    for(uint8_t i = 1; i < BsDevCount; i++) {  // skip Unknown
        uint8_t bar = (uint8_t)(m->cat_counts[i] * 48 / max_cat);
        canvas_draw_str(canvas, 2, (uint8_t)(cy + 5), CAT_ABBR[i]);
        if(bar > 0)
            canvas_draw_box(canvas, 20, cy, bar, 5);
        canvas_draw_frame(canvas, 20, cy, 48, 5);

        // count label
        if(m->cat_counts[i] > 0) {
            char cnt[4];
            snprintf(cnt, sizeof(cnt), "%u", (unsigned)m->cat_counts[i]);
            canvas_draw_str(canvas, 72, (uint8_t)(cy + 5), cnt);
        }
        cy += 7;
        if(cy > 60) break;
    }

    // Footer hint
    canvas_draw_line(canvas, 0, 60, 127, 60);
    canvas_draw_str(canvas, 0, 63, "[<]back");

    UNUSED(m->tick);
}

static bool stats_input(InputEvent* ev, void* ctx) {
    BsApp* app = ctx;
    if(ev->type != InputTypeShort) return false;
    if(ev->key == InputKeyBack || ev->key == InputKeyLeft) {
        view_dispatcher_send_custom_event(app->view_dispatcher, BsEventBack);
        return true;
    }
    return false;
}

static FuriTimer* s_stats_timer = NULL;

static void stats_tick_cb(void* ctx) {
    BsApp* app = ctx;
    stats_sync(app);
    with_view_model(app->stats_view, StatsViewModel* m, { m->tick++; }, true);
}

void bs_scene_stats_on_enter(void* ctx) {
    BsApp* app = ctx;

    view_set_draw_callback(app->stats_view,  stats_draw);
    view_set_input_callback(app->stats_view, stats_input);
    view_set_context(app->stats_view, app);

    stats_sync(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, BsViewStats);

    s_stats_timer = furi_timer_alloc(stats_tick_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(s_stats_timer, 800);
}

bool bs_scene_stats_on_event(void* ctx, SceneManagerEvent ev) {
    BsApp* app = ctx;
    if(ev.type == SceneManagerEventTypeCustom) {
        if(ev.event == BsEventBack) {
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
    }
    if(ev.type == SceneManagerEventTypeBack) return false;
    return false;
}

void bs_scene_stats_on_exit(void* ctx) {
    UNUSED(ctx);
    if(s_stats_timer) {
        furi_timer_stop(s_stats_timer);
        furi_timer_free(s_stats_timer);
        s_stats_timer = NULL;
    }
}
