// Sub-GHz RF Signal Monitor — detects pump-to-controller traffic + pager decode

#include "../gas_station_audit.h"
#include "../gsa_storage.h"

// CC1101 OOK 650kHz async preset — addr/val pairs, 0x00 0x00 terminated
// Matches FuriHalSubGhzPresetOok650Async from firmware source
static const uint8_t gsa_ook650_preset[] = {
    0x02, 0x0D,  0x08, 0x32,  0x0B, 0x06,
    0x10, 0x67,  0x11, 0x32,  0x12, 0x30,
    0x13, 0x00,  0x14, 0x00,  0x17, 0x00,
    0x18, 0x18,  0x19, 0x16,  0x1B, 0x07,
    0x1C, 0x00,  0x1D, 0x91,  0x20, 0xFB,
    0x21, 0xB6,  0x22, 0x11,  0x00, 0x00,
};

// Gas pump Sub-GHz frequencies to sweep
// Sources: industry docs, FCC ID database for pump controller models
static const uint32_t GSA_PUMP_FREQS[] = {
    315000000,   // 315 MHz  — North America fixed-code remotes
    433920000,   // 433.92 MHz — EU/universal OOK (most common pump controllers)
    434000000,   // 434 MHz
    868350000,   // 868.35 MHz — EU Sub-GHz
    902000000,   // 902 MHz   — US ISM band
    915000000,   // 915 MHz   — US pump-to-POS wireless
    928000000,   // 928 MHz
    152000000,   // 152 MHz   — POCSAG pager (hospitals/businesses)
    157450000,   // 157.45 MHz — common US pager
    462562500,   // 462.5625 MHz — FRS/GMRS (some older pump intercoms)
};
#define GSA_PUMP_FREQ_COUNT 10

typedef struct {
    uint8_t  tick;
    uint8_t  sweep_idx;          // which frequency we're on
    bool     monitoring;
    uint8_t  signal_count;
    // Last detected signal
    uint32_t last_freq;
    int8_t   last_rssi;
    char     last_note[32];
    // Bar graph — RSSI per frequency slot
    int8_t   rssi_bars[GSA_PUMP_FREQ_COUNT];
} RfViewModel;

static FuriTimer* rf_timer = NULL;

static void rf_timer_cb(void* ctx) {
    GasStationAuditApp* app = ctx;
    app->scan_tick++;

    // CC1101 RSSI read at current frequency
    // furi_hal_subghz_get_rssi() returns current RSSI in dBm
    int8_t rssi = furi_hal_subghz_get_rssi();

    with_view_model(app->rf_view, RfViewModel* m, {
        m->tick++;
        m->rssi_bars[m->sweep_idx] = rssi;

        // Threshold: signal > -90 dBm considered "active"
        if(rssi > -90) {
            m->last_freq  = GSA_PUMP_FREQS[m->sweep_idx];
            m->last_rssi  = rssi;
            m->signal_count++;

            // Classify frequency
            uint32_t f = GSA_PUMP_FREQS[m->sweep_idx];
            if(f == 152000000 || f == 157450000)
                snprintf(m->last_note, sizeof(m->last_note), "PAGER SIGNAL");
            else if(f == 433920000 || f == 434000000)
                snprintf(m->last_note, sizeof(m->last_note), "433MHz OOK CTRL");
            else if(f >= 900000000)
                snprintf(m->last_note, sizeof(m->last_note), "ISM/PUMP CTRL");
            else if(f == 315000000)
                snprintf(m->last_note, sizeof(m->last_note), "315MHz REMOTE");
            else
                snprintf(m->last_note, sizeof(m->last_note), "UNKNOWN RF");

            // Log to encrypted store
            if(app->rf_event_count < GSA_MAX_RF_EVENTS) {
                GsaRfEvent* e = &app->rf_events[app->rf_event_count++];
                e->frequency = f;
                e->rssi      = rssi;
                e->timestamp = app->scan_tick;
                strncpy(e->note, m->last_note, 31);

                char line[GSA_LOG_LINE_MAX];
                char ts[24]; gsa_get_timestamp(ts);
                snprintf(line, sizeof(line),
                    "RF|%s|%luHz|%ddBm|%s",
                    ts, (unsigned long)f, rssi, m->last_note);
                gsa_write_encrypted_record(app->storage, line);

                notification_message(app->notifications, &sequence_blink_red_10);
            }
        }

        // Sweep to next frequency every 3 ticks (300ms per channel)
        if((app->scan_tick % 3) == 0) {
            m->sweep_idx = (m->sweep_idx + 1) % GSA_PUMP_FREQ_COUNT;
            // Retune CC1101
            furi_hal_subghz_set_frequency_and_path(GSA_PUMP_FREQS[m->sweep_idx]);
        }
    }, true);
}

