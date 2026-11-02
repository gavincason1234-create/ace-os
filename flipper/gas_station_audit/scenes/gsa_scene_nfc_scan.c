// NFC/RFID Fleet Card Scanner — reads and grades any card near a pump

#include "../gas_station_audit.h"
#include "../gsa_storage.h"

typedef struct {
    uint8_t  tick;
    bool     scanning;
    bool     found;
    char     proto[16];
    char     uid[32];
    char     info[128];
    char     grade[4];       // "A"-"F" + null
    bool     cloneable;
    uint8_t  pulse;          // animation
} NfcViewModel;

// ── NFC poller callback ───────────────────────────────────────────────────────
static NfcCommand nfc_scan_callback(NfcEvent event, void* ctx) {
    GasStationAuditApp* app = ctx;

    if(event.type == NfcEventTypeFieldOn) {
        with_view_model(app->nfc_view, NfcViewModel*, m, {
            m->scanning = true;
            m->pulse    = (m->pulse + 1) & 0x0F;
        }, true);
    }

    if(event.type == NfcEventTypePollerReady) {
        // We get here when a tag is detected — read base poller data
        // Check protocol from the poller type
        if(app->nfc_count < GSA_MAX_NFC_SCANS) {
            GsaNfcResult* r = &app->nfc_results[app->nfc_count];
            memset(r, 0, sizeof(GsaNfcResult));

            // Try ISO 14443-3A (most contactless payment + fleet cards)
            const Iso14443_3aData* a_data =
                (const Iso14443_3aData*)nfc_poller_get_data(app->nfc_poller);
            if(a_data) {
                r->proto   = GsaNfcProtoISO14443A;
                r->uid_len = iso14443_3a_get_uid(a_data, r->uid, GSA_UID_MAX_LEN);
                gsa_uid_to_str(r->uid, r->uid_len, r->uid_str);

                uint8_t sak = iso14443_3a_get_sak(a_data);
                // Classify card type from SAK
                if(sak == 0x00) {
                    // MIFARE Ultralight / NTAG — no crypto
                    snprintf(r->info, sizeof(r->info),
                        "MIFARE Ultralight/NTAG\nNo encryption. UID cloneable.");
                    r->cloneable = true;
                } else if(sak == 0x08 || sak == 0x18) {
                    snprintf(r->info, sizeof(r->info),
                        "MIFARE Classic %s\nWeak CRYPTO1 cipher.\nKnown cracked - clone in <1min.",
                        sak == 0x08 ? "1K" : "4K");
                    r->cloneable = true;
                } else if(sak == 0x20) {
                    snprintf(r->info, sizeof(r->info),
                        "ISO 14443-4 / DESFire\nAES encryption present.\nHigh security.");
                    r->cloneable = false;
                } else if(sak == 0x28) {
                    snprintf(r->info, sizeof(r->info),
                        "MIFARE Classic (emulated)\nSAK:0x28 - potential magic card.\nClone likely.");
                    r->cloneable = true;
                } else {
                    snprintf(r->info, sizeof(r->info),
                        "ISO 14443-3A SAK:0x%02X\nUnknown subtype.", sak);
                    r->cloneable = false;
                }

                bool has_crypto = !r->cloneable;
                r->grade = gsa_nfc_security_grade(r->proto, has_crypto);

                app->nfc_count++;

                with_view_model(app->nfc_view, NfcViewModel*, m, {
                    m->found = true;
                    m->scanning = false;
                    strncpy(m->proto, "ISO14443-A", 15);
                    strncpy(m->uid, r->uid_str, 31);
                    strncpy(m->info, r->info, 127);
                    m->grade[0] = r->grade; m->grade[1] = '\0';
                    m->cloneable = r->cloneable;
                }, true);

                notification_message(app->notifications,
                    r->cloneable ? &sequence_error : &sequence_success);

                // Save to encrypted log
                char line[GSA_LOG_LINE_MAX];
                char ts[24]; gsa_get_timestamp(ts);
                snprintf(line, sizeof(line),
                    "NFC|%s|ISO14443-A|%s|SAK:%02X|grade:%c|cloneable:%s",
                    ts, r->uid_str, sak, r->grade,
                    r->cloneable ? "YES" : "NO");
                gsa_write_encrypted_record(app->storage, line);
            }
        }
        return NfcCommandContinue;
    }

    if(event.type == NfcEventTypeFieldOff) {
        with_view_model(app->nfc_view, NfcViewModel*, m, {
            m->scanning = false;
        }, true);
    }

    return NfcCommandContinue;
}

