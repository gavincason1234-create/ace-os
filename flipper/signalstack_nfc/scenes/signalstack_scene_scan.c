// Scan scene — NFC polling for ALL 13.56 MHz protocols
// Protocols: ISO 14443-3A/B, ISO 14443-4A, ISO 15693, FeliCa,
//            MIFARE Classic, MIFARE Ultralight, DESFire

#include "signalstack_scene.h"
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
#include <nfc/protocols/iso15693_3/iso15693_3_poller.h>

// ── Scan view model ───────────────────────────────────────────────────────────
typedef struct {
    bool    scanning;
    bool    found;
    uint8_t pulse;       // animation counter
    char    proto[32];
    char    uid[32];
    char    extra[128];
} ScanViewModel;

// ── Draw callback ─────────────────────────────────────────────────────────────
void signalstack_scan_view_draw(Canvas* canvas, void* model) {
    ScanViewModel* vm = model;
    canvas_clear(canvas);

    // Header bar
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 5, AlignCenter, AlignTop, "SignalStack Scan");
    canvas_draw_line(canvas, 0, 13, 128, 13);

    if(!vm->found) {
        // Animated scanning indicator
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignTop, "Scanning 13.56 MHz...");

        // Pulse circle animation (3 rings)
        uint8_t r = (vm->pulse % 12) + 4;
        canvas_draw_circle(canvas, 64, 50, r);
        if(r > 8)  canvas_draw_circle(canvas, 64, 50, r - 6);
        if(r > 14) canvas_draw_circle(canvas, 64, 50, r - 12);

        canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignTop,
            "[Back] to exit");
    } else {
        // Tag found — show result
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 18, AlignCenter, AlignTop, "TAG FOUND!");

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 30, "Proto:");
        canvas_draw_str(canvas, 38, 30, vm->proto);

        canvas_draw_str(canvas, 2, 40, "UID:");
        canvas_draw_str(canvas, 38, 40, vm->uid);

        if(vm->extra[0]) {
            canvas_draw_str(canvas, 2, 50, vm->extra);
        }

        canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignTop,
            "[OK] Save  [Back] Scan again");
    }
}

// ── Input callback ────────────────────────────────────────────────────────────
bool signalstack_scan_view_input(InputEvent* event, void* ctx) {
    SignalStackApp* app = ctx;
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyOk) {
            view_dispatcher_send_custom_event(app->view_dispatcher, SignalStackEventTagFound);
            return true;
        }
    }
    return false;
}

// ── NFC poller callback ───────────────────────────────────────────────────────

