// Scan scene — cycles ISO 14443-3A → ISO 15693-3 to cover all retail tags

#include "signalstack_scene.h"
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
#include <nfc/protocols/iso15693_3/iso15693_3_poller.h>

// ── Scan view model ───────────────────────────────────────────────────────────
typedef struct {
    bool    found;
    uint8_t pulse;
    char    proto[32];
    char    uid[32];
    char    extra[128];
} ScanViewModel;

// ── Draw ──────────────────────────────────────────────────────────────────────
void signalstack_scan_view_draw(Canvas* canvas, void* model) {
    ScanViewModel* vm = model;
    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 5, AlignCenter, AlignTop, "SignalStack Scan");
    canvas_draw_line(canvas, 0, 13, 128, 13);

    if(!vm->found) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignTop,
            "Scanning 13.56 MHz...");

        uint8_t r = (vm->pulse % 12) + 4;
        canvas_draw_circle(canvas, 64, 48, r);
        if(r > 8)  canvas_draw_circle(canvas, 64, 48, r - 6);
        if(r > 14) canvas_draw_circle(canvas, 64, 48, r - 12);

        canvas_draw_str_aligned(canvas, 64, 60, AlignCenter, AlignTop, "[Back] exit");
    } else {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 17, AlignCenter, AlignTop, "TAG FOUND!");

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 29, "Proto:");
        canvas_draw_str(canvas, 40, 29, vm->proto);

        canvas_draw_str(canvas, 2, 39, "UID:");
        canvas_draw_str(canvas, 40, 39, vm->uid);

        if(vm->extra[0])
            canvas_draw_str(canvas, 2, 49, vm->extra);

        canvas_draw_str_aligned(canvas, 64, 60, AlignCenter, AlignTop,
            "[Back] scan again");
    }
}

bool signalstack_scan_view_input(InputEvent* event, void* ctx) {
    UNUSED(event);
    UNUSED(ctx);
    return false;
}

// ── Helpers ───────────────────────────────────────────────────────────────────

// Identify MIFARE sub-type from ATQA + SAK
static const char* mifare_type_from_atqa_sak(
    uint8_t atqa0, uint8_t atqa1, uint8_t sak) {
    UNUSED(atqa1);
    if(sak == 0x08 && atqa0 == 0x04) return "MIFARE Classic 1K";
    if(sak == 0x18 && atqa0 == 0x02) return "MIFARE Classic 4K";
    if(sak == 0x00 && atqa0 == 0x44) return "MIFARE Ultralight";
    if(sak == 0x20)                  return "ISO14443-4 / DESFire";
    if(sak == 0x28)                  return "MIFARE Classic 1K (emulated)";
    if(sak == 0x38)                  return "MIFARE Classic 4K (emulated)";
    return "ISO14443-A";
}

static void on_tag_found(SignalStackApp* app) {
    app->scan_found = true;
    app->session_scans++;
    app->total_scans++;

    // Push to history ring buffer
    if(app->history_count < SIGNALSTACK_MAX_HISTORY) {
        app->history[app->history_count++] = app->last_scan;
    } else {
        memmove(&app->history[0], &app->history[1],
            sizeof(SignalStackScanResult) * (SIGNALSTACK_MAX_HISTORY - 1));
        app->history[SIGNALSTACK_MAX_HISTORY - 1] = app->last_scan;
    }

    if(app->auto_save) {
        signalstack_save_to_log(app, &app->last_scan);
        if(app->save_raw_nfc) signalstack_save_nfc_file(app, &app->last_scan);
        app->last_scan.saved = true;
    }

    if(app->vibrate_on_scan) {
        notification_message(app->notifications, &sequence_set_vibro_on);
        furi_delay_ms(80);
        notification_message(app->notifications, &sequence_reset_vibro);
    }
    if(app->beep_on_scan)
        notification_message(app->notifications, &sequence_success);

    view_dispatcher_send_custom_event(
        app->view_dispatcher, SignalStackEventTagFound);
}

// ── ISO 14443-3A poller callback ──────────────────────────────────────────────
static NfcCommand poller_14443a_cb(NfcGenericEvent event, void* ctx) {
    SignalStackApp* app = ctx;
    furi_assert(event.protocol == NfcProtocolIso14443_3a);

    Iso14443_3aPollerEvent* e = event.event_data;
    if(e->type == Iso14443_3aPollerEventTypeReady) {
        const Iso14443_3aData* d = iso14443_3a_poller_get_data(event.instance);

        SignalStackScanResult* r = &app->last_scan;
        memset(r, 0, sizeof(*r));
        signalstack_get_timestamp(r->timestamp, &r->unix_ts);

        r->uid_len = d->uid_len;
        memcpy(r->uid, d->uid, d->uid_len);
        signalstack_uid_to_str(r->uid, r->uid_len, r->uid_str);

        const char* type = mifare_type_from_atqa_sak(d->atqa[0], d->atqa[1], d->sak);
        snprintf(r->proto_name, sizeof(r->proto_name), "%s", type);
        r->protocol = TagProtoISO14443A;

        snprintf(r->extra, sizeof(r->extra),
            "ATQA:%02X%02X SAK:%02X", d->atqa[0], d->atqa[1], d->sak);

        on_tag_found(app);
        return NfcCommandStop;
    }
    return NfcCommandContinue;
}

