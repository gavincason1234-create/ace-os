// Pump Threat Report — full security grade with hacker-terminal styling

#include "../gas_station_audit.h"
#include "../gsa_storage.h"

typedef struct {
    uint8_t tick;
    char    grade;
    uint8_t score;
    char    summary[256];
    bool    has_data;
} ReportViewModel;

static FuriTimer* report_anim_timer = NULL;
static void report_anim_cb(void* ctx) {
    GasStationAuditApp* app = ctx;
    with_view_model(app->report_view, ReportViewModel* m, {
        m->tick++;
    }, true);
}

void gsa_report_view_draw(Canvas* canvas, void* model) {
    ReportViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Header
    canvas_draw_rbox(canvas, 0, 0, 128, 12, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 9, "PUMP THREAT REPORT");
    canvas_set_color(canvas, ColorBlack);

    if(!m->has_data) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignTop, "NO DATA YET");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignTop, "Run BT + NFC + RF scans");
        canvas_draw_str_aligned(canvas, 64, 53, AlignCenter, AlignTop, "then return here.");
        return;
    }

    // Big grade — animated pulse when threat detected
    char grade_str[3] = {m->grade, '\0', '\0'};
    bool threat = (m->grade == 'D' || m->grade == 'F');

    if(threat && (m->tick & 4)) {
        // Invert box for threat blink
        canvas_draw_rbox(canvas, 0, 13, 30, 20, 3);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str(canvas, 6, 30, grade_str);
    canvas_set_color(canvas, ColorBlack);

    // Score bar
    char sbuf[16];
    snprintf(sbuf, sizeof(sbuf), "%u/100", m->score);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 34, 22, sbuf);
    // Score bar graphic
    uint8_t bar = (uint8_t)((m->score * 90UL) / 100);
    canvas_draw_frame(canvas, 34, 24, 90, 6);
    if(bar > 0) canvas_draw_box(canvas, 35, 25, bar > 88 ? 88 : bar, 4);

    // Summary text — up to 4 lines
    canvas_set_font(canvas, FontSecondary);
    const char* p = m->summary;
    uint8_t y = 35;
    while(*p && y < 63) {
        char line[36] = {0};
        const char* nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        if(len > 35) len = 35;
        memcpy(line, p, len);
        canvas_draw_str(canvas, 2, y, line);
        y += 9;
        p = nl ? nl + 1 : p + len;
        if(!nl) break;
    }

    canvas_draw_str(canvas, 2, 63, "[OK]save report  [BACK]menu");
}

bool gsa_report_view_input(InputEvent* ev, void* ctx) {
    GasStationAuditApp* app = ctx;
    if(ev->type == InputTypeShort) {
        if(ev->key == InputKeyOk) {
            // Save full report as one encrypted record
            char line[GSA_LOG_LINE_MAX];
            char ts[24]; gsa_get_timestamp(ts);
            snprintf(line, sizeof(line),
                "REPORT|%s|grade:%c|score:%u|bt_flag:%u|nfc_clone:%u|rf_sig:%u",
                ts,
                app->report.pump_grade,
                app->report.pump_score,
                app->report.bt_flagged_count,
                app->report.nfc_cloneable_count,
                app->report.rf_signals_count);
            gsa_write_encrypted_record(app->storage, line);
            notification_message(app->notifications, &sequence_success);
            return true;
        }
        if(ev->key == InputKeyBack) {
            view_dispatcher_send_custom_event(app->view_dispatcher, GsaEventBack);
            return true;
        }
    }
    return false;
}

void gsa_scene_report_on_enter(void* ctx) {
    GasStationAuditApp* app = ctx;

    if(!app->report_ready) gsa_build_report(app);

    with_view_model(app->report_view, ReportViewModel* m, {
        m->tick     = 0;
        m->has_data = app->report_ready;
        m->grade    = app->report.pump_grade;
        m->score    = app->report.pump_score;
        strncpy(m->summary, app->report.summary, 255);
    }, false);

    report_anim_timer = furi_timer_alloc(report_anim_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(report_anim_timer, 200);

    view_dispatcher_switch_to_view(app->view_dispatcher, GsaViewReport);
}

bool gsa_scene_report_on_event(void* ctx, SceneManagerEvent ev) {
    GasStationAuditApp* app = ctx;
    if(ev.type == SceneManagerEventTypeCustom && ev.event == GsaEventBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    if(ev.type == SceneManagerEventTypeBack) return false;
    return false;
}

void gsa_scene_report_on_exit(void* ctx) {
    UNUSED(ctx);
    if(report_anim_timer) {
        furi_timer_stop(report_anim_timer);
        furi_timer_free(report_anim_timer);
        report_anim_timer = NULL;
    }
}