void gsa_rf_view_draw(Canvas* canvas, void* model) {
    RfViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Header
    canvas_draw_rbox(canvas, 0, 0, 128, 12, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 9, "RF SIGNAL MONITOR");
    if(m->monitoring && (m->tick & 4))
        canvas_draw_str(canvas, 98, 9, "[SCAN]");
    canvas_set_color(canvas, ColorBlack);

    // Current frequency
    char buf[48];
    uint32_t f = GSA_PUMP_FREQS[m->sweep_idx % GSA_PUMP_FREQ_COUNT];
    uint32_t mhz  = f / 1000000;
    uint32_t frac = (f % 1000000) / 1000;
    snprintf(buf, sizeof(buf), "FREQ: %lu.%03lu MHz", (unsigned long)mhz, (unsigned long)frac);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 22, buf);

    // Signals found counter
    snprintf(buf, sizeof(buf), "SIGNALS: %u", m->signal_count);
    canvas_draw_str(canvas, 80, 22, buf);

    // Spectrum bar chart — 10 frequency bars
    for(uint8_t i = 0; i < GSA_PUMP_FREQ_COUNT; i++) {
        int8_t r = m->rssi_bars[i];
        // Map -120..-40 dBm → 0..16 px height
        int h = (r + 120) / 5;
        if(h < 0) h = 0;
        if(h > 16) h = 16;
        uint8_t x = 4 + i * 12;
        // Highlight current frequency
        if(i == m->sweep_idx % GSA_PUMP_FREQ_COUNT) {
            canvas_draw_rbox(canvas, x - 1, 26, 10, 18, 1);
            canvas_set_color(canvas, ColorWhite);
        }
        if(h > 0) canvas_draw_box(canvas, x, 43 - h, 8, h);
        canvas_set_color(canvas, ColorBlack);
    }
    // Axis
    canvas_draw_line(canvas, 2, 44, 126, 44);

    // Last signal details
    if(m->signal_count > 0) {
        uint32_t lmhz = m->last_freq / 1000000;
        char sigbuf[64];
        snprintf(sigbuf, sizeof(sigbuf), "! %s %luM %ddBm",
            m->last_note, (unsigned long)lmhz, m->last_rssi);
        canvas_draw_str(canvas, 2, 54, sigbuf);
    } else {
        canvas_draw_str(canvas, 2, 54, "No active signals yet...");
    }

    canvas_draw_str(canvas, 2, 63, "[OK]save  [BACK]stop");
}

bool gsa_rf_view_input(InputEvent* ev, void* ctx) {
    GasStationAuditApp* app = ctx;
    if(ev->type == InputTypeShort) {
        if(ev->key == InputKeyOk) {
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

void gsa_scene_rf_monitor_on_enter(void* ctx) {
    GasStationAuditApp* app = ctx;
    app->rf_event_count = 0;
    app->rf_monitoring  = true;
    app->scan_tick      = 0;

    with_view_model(app->rf_view, RfViewModel* m, {
        memset(m, 0, sizeof(RfViewModel));
        m->monitoring = true;
    }, false);

    // Init CC1101 Sub-GHz
    furi_hal_subghz_reset();
    furi_hal_subghz_load_custom_preset(gsa_ook650_preset);
    furi_hal_subghz_set_frequency_and_path(GSA_PUMP_FREQS[0]);
    furi_hal_subghz_rx();

    rf_timer = furi_timer_alloc(rf_timer_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(rf_timer, 100);

    view_dispatcher_switch_to_view(app->view_dispatcher, GsaViewRf);
}

bool gsa_scene_rf_monitor_on_event(void* ctx, SceneManagerEvent ev) {
    GasStationAuditApp* app = ctx;
    if(ev.type == SceneManagerEventTypeCustom && ev.event == GsaEventBack) {
        app->rf_monitoring = false;
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    if(ev.type == SceneManagerEventTypeBack) {
        app->rf_monitoring = false;
        return false;
    }
    return false;
}

void gsa_scene_rf_monitor_on_exit(void* ctx) {
    GasStationAuditApp* app = ctx;
    app->rf_monitoring = false;
    if(rf_timer) {
        furi_timer_stop(rf_timer);
        furi_timer_free(rf_timer);
        rf_timer = NULL;
    }
    furi_hal_subghz_idle();
    furi_hal_subghz_sleep();
    gsa_build_report(app);
}
