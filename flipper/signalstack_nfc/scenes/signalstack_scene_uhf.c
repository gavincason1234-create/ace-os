// UHF Bridge scene — reads EPC Gen2 tags forwarded from PC bridge via USART1

#include "signalstack_scene.h"
#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>

#define UHF_BAUD      115200
#define UHF_RING_SIZE 256
#define UHF_LINE_MAX  128
#define UHF_TIMER_MS  50

// ── UHF context (heap-allocated per scene entry) ─────────────────────────────
typedef struct {
    SignalStackApp*      app;
    FuriHalSerialHandle* serial;
    FuriTimer*           timer;
    // Ring buffer (ISR fills, timer drains)
    uint8_t              ring[UHF_RING_SIZE];
    volatile uint16_t    head;
    volatile uint16_t    tail;
    // Line assembler
    char                 line[UHF_LINE_MAX];
    uint8_t              line_len;
} UhfCtx;

// ── Draw ──────────────────────────────────────────────────────────────────────
static void uhf_view_draw(Canvas* canvas, void* model) {
    ScanViewModel* vm = model;
    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 5, AlignCenter, AlignTop, "UHF Bridge Scan");
    canvas_draw_line(canvas, 0, 13, 128, 13);

    if(!vm->found) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignTop,
            "Waiting for bridge...");
        canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignTop,
            "Run signalstack_bridge.py");

        uint8_t r = (vm->pulse % 12) + 4;
        canvas_draw_circle(canvas, 64, 50, r);
        if(r > 8)  canvas_draw_circle(canvas, 64, 50, r - 6);

        canvas_draw_str_aligned(canvas, 64, 60, AlignCenter, AlignTop, "[Back] exit");
    } else {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 17, AlignCenter, AlignTop, "UHF TAG FOUND!");

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 29, "Proto:");
        canvas_draw_str(canvas, 40, 29, vm->proto);

        canvas_draw_str(canvas, 2, 39, "EPC:");
        canvas_draw_str(canvas, 28, 39, vm->uid);

        if(vm->extra[0])
            canvas_draw_str(canvas, 2, 49, vm->extra);

        canvas_draw_str_aligned(canvas, 64, 60, AlignCenter, AlignTop,
            "[Back] scan again");
    }
}

static bool uhf_view_input(InputEvent* event, void* ctx) {
    UNUSED(event);
    UNUSED(ctx);
    return false;
}

// ── Serial RX ISR — push bytes into ring buffer only ─────────────────────────
static void uhf_serial_rx_cb(
    FuriHalSerialHandle* handle,
    FuriHalSerialRxEvent ev,
    void* ctx) {
    UhfCtx* uc = ctx;
    if(ev != FuriHalSerialRxEventData) return;

    uint8_t b = furi_hal_serial_async_rx(handle);
    uint16_t next = (uc->head + 1u) % UHF_RING_SIZE;
    if(next != uc->tail) {
        uc->ring[uc->head] = b;
        uc->head = next;
    }
}