// Fills result from a generic NfcDevice after polling
static void signalstack_fill_result_from_device(
    SignalStackApp* app,
    NfcProtocol     proto) {

    SignalStackScanResult* r = &app->last_scan;
    memset(r, 0, sizeof(*r));

    signalstack_get_timestamp(r->timestamp, &r->unix_ts);

    const NfcDeviceData* data = nfc_device_get_data(app->nfc_device, proto);

    switch(proto) {
    case NfcProtocolIso14443_3a: {
        r->protocol = TagProtoISO14443A;
        const Iso14443_3aData* d = data;
        memcpy(r->uid, d->uid, d->uid_len);
        r->uid_len = d->uid_len;
        snprintf(r->extra, sizeof(r->extra),
            "ATQA:%02X%02X SAK:%02X",
            d->atqa[0], d->atqa[1], d->sak);
        break;
    }
    case NfcProtocolIso14443_3b: {
        r->protocol = TagProtoISO14443B;
        const Iso14443_3bData* d = data;
        memcpy(r->uid, d->uid, 4);
        r->uid_len = 4;
        snprintf(r->extra, sizeof(r->extra), "ISO14443-B");
        break;
    }
    case NfcProtocolIso14443_4a: {
        r->protocol = TagProtoISO14443_4A;
        // Underlying 14443-3a data
        const Iso14443_4aData* d = data;
        memcpy(r->uid, d->iso14443_3a_data->uid, d->iso14443_3a_data->uid_len);
        r->uid_len = d->iso14443_3a_data->uid_len;
        snprintf(r->extra, sizeof(r->extra), "T=CL ATS present");
        break;
    }
    case NfcProtocolIso15693_3: {
        r->protocol = TagProtoISO15693;
        const Iso15693_3Data* d = data;
        // UID is 8 bytes, LSB first per ISO 15693
        memcpy(r->uid, d->uid, 8);
        r->uid_len = 8;
        snprintf(r->extra, sizeof(r->extra),
            "DSFID:%02X AFI:%02X Blks:%u/%uB",
            d->system_info.dsfid,
            d->system_info.afi,
            d->system_info.block_count,
            d->system_info.block_size);
        break;
    }
    case NfcProtocolFelica: {
        r->protocol = TagProtoFeliCa;
        const FelicaData* d = data;
        memcpy(r->uid, d->idm, 8);
        r->uid_len = 8;
        snprintf(r->extra, sizeof(r->extra),
            "PMm:%02X%02X%02X%02X%02X%02X%02X%02X",
            d->pmm[0], d->pmm[1], d->pmm[2], d->pmm[3],
            d->pmm[4], d->pmm[5], d->pmm[6], d->pmm[7]);
        break;
    }
    case NfcProtocolMfClassic: {
        r->protocol = TagProtoMifareClassic;
        const MfClassicData* d = data;
        memcpy(r->uid, d->iso14443_3a_data->uid, d->iso14443_3a_data->uid_len);
        r->uid_len = d->iso14443_3a_data->uid_len;
        const char* type_str = (d->type == MfClassicType4k) ? "4K" :
                               (d->type == MfClassicType2k) ? "2K" : "1K";
        snprintf(r->extra, sizeof(r->extra), "MIFARE Classic %s", type_str);
        break;
    }
    case NfcProtocolMfUltralight: {
        r->protocol = TagProtoMifareUltralight;
        const MfUltralightData* d = data;
        memcpy(r->uid, d->iso14443_3a_data->uid, d->iso14443_3a_data->uid_len);
        r->uid_len = d->iso14443_3a_data->uid_len;
        snprintf(r->extra, sizeof(r->extra),
            "MF Ultralight type=%u pages=%u",
            (unsigned)d->type, (unsigned)d->pages_read);
        break;
    }
    case NfcProtocolMfDesfire: {
        r->protocol = TagProtoDESFire;
        const MfDesfireData* d = data;
        memcpy(r->uid, d->iso14443_4a_data->iso14443_3a_data->uid,
               d->iso14443_4a_data->iso14443_3a_data->uid_len);
        r->uid_len = d->iso14443_4a_data->iso14443_3a_data->uid_len;
        snprintf(r->extra, sizeof(r->extra),
            "DESFire apps:%u",
            (unsigned)simple_array_get_count(d->applications));
        break;
    }
    default:
        r->protocol = TagProtoUnknown;
        snprintf(r->extra, sizeof(r->extra), "Unknown proto %d", proto);
        break;
    }

    snprintf(r->proto_name, sizeof(r->proto_name), "%s", signalstack_proto_name(r->protocol));
    signalstack_uid_to_str(r->uid, r->uid_len, r->uid_str);
    app->session_scans++;
    app->total_scans++;
}

