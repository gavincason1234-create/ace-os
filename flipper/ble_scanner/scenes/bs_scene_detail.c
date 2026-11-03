// BLE Scanner — detail scene
// Full forensic view of a single device:
//   MAC address (colon-separated), type (FIXED/RANDOM),
//   manufacturer, category, RSSI/peak, seen count, connectable,
//   and privacy verdict.

#include "../ble_scanner.h"

typedef struct {
    char    mac_str[18];      // "AA:BB:CC:DD:EE:FF"
    char    mac_type_str[8];  // "FIXED" or "RANDOM"
    char    mfr[BS_MFR_LEN];
    char    category[16];
    char    name[BS_NAME_LEN];
    int8_t  rssi;
    int8_t  rssi_peak;
    uint32_t seen_count;
    bool    connectable;
    bool    trackable;        // true = public MAC
    uint8_t bars;
    uint8_t tick;
} DetailViewModel;

static void detail_sync(BsApp* app) {
    if(app->detail_idx >= app->device_count) return;
    const BsDevice* d = &app->devices[app->detail_idx];

    with_view_model(app->detail_view, DetailViewModel* m, {
        snprintf(m->mac_str, sizeof(m->mac_str),
                 "%02X:%02X:%02X:%02X:%02X:%02X",
                 d->addr[0], d->addr[1], d->addr[2],
                 d->addr[3], d->addr[4], d->addr[5]);
        strncpy(m->mac_type_str,
                (d->mac_type == BsMacPublic) ? "FIXED" : "RANDOM", 7);
        strncpy(m->mfr,      d->mfr[0]  ? d->mfr  : "Unknown",  BS_MFR_LEN - 1);
        strncpy(m->category, bs_category_name(d->category), 15);
        strncpy(m->name,     d->name[0]  ? d->name : "(no name)", BS_NAME_LEN - 1);
        m->rssi       = d->rssi;
        m->rssi_peak  = d->rssi_peak;
        m->seen_count = d->seen_count;
        m->connectable= d->connectable;
        m->trackable  = (d->mac_type == BsMacPublic);
        m->bars       = bs_rssi_bars(d->rssi);
    }, true);
}

static void detail_draw(Canvas* canvas, void* model) {
    DetailViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Header
    canvas_draw_rbox(canvas, 0, 0, 128, 11, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, "DEVICE FORENSICS");
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontSecondary);

    // Name (primary label)
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 22, m->name);
    canvas_set_font(canvas, FontSecondary);

    // MAC + type
    canvas_draw_str(canvas, 2, 31, m->mac_str);
    // MAC type badge on right
    if(m->trackable) {
        canvas_draw_box(canvas, 95, 24, 32, 9);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, 97, 31, "FIXED");
        canvas_set_color(canvas, ColorBlack);
    } else {
        canvas_draw_frame(canvas, 91, 24, 36, 9);
        canvas_draw_str(canvas, 93, 31, "RANDOM");
    }

    // Manufacturer + category
    canvas_draw_str(canvas, 2, 40, m->mfr);
    canvas_draw_str(canvas, 2, 49, m->category);

    // RSSI / peak
    char rssi_str[22];
    snprintf(rssi_str, sizeof(rssi_str), "RSSI %ddBm  pk %ddBm",
             (int)m->rssi, (int)m->rssi_peak);
    canvas_draw_str(canvas, 2, 58, rssi_str);

    // Connectable flag
    if(m->connectable)
        canvas_draw_str(canvas, 96, 49, "[CONN]");

    // Seen count + blink
    char seen[12];
    snprintf(seen, sizeof(seen), "%lu adv", (unsigned long)m->seen_count);
    canvas_draw_str(canvas, 96, 40, seen);

    // Privacy verdict bar at bottom
    const char* verdict = m->trackable
        ? "TRACKABLE: fixed MAC"
        : "PRIVATE: random MAC";
    canvas_draw_line(canvas, 0, 60, 127, 60);
    if(m->trackable) {
        if(m->tick & 1) {
            canvas_draw_box(canvas, 0, 61, 128, 3);
        }
    }
    canvas_draw_str(canvas, 2, 63, verdict);
}

static bool detail_input(InputEvent* ev, void* ctx) {
    BsApp* app = ctx;
    if(ev->type != InputTypeShort) return false;
    if(ev->key == InputKeyBack) {
        view_dispatcher_send_custom_event(app->view_dispatcher, BsEventBack);
        return true;
    }
    if(ev->key == InputKeyLeft) {
        if(app->detail_idx > 0) {
            app->detail_idx--;
            detail_sync(app);
        }
        return true;
    }
    if(ev->key == InputKeyRight) {
        if(app->detail_idx < app->device_count - 1) {
            app->detail_idx++;
            detail_sync(app);
        }
        return true;
    }
    return false;
}

static FuriTimer* s_detail_timer = NULL;

static void detail_tick_cb(void* ctx) {
    BsApp* app = ctx;
    // Keep RSSI fresh from live table
    detail_sync(app);
    with_view_model(app->detail_view, DetailViewModel* m, { m->tick++; }, true);
}

void bs_scene_detail_on_enter(void* ctx) {
    BsApp* app = ctx;

    view_set_draw_callback(app->detail_view,  detail_draw);
    view_set_input_callback(app->detail_view, detail_input);
    view_set_context(app->detail_view, app);

    detail_sync(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, BsViewDetail);

    s_detail_timer = furi_timer_alloc(detail_tick_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(s_detail_timer, 500);
}

bool bs_scene_detail_on_event(void* ctx, SceneManagerEvent ev) {
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

void bs_scene_detail_on_exit(void* ctx) {
    UNUSED(ctx);
    if(s_detail_timer) {
        furi_timer_stop(s_detail_timer);
        furi_timer_free(s_detail_timer);
        s_detail_timer = NULL;
    }
}