// ── Timer — drain ring, parse lines, fire events ─────────────────────────────
static void uhf_timer_cb(void* ctx) {
    UhfCtx* uc = ctx;
    SignalStackApp* app = uc->app;

    // Pulse animation
    if(!app->scan_found) {
        with_view_model(
            app->uhf_view,
            ScanViewModel* vm,
            { vm->pulse = (vm->pulse + 1) % 24; },
            true);
    }

    // Drain ring buffer byte by byte
    while(uc->tail != uc->head) {
        uint8_t b = uc->ring[uc->tail];
        uc->tail = (uc->tail + 1u) % UHF_RING_SIZE;

        if(b == '\n' || b == '\r' || uc->line_len >= UHF_LINE_MAX - 1) {
            if(uc->line_len == 0) continue;
            uc->line[uc->line_len] = '\0';
            uc->line_len = 0;

            // Parse "UHF,<EPC>,<RSSI>,<ANT>"
            if(strncmp(uc->line, "UHF,", 4) != 0) continue;

            char    epc[32] = {0};
            int     rssi    = 0;
            int     ant     = 0;
            int     parsed  = sscanf(uc->line + 4, "%31[^,],%d,%d", epc, &rssi, &ant);
            if(parsed < 1 || epc[0] == '\0') continue;

            SignalStackScanResult* r = &app->last_scan;
            memset(r, 0, sizeof(*r));
            signalstack_get_timestamp(r->timestamp, &r->unix_ts);

            r->protocol = TagProtoISO18000_6C;
            snprintf(r->proto_name, sizeof(r->proto_name), "UHF EPC Gen2");
            snprintf(r->uid_str, sizeof(r->uid_str), "%s", epc);

            // Convert hex EPC string to bytes (best-effort)
            size_t hex_len = strlen(epc);
            r->uid_len = (uint8_t)MIN(hex_len / 2, SIGNALSTACK_UID_MAX_LEN);
            for(uint8_t i = 0; i < r->uid_len; i++) {
                char byte_str[3] = {epc[i * 2], epc[i * 2 + 1], '\0'};
                r->uid[i] = (uint8_t)strtol(byte_str, NULL, 16);
            }

            snprintf(r->extra, sizeof(r->extra), "RSSI:%ddBm ANT:%d", rssi, ant);
            r->saved = false;

            app->scan_found = true;
            app->session_scans++;
            app->total_scans++;

            if(app->history_count < SIGNALSTACK_MAX_HISTORY) {
                app->history[app->history_count++] = *r;
            } else {
                memmove(&app->history[0], &app->history[1],
                    (SIGNALSTACK_MAX_HISTORY - 1) * sizeof(SignalStackScanResult));
                app->history[SIGNALSTACK_MAX_HISTORY - 1] = *r;
            }

            if(app->auto_save) {
                signalstack_save_to_log(app, r);
                if(app->save_raw_nfc) signalstack_save_nfc_file(app, r);
                r->saved = true;
            }
            if(app->beep_on_scan)
                notification_message(app->notifications, &sequence_success);

            view_dispatcher_send_custom_event(
                app->view_dispatcher, SignalStackEventTagFound);
        } else if(b >= 0x20 && b < 0x7F) {
            uc->line[uc->line_len++] = (char)b;
        }
    }
}

// ── Scene on_enter ────────────────────────────────────────────────────────────
void signalstack_scene_uhf_on_enter(void* ctx) {
    SignalStackApp* app = ctx;
    app->scan_found = false;

    with_view_model(
        app->uhf_view,
        ScanViewModel* vm,
        {
            vm->found    = false;
            vm->pulse    = 0;
            vm->proto[0] = '\0';
            vm->uid[0]   = '\0';
            vm->extra[0] = '\0';
        },
        true);

    view_dispatcher_switch_to_view(app->view_dispatcher, SignalStackViewUhf);

    // Allocate and init context
    UhfCtx* uc = malloc(sizeof(UhfCtx));
    memset(uc, 0, sizeof(UhfCtx));
    uc->app = app;
    app->uhf_ctx = uc;

    // Acquire USART1 (GPIO pins 13/14)
    uc->serial = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    furi_hal_serial_init(uc->serial, UHF_BAUD);
    furi_hal_serial_async_rx_start(uc->serial, uhf_serial_rx_cb, uc, false);

    // Start poll timer
    uc->timer = furi_timer_alloc(uhf_timer_cb, FuriTimerTypePeriodic, uc);
    furi_timer_start(uc->timer, UHF_TIMER_MS);
}

// ── Scene on_event ────────────────────────────────────────────────────────────
bool signalstack_scene_uhf_on_event(void* ctx, SceneManagerEvent event) {
    SignalStackApp* app = ctx;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom &&
       event.event == SignalStackEventTagFound) {

        with_view_model(
            app->uhf_view,
            ScanViewModel* vm,
            {
                vm->found = true;
                snprintf(vm->proto, sizeof(vm->proto), "%s", app->last_scan.proto_name);
                snprintf(vm->uid,   sizeof(vm->uid),   "%s", app->last_scan.uid_str);
                snprintf(vm->extra, sizeof(vm->extra), "%.*s",
                    (int)(sizeof(vm->extra) - 1), app->last_scan.extra);
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
void signalstack_scene_uhf_on_exit(void* ctx) {
    SignalStackApp* app = ctx;
    UhfCtx* uc = app->uhf_ctx;
    if(!uc) return;

    furi_timer_stop(uc->timer);
    furi_timer_free(uc->timer);

    furi_hal_serial_async_rx_stop(uc->serial);
    furi_hal_serial_deinit(uc->serial);
    furi_hal_serial_control_release(uc->serial);

    free(uc);
    app->uhf_ctx = NULL;
}