// NFC poller run callback — called by NFC worker thread
static NfcCommand signalstack_poller_callback(NfcGenericEvent event, void* ctx) {
    SignalStackApp* app = ctx;
    NfcEvent*       nfc_event = event.event_data;

    if(nfc_event->type == NfcEventTypeFieldOff) {
        // Card removed — keep scanning
        return NfcCommandContinue;
    }

    if(nfc_event->type == NfcEventTypeDeviceDetected) {
        NfcProtocol detected_proto = nfc_device_get_protocol(app->nfc_device);
        signalstack_fill_result_from_device(app, detected_proto);
        app->scan_found = true;

        // Notify the scan scene
        view_dispatcher_send_custom_event(app->view_dispatcher, SignalStackEventTagFound);

        // Auto-save
        if(app->auto_save) {
            signalstack_save_to_log(app, &app->last_scan);
            if(app->save_raw_nfc) signalstack_save_nfc_file(app, &app->last_scan);
            app->last_scan.saved = true;
        }

        // Notification
        if(app->vibrate_on_scan) {
            notification_message(app->notifications, &sequence_set_vibro_on);
            furi_delay_ms(80);
            notification_message(app->notifications, &sequence_reset_vibro);
        }
        if(app->beep_on_scan) {
            notification_message(app->notifications, &sequence_success);
        }

        return NfcCommandStop;
    }

    return NfcCommandContinue;
}

// ── Pulse animation timer ─────────────────────────────────────────────────────
static void scan_pulse_timer(void* ctx) {
    SignalStackApp* app = ctx;
    with_view_model(
        app->scan_view,
        ScanViewModel * vm,
        { vm->pulse = (vm->pulse + 1) % 24; },
        true);
}

// ── Scene on_enter ────────────────────────────────────────────────────────────
void signalstack_scene_scan_on_enter(void* ctx) {
    SignalStackApp* app = ctx;
    app->scan_found = false;

    // Reset view model
    with_view_model(
        app->scan_view,
        ScanViewModel * vm,
        {
            vm->scanning = true;
            vm->found    = false;
            vm->pulse    = 0;
            vm->proto[0] = 0;
            vm->uid[0]   = 0;
            vm->extra[0] = 0;
        },
        true);

    view_dispatcher_switch_to_view(app->view_dispatcher, SignalStackViewScan);

    // Start NFC polling — auto-detect all protocols
    app->poller = nfc_poller_alloc(app->nfc, NfcProtocolIso14443_3a);
    nfc_poller_start(app->poller, signalstack_poller_callback, app);

    // Pulse animation timer (100 ms)
    FuriTimer* timer = furi_timer_alloc(scan_pulse_timer, FuriTimerTypePeriodic, app);
    furi_timer_start(timer, 100);
    scene_manager_set_scene_state(app->scene_manager, SignalStackSceneScan,
        (uint32_t)(uintptr_t)timer);
}

// ── Scene on_event ────────────────────────────────────────────────────────────
bool signalstack_scene_scan_on_event(void* ctx, SceneManagerEvent event) {
    SignalStackApp* app = ctx;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SignalStackEventTagFound && app->scan_found) {
            // Update view model with found tag
            with_view_model(
                app->scan_view,
                ScanViewModel * vm,
                {
                    vm->found    = true;
                    vm->scanning = false;
                    snprintf(vm->proto, sizeof(vm->proto), "%s", app->last_scan.proto_name);
                    snprintf(vm->uid,   sizeof(vm->uid),   "%s", app->last_scan.uid_str);
                    snprintf(vm->extra, sizeof(vm->extra), "%s", app->last_scan.extra);
                },
                true);

            // Push to history
            if(app->history_count < SIGNALSTACK_MAX_HISTORY) {
                app->history[app->history_count++] = app->last_scan;
            } else {
                // Ring buffer — overwrite oldest
                memmove(&app->history[0], &app->history[1],
                    sizeof(SignalStackScanResult) * (SIGNALSTACK_MAX_HISTORY - 1));
                app->history[SIGNALSTACK_MAX_HISTORY - 1] = app->last_scan;
            }
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

// ── Scene on_exit ─────────────────────────────────────────────────────────────
void signalstack_scene_scan_on_exit(void* ctx) {
    SignalStackApp* app = ctx;

    // Stop pulse timer
    FuriTimer* timer = (FuriTimer*)(uintptr_t)
        scene_manager_get_scene_state(app->scene_manager, SignalStackSceneScan);
    if(timer) {
        furi_timer_stop(timer);
        furi_timer_free(timer);
    }

    // Stop NFC
    if(app->poller) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
        app->poller = NULL;
    }
}
