// BLE Scanner — scan scene
// Shows a live sorted device list with RSSI bars, MAC type, category tag, and name.
// Up/Down scroll, OK=detail, Right=stats, Back=exit.

#include "../ble_scanner.h"

#define SCAN_ROWS 4        // devices visible at once
#define ROW_H     12       // px per row
#define HDR_H     11       // header height

typedef struct {
    // Snapshot of up to BS_DEVICE_MAX entries for rendering
    struct {
        char     label[24];   // "PHN [PUB] Apple          "
        int8_t   rssi;
        uint8_t  bars;        // 0-4
        bool     is_public;
    } rows[BS_DEVICE_MAX];
    uint8_t  count;
    uint8_t  sel;
    uint32_t total_seen;
    uint8_t  tick;
    bool     scanning;
} ScanViewModel;

// Sync device table → view model (called under lock or from main thread)
static void scan_sync(BsApp* app) {
    with_view_model(app->scan_view, ScanViewModel* m, {
        m->count      = app->device_count;
        m->sel        = app->list_sel;
        m->total_seen = app->total_seen;
        m->scanning   = app->scanning;

        for(uint8_t i = 0; i < app->device_count && i < BS_DEVICE_MAX; i++) {
            const BsDevice* d = &app->devices[i];
            const char* cat  = bs_category_icon(d->category);
            const char* mac  = (d->mac_type == BsMacPublic) ? "PUB" : "RND";
            // Display: name if known, else manufacturer, else MAC fallback
            char label[24];
            if(d->name[0]) {
                snprintf(label, sizeof(label), "%s", d->name);
            } else if(d->mfr[0]) {
                snprintf(label, sizeof(label), "%s", d->mfr);
            } else {
                snprintf(label, sizeof(label), "%02X:%02X:%02X",
                         d->addr[0], d->addr[1], d->addr[2]);
            }
            snprintf(m->rows[i].label, sizeof(m->rows[i].label),
                     "[%s][%s] %s", cat, mac, label);
            m->rows[i].rssi      = d->rssi;
            m->rows[i].bars      = bs_rssi_bars(d->rssi);
            m->rows[i].is_public = (d->mac_type == BsMacPublic);
        }
    }, true);
}

// Draw 4 signal-strength bars (4 px each, heights 2/4/6/8)
static void draw_rssi_bars(Canvas* canvas, uint8_t x, uint8_t y, uint8_t bars) {
    for(uint8_t b = 0; b < 4; b++) {
        uint8_t h  = (uint8_t)(2 + b * 2);
        uint8_t bx = (uint8_t)(x + b * 4);
        uint8_t by = (uint8_t)(y - h + 1);
        if(b < bars)
            canvas_draw_box(canvas, bx, by, 3, h);
        else
            canvas_draw_frame(canvas, bx, by, 3, h);
    }
}

static void scan_draw(Canvas* canvas, void* model) {
    ScanViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Header bar
    canvas_draw_rbox(canvas, 0, 0, 128, HDR_H, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, "BLE SCANNER");

    char hdr_r[16];
    if(m->scanning)
        snprintf(hdr_r, sizeof(hdr_r), "%u dev", (unsigned)m->count);
    else
        snprintf(hdr_r, sizeof(hdr_r), "PAUSED");
    canvas_draw_str(canvas, 88, 8, hdr_r);
    canvas_set_color(canvas, ColorBlack);

    if(m->count == 0) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignTop, "Scanning...");
        canvas_set_font(canvas, FontSecondary);
        // Animated dots
        const char* ani[] = {".", "..", "..."};
        canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignTop,
                                ani[m->tick % 3]);
        return;
    }

    // Clamp selection scroll: show SCAN_ROWS rows
    uint8_t top = 0;
    if(m->sel >= SCAN_ROWS) top = (uint8_t)(m->sel - SCAN_ROWS + 1);
    if(top + SCAN_ROWS > m->count) {
        top = (m->count >= SCAN_ROWS) ? (uint8_t)(m->count - SCAN_ROWS) : 0;
    }

    canvas_set_font(canvas, FontSecondary);
    for(uint8_t r = 0; r < SCAN_ROWS && (top + r) < m->count; r++) {
        uint8_t idx = (uint8_t)(top + r);
        uint8_t ry  = (uint8_t)(HDR_H + 1 + r * ROW_H);
        bool sel    = (idx == m->sel);

        if(sel) {
            canvas_draw_box(canvas, 0, ry - 1, 128, ROW_H);
            canvas_set_color(canvas, ColorWhite);
        }

        // RSSI bars at right edge (4 bars × 4px = 16px wide, leave 2px margin)
        draw_rssi_bars(canvas, 110, (uint8_t)(ry + ROW_H - 3), m->rows[idx].bars);

        // Public MAC indicator — small filled square for trackable
        if(m->rows[idx].is_public)
            canvas_draw_box(canvas, 103, ry, 4, 4);
        else
            canvas_draw_frame(canvas, 103, ry, 4, 4);

        // Device label — truncate to fit left of MAC icon
        char trunc[22];
        strncpy(trunc, m->rows[idx].label, 21);
        trunc[21] = '\0';
        canvas_draw_str(canvas, 2, (uint8_t)(ry + ROW_H - 3), trunc);

        if(sel) canvas_set_color(canvas, ColorBlack);
    }

    // Status bar
    canvas_draw_line(canvas, 0, 58, 127, 58);
    char stat[32];
    snprintf(stat, sizeof(stat), "%u adv  [OK]detail [>]stats",
             (unsigned)m->total_seen);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 63, stat);
}