// ── ISO 15693-3 poller callback (retail/vicinity tags) ────────────────────────
static NfcCommand poller_15693_cb(NfcGenericEvent event, void* ctx) {
    SignalStackApp* app = ctx;
    furi_assert(event.protocol == NfcProtocolIso15693_3);

    Iso15693_3PollerEvent* e = event.event_data;
    if(e->type == Iso15693_3PollerEventTypeReady) {
        const Iso15693_3Data* d = iso15693_3_poller_get_data(event.instance);

        SignalStackScanResult* r = &app->last_scan;
        memset(r, 0, sizeof(*r));
        signalstack_get_timestamp(r->timestamp, &r->unix_ts);

        r->uid_len = ISO15693_3_UID_SIZE; // 8
        memcpy(r->uid, d->uid, ISO15693_3_UID_SIZE);
        signalstack_uid_to_str(r->uid, r->uid_len, r->uid_str);

        snprintf(r->proto_name, sizeof(r->proto_name), "ISO15693");
        r->protocol = TagProtoISO15693;

        snprintf(r->extra, sizeof(r->extra),
            "DSFID:%02X AFI:%02X Blks:%u/%uB",
            d->system_info.dsfid,
            d->system_info.afi,
            d->system_info.block_count,
            d->system_info.block_size);

        on_tag_found(app);
        return NfcCommandStop;
    }
    return NfcCommandContinue;
}

// ── Protocol cycling — start next poller ─────────────────────────────────────
static void start_poller(SignalStackApp* app) {
    if(app->poller) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
        app->poller = NULL;
    }

    // Alternate: 0 = 14443-3A, 1 = 15693
    uint32_t idx = scene_manager_get_scene_state(
        app->scene_manager, SignalStackSceneScan) & 0xFF;

    if(idx == 0) {
        app->poller = nfc_poller_alloc(app->nfc, NfcProtocolIso14443_3a);
        nfc_poller_start(app->poller, poller_14443a_cb, app);
    } else {
        app->poller = nfc_poller_alloc(app->nfc, NfcProtocolIso15693_3);
        nfc_poller_start(app->poller, poller_15693_cb, app);
    }

    // Toggle for next call, preserve timer pointer in upper bits
    uint32_t state = scene_manager_get_scene_state(
        app->scene_manager, SignalStackSceneScan);
    scene_manager_set_scene_state(
        app->scene_manager, SignalStackSceneScan,
        (state & 0xFFFFFF00) | ((idx ^ 1) & 0xFF));
}

// ── Pulse + protocol-cycle timer ──────────────────────────────────────────────
typedef struct {
    SignalStackApp* app;
    uint8_t         tick;
} ScanTimerCtx;

static void scan_timer_cb(void* ctx) {
    ScanTimerCtx* tc = ctx;
    SignalStackApp* app = tc->app;

    if(app->scan_found) return;

    tc->tick++;

    // Pulse animation
    with_view_model(
        app->scan_view,
        ScanViewModel* vm,
        { vm->pulse = (vm->pulse + 1) % 24; },
        true);

    // Switch protocol every 20 ticks (2s at 100ms interval)
    if(tc->tick % 20 == 0) {
        start_poller(app);
    }
}

// ── Scene on_enter ────────────────────────────────────────────────────────────
void signalstack_scene_scan_on_enter(void* ctx) {
    SignalStackApp* app = ctx;
    app->scan_found = false;

    with_view_model(
        app->scan_view,
        ScanViewModel* vm,
        {
            vm->found  = false;
            vm->pulse  = 0;
            vm->proto[0] = 0;
            vm->uid[0]   = 0;
            vm->extra[0] = 0;
        },
        true);

    view_dispatcher_switch_to_view(app->view_dispatcher, SignalStackViewScan);

    // Reset protocol index (stored in low byte of scene state)
    scene_manager_set_scene_state(app->scene_manager, SignalStackSceneScan, 0);

    // Allocate timer context on heap (freed on exit)
    ScanTimerCtx* tc = malloc(sizeof(ScanTimerCtx));
    tc->app  = app;
    tc->tick = 0;

    FuriTimer* timer = furi_timer_alloc(scan_timer_cb, FuriTimerTypePeriodic, tc);
    furi_timer_start(timer, 100);

    // Store timer ptr in scene state upper bits (this is a common Flipper pattern)
    // We use a separate pointer store since scene state is only 32-bit
    app->scan_timer_ctx = tc;
    app->scan_timer     = timer;

    start_poller(app);
}

// ── Scene on_event ────────────────────────────────────────────────────────────
bool signalstack_scene_scan_on_event(void* ctx, SceneManagerEvent event) {
    SignalStackApp* app = ctx;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom &&
       event.event == SignalStackEventTagFound) {

        with_view_model(
            app->scan_view,
            ScanViewModel* vm,
            {
                vm->found = true;
                snprintf(vm->proto, sizeof(vm->proto), "%s", app->last_scan.proto_name);
                snprintf(vm->uid,   sizeof(vm->uid),   "%s", app->last_scan.uid_str);
                snprintf(vm->extra, sizeof(vm->extra), "%s", app->last_scan.extra);
            },
            true);
        consumed = true;

    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

// ── Scene on_exit ─────────────────────────────────────────────────────────────
void signalstack_scene_scan_on_exit(void* ctx) {
    SignalStackApp* app = ctx;

    if(app->scan_timer) {
        furi_timer_stop(app->scan_timer);
        furi_timer_free(app->scan_timer);
        app->scan_timer = NULL;
    }
    if(app->scan_timer_ctx) {
        free(app->scan_timer_ctx);
        app->scan_timer_ctx = NULL;
    }
    if(app->poller) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
        app->poller = NULL;
    }
}
