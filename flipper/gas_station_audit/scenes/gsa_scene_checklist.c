// Field Audit Checklist — on-device walkthrough for the auditor on-site

#include "../gas_station_audit.h"
#include "../gsa_storage.h"

// Every item your guy checks at the station
static const char* const GSA_CHECKLIST[] = {
    "1. BT scan all pumps (skimmer)",
    "2. NFC scan pump card readers",
    "3. Check card reader for overlay",
    "4. RF scan price sign freq",
    "5. Capture price update signal",
    "6. Replay test (owner watches)",
    "7. NFC tap loyalty/fleet cards",
    "8. Check pump cabinet locks",
    "9. Inspect Ethernet/serial ports",
    "10. Photo POS terminal model",
    "11. RF scan pump-to-POS freq",
    "12. Check WiFi SSIDs near POS",
    "13. Test NFC payment terminal",
    "14. Verify PIN pad tamper seal",
    "15. Document firmware versions",
};
#define GSA_CHECKLIST_COUNT 15

typedef struct {
    uint8_t scroll;
    bool    checked[GSA_CHECKLIST_COUNT];
    uint8_t done_count;
    char    station_id[16];  // auditor types on device
} ChecklistViewModel;

void gsa_checklist_view_draw(Canvas* canvas, void* model) {
    ChecklistViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Header
    canvas_draw_rbox(canvas, 0, 0, 128, 12, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    char hbuf[32];
    snprintf(hbuf, sizeof(hbuf), "AUDIT CHECKLIST %u/%u",
        m->done_count, GSA_CHECKLIST_COUNT);
    canvas_draw_str(canvas, 2, 9, hbuf);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontSecondary);

    // Show 4 items at a time
    for(uint8_t i = 0; i < 4; i++) {
        uint8_t idx = (m->scroll + i) % GSA_CHECKLIST_COUNT;
        uint8_t y = 14 + i * 12;
        bool current = (i == 0);

        if(current) canvas_draw_frame(canvas, 0, y - 1, 128, 11);

        // Checkbox
        canvas_draw_frame(canvas, 2, y, 9, 9);
        if(m->checked[idx])
            canvas_draw_str(canvas, 3, y + 7, "X");

        // Item text (truncate to fit)
        char truncated[28];
        strncpy(truncated, GSA_CHECKLIST[idx], 27);
        truncated[27] = '\0';
        canvas_draw_str(canvas, 14, y + 7, truncated);
    }

    // Progress bar
    uint8_t prog = (uint8_t)((m->done_count * 124UL) / GSA_CHECKLIST_COUNT);
    canvas_draw_frame(canvas, 2, 62, 124, 4);
    if(prog > 0) canvas_draw_box(canvas, 2, 62, prog, 4);
}

bool gsa_checklist_view_input(InputEvent* ev, void* ctx) {
    GasStationAuditApp* app = ctx;
    if(ev->type != InputTypeShort) return false;

    with_view_model(app->report_view, ChecklistViewModel* m, {
        if(ev->key == InputKeyDown)
            m->scroll = (m->scroll + 1) % GSA_CHECKLIST_COUNT;
        if(ev->key == InputKeyUp)
            m->scroll = (m->scroll + GSA_CHECKLIST_COUNT - 1) % GSA_CHECKLIST_COUNT;
        if(ev->key == InputKeyOk) {
            uint8_t idx = m->scroll;
            m->checked[idx] = !m->checked[idx];
            m->done_count = 0;
            for(uint8_t i = 0; i < GSA_CHECKLIST_COUNT; i++)
                if(m->checked[i]) m->done_count++;

            // Auto-save progress to encrypted store
            char line[GSA_LOG_LINE_MAX];
            char ts[24]; gsa_get_timestamp(ts);
            snprintf(line, sizeof(line),
                "CHECKLIST|%s|item:%u|%s|%s",
                ts, idx + 1, GSA_CHECKLIST[idx],
                m->checked[idx] ? "DONE" : "UNDONE");
            gsa_write_encrypted_record(app->storage, line);

            // Buzz on complete
            if(m->done_count == GSA_CHECKLIST_COUNT)
                notification_message(app->notifications, &sequence_success);
        }
    }, true);

    if(ev->key == InputKeyBack) {
        view_dispatcher_send_custom_event(app->view_dispatcher, GsaEventBack);
        return true;
    }
    return true;
}

// ── Scene wrappers ────────────────────────────────────────────────────────────
void gsa_scene_checklist_on_enter(void* ctx) {
    GasStationAuditApp* app = ctx;
    with_view_model(app->report_view, ChecklistViewModel* m, {
        memset(m, 0, sizeof(ChecklistViewModel));
        snprintf(m->station_id, sizeof(m->station_id), "STATION-01");
    }, false);
    view_set_draw_callback(app->report_view, gsa_checklist_view_draw);
    view_set_input_callback(app->report_view, gsa_checklist_view_input);
    view_dispatcher_switch_to_view(app->view_dispatcher, GsaViewReport);
}

bool gsa_scene_checklist_on_event(void* ctx, SceneManagerEvent ev) {
    GasStationAuditApp* app = ctx;
    if(ev.type == SceneManagerEventTypeCustom && ev.event == GsaEventBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    if(ev.type == SceneManagerEventTypeBack) return false;
    return false;
}

void gsa_scene_checklist_on_exit(void* ctx) {
    GasStationAuditApp* app = ctx;
    // Restore default report callbacks
    view_set_draw_callback(app->report_view, gsa_report_view_draw);
    view_set_input_callback(app->report_view, gsa_report_view_input);
}