// ── View draw ─────────────────────────────────────────────────────────────────
void gsa_nfc_view_draw(Canvas* canvas, void* model) {
    NfcViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Header
    canvas_draw_rbox(canvas, 0, 0, 128, 12, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 9, "NFC/RFID FLEET CARD SCAN");
    canvas_set_color(canvas, ColorBlack);

    if(!m->found) {
        // Pulsing NFC ring animation
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 25, AlignCenter, AlignTop, "HOLD CARD NEAR");
        canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignTop, "FLIPPER...");
        // Concentric circles animate with pulse
        uint8_t p = m->pulse;
        canvas_draw_circle(canvas, 64, 48, 4 + (p % 8));
        canvas_draw_circle(canvas, 64, 48, 8 + (p % 8));
        canvas_draw_circle(canvas, 64, 48, 12 + (p % 8));
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 60, AlignCenter, AlignTop,
            m->scanning ? "FIELD ON" : "SCANNING...");
        return;
    }

    // Found — show card details
    canvas_set_font(canvas, FontSecondary);

    // Grade box — top right
    char grade_str[8];
    snprintf(grade_str, sizeof(grade_str), " %s ", m->grade);
    if(m->cloneable) {
        canvas_draw_rbox(canvas, 96, 14, 30, 14, 2);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, 98, 24, "CLONE");
        canvas_set_color(canvas, ColorBlack);
    } else {
        canvas_draw_str(canvas, 98, 24, "SECURE");
    }

    canvas_draw_str(canvas, 2, 22, m->proto);
    canvas_draw_str(canvas, 2, 31, m->uid);

    // Info (first line only — space limited)
    char first_line[42] = {0};
    const char* nl = strchr(m->info, '\n');
    size_t fl = nl ? (size_t)(nl - m->info) : strlen(m->info);
    if(fl > 41) fl = 41;
    memcpy(first_line, m->info, fl);
    canvas_draw_str(canvas, 2, 40, first_line);

    if(nl) {
        char second_line[42] = {0};
        const char* nl2 = strchr(nl + 1, '\n');
        size_t sl = nl2 ? (size_t)(nl2 - nl - 1) : strlen(nl + 1);
        if(sl > 41) sl = 41;
        memcpy(second_line, nl + 1, sl);
        canvas_draw_str(canvas, 2, 49, second_line);
    }

    canvas_draw_str(canvas, 2, 61, "[OK]next  [BACK]done");
}

bool gsa_nfc_view_input(InputEvent* ev, void* ctx) {
    GasStationAuditApp* app = ctx;
    if(ev->type == InputTypeShort) {
        if(ev->key == InputKeyOk) {
            // Reset found state for next card
            with_view_model(app->nfc_view, NfcViewModel*, m, {
                m->found = false; m->scanning = true;
            }, true);
            return true;
        }
        if(ev->key == InputKeyBack) {
            view_dispatcher_send_custom_event(app->view_dispatcher, GsaEventBack);
            return true;
        }
    }
    return false;
}

// ── Scene ─────────────────────────────────────────────────────────────────────
static FuriTimer* nfc_anim_timer = NULL;
static void nfc_anim_cb(void* ctx) {
    GasStationAuditApp* app = ctx;
    with_view_model(app->nfc_view, NfcViewModel*, m, {
        m->pulse = (m->pulse + 1) & 0x0F;
        m->tick++;
    }, true);
}

void gsa_scene_nfc_scan_on_enter(void* ctx) {
    GasStationAuditApp* app = ctx;
    app->nfc_scanning = true;

    with_view_model(app->nfc_view, NfcViewModel*, m, {
        memset(m, 0, sizeof(NfcViewModel));
        m->scanning = true;
    }, false);

    // Start ISO 14443-3A poller
    app->nfc_poller = nfc_poller_alloc(app->nfc, NfcProtocolIso14443_3a);
    nfc_poller_start(app->nfc_poller, nfc_scan_callback, app);

    // Animation timer
    nfc_anim_timer = furi_timer_alloc(nfc_anim_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(nfc_anim_timer, 150);

    view_dispatcher_switch_to_view(app->view_dispatcher, GsaViewNfc);
}

bool gsa_scene_nfc_scan_on_event(void* ctx, SceneManagerEvent ev) {
    GasStationAuditApp* app = ctx;
    if(ev.type == SceneManagerEventTypeCustom && ev.event == GsaEventBack) {
        app->nfc_scanning = false;
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    if(ev.type == SceneManagerEventTypeBack) {
        app->nfc_scanning = false;
        return false;
    }
    return false;
}

void gsa_scene_nfc_scan_on_exit(void* ctx) {
    GasStationAuditApp* app = ctx;
    app->nfc_scanning = false;
    if(nfc_anim_timer) {
        furi_timer_stop(nfc_anim_timer);
        furi_timer_free(nfc_anim_timer);
        nfc_anim_timer = NULL;
    }
    if(app->nfc_poller) {
        nfc_poller_stop(app->nfc_poller);
        nfc_poller_free(app->nfc_poller);
        app->nfc_poller = NULL;
    }
    gsa_build_report(app);
}