static bool scan_input(InputEvent* ev, void* ctx) {
    BsApp* app = ctx;
    if(ev->type != InputTypeShort) return false;

    switch(ev->key) {
    case InputKeyBack:
        view_dispatcher_send_custom_event(app->view_dispatcher, BsEventBack);
        return true;
    case InputKeyUp:
        if(app->list_sel > 0) {
            app->list_sel--;
            scan_sync(app);
        }
        return true;
    case InputKeyDown:
        if(app->device_count > 0 && app->list_sel < app->device_count - 1) {
            app->list_sel++;
            scan_sync(app);
        }
        return true;
    case InputKeyOk:
        if(app->device_count > 0) {
            app->detail_idx = app->list_sel;
            view_dispatcher_send_custom_event(app->view_dispatcher, BsEventSelect);
        }
        return true;
    case InputKeyRight:
        view_dispatcher_send_custom_event(app->view_dispatcher, BsEventNext);
        return true;
    default:
        return false;
    }
}

// Tick timer — re-sort + redraw every 500ms
static FuriTimer* s_scan_timer = NULL;

static void scan_tick_cb(void* ctx) {
    BsApp* app = ctx;
    // Re-sort by RSSI then sync to view
    if(app->device_count > 1) {
        // Insertion sort — small list, called infrequently
        for(uint8_t i = 0; i < app->device_count - 1; i++) {
            for(uint8_t j = i + 1; j < app->device_count; j++) {
                if(app->devices[j].rssi > app->devices[i].rssi) {
                    BsDevice tmp = app->devices[i];
                    app->devices[i] = app->devices[j];
                    app->devices[j] = tmp;
                }
            }
        }
    }
    scan_sync(app);
    with_view_model(app->scan_view, ScanViewModel* m, { m->tick++; }, true);
}

void bs_scene_scan_on_enter(void* ctx) {
    BsApp* app = ctx;

    view_set_draw_callback(app->scan_view,  scan_draw);
    view_set_input_callback(app->scan_view, scan_input);
    view_set_context(app->scan_view, app);

    // Start BLE scanning
    app->scanning = true;
    furi_hal_bt_extra_beacon_stop();  // ensure no TX
    furi_hal_bt_set_gap_event_callback(bs_gap_callback, app);

    scan_sync(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, BsViewScan);

    s_scan_timer = furi_timer_alloc(scan_tick_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(s_scan_timer, 500);
}

bool bs_scene_scan_on_event(void* ctx, SceneManagerEvent ev) {
    BsApp* app = ctx;
    if(ev.type == SceneManagerEventTypeCustom) {
        if(ev.event == BsEventDeviceFound) {
            scan_sync(app);
            return true;
        }
        if(ev.event == BsEventSelect) {
            scene_manager_next_scene(app->scene_manager, BsSceneDetail);
            return true;
        }
        if(ev.event == BsEventNext) {
            scene_manager_next_scene(app->scene_manager, BsSceneStats);
            return true;
        }
        if(ev.event == BsEventBack) {
            view_dispatcher_stop(app->view_dispatcher);
            return true;
        }
    }
    if(ev.type == SceneManagerEventTypeBack) return false;
    return false;
}

void bs_scene_scan_on_exit(void* ctx) {
    BsApp* app = ctx;
    if(s_scan_timer) {
        furi_timer_stop(s_scan_timer);
        furi_timer_free(s_scan_timer);
        s_scan_timer = NULL;
    }
    furi_hal_bt_set_gap_event_callback(NULL, NULL);
    app->scanning = false;
}
