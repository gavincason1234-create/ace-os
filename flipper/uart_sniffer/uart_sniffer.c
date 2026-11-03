// UART Debug Port Sniffer — passive serial monitor on GPIO pins
// Company: SignalStack  |  Authorized demonstrations only.
// Demonstrates that IoT devices routinely expose UART debug ports outputting
// boot logs, shell prompts, and plaintext credentials at 115200 baud.
// Connect Flipper GPIO pin 13 (RX) to target TX. No data is transmitted.
//
// Wiring:
//   Flipper GPIO 13 (RX) → Target UART TX
//   Flipper GND          → Target GND
//   Do NOT connect power unless using the same supply.

#include "uart_sniffer.h"

// ─────────────────────────────────────────────────────────────────────────────
// UART ISR callback — called from interrupt context, must be very fast
// Push bytes into the ring buffer; dispatch event to main thread when ready.
// ─────────────────────────────────────────────────────────────────────────────

static UsApp* s_isr_app = NULL;

static void us_uart_irq_cb(UartIrqEvent ev, uint8_t byte, void* ctx) {
    UsApp* app = ctx;
    if(ev != UartIrqEventRXNE) return;

    uint16_t next = (uint16_t)((app->buf_head + 1) % US_BYTE_BUF);
    if(next != app->buf_tail) {
        app->byte_buf[app->buf_head] = byte;
        app->buf_head = next;
    }
    // Signal main thread every 16 bytes to avoid flooding
    if((app->buf_head % 16) == 0) {
        view_dispatcher_send_custom_event(app->view_dispatcher, UsEventDataReady);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// UART start / stop
// ─────────────────────────────────────────────────────────────────────────────

void us_uart_start(UsApp* app) {
    if(app->uart_active) return;
    s_isr_app = app;
    furi_hal_uart_init(FuriHalUartIdUSART1, US_BAUDS[app->baud_idx]);
    furi_hal_uart_set_irq_cb(FuriHalUartIdUSART1, us_uart_irq_cb, app);
    app->uart_active = true;
}

void us_uart_stop(UsApp* app) {
    if(!app->uart_active) return;
    furi_hal_uart_set_irq_cb(FuriHalUartIdUSART1, NULL, NULL);
    furi_hal_uart_deinit(FuriHalUartIdUSART1);
    app->uart_active  = false;
    s_isr_app = NULL;
}

// ─────────────────────────────────────────────────────────────────────────────
// Process bytes from ring buffer into terminal lines
// Called from main thread (sniff scene tick or event handler).
// ─────────────────────────────────────────────────────────────────────────────

static void us_flush_line(UsApp* app) {
    if(app->partial_len == 0) return;
    app->partial[app->partial_len] = '\0';

    UsLine* line = &app->lines[app->line_head % US_TOTAL_LINES];
    strncpy(line->text, app->partial, US_COLS);
    line->text[US_COLS] = '\0';
    line->len   = app->partial_len;
    line->alert = us_is_alert(line->text);
    if(line->alert) {
        app->alert_count++;
    }
    app->line_head++;
    if(app->line_count < US_TOTAL_LINES) app->line_count++;
    app->partial_len = 0;
    app->partial[0]  = '\0';
}

void us_process_bytes(UsApp* app) {
    while(app->buf_tail != app->buf_head) {
        uint8_t byte = app->byte_buf[app->buf_tail];
        app->buf_tail = (uint16_t)((app->buf_tail + 1) % US_BYTE_BUF);
        app->total_bytes++;

        if(app->display_mode == UsDisplayAscii) {
            if(byte == '\n' || byte == '\r') {
                if(app->partial_len > 0) us_flush_line(app);
            } else if(byte >= 0x20 && byte < 0x7F) {
                // Printable ASCII
                if(app->partial_len < US_COLS - 1) {
                    app->partial[app->partial_len++] = (char)byte;
                } else {
                    us_flush_line(app);
                    app->partial[app->partial_len++] = (char)byte;
                }
            } else {
                // Non-printable: encode as [XX]
                char hex[5];
                snprintf(hex, sizeof(hex), "[%02X]", byte);
                if(app->partial_len + 4 >= US_COLS) us_flush_line(app);
                for(uint8_t i = 0; hex[i] && app->partial_len < US_COLS - 1; i++)
                    app->partial[app->partial_len++] = hex[i];
            }
        } else {
            // Hex mode: "XX " per byte, wrap at US_COLS
            char hex[4];
            snprintf(hex, sizeof(hex), "%02X ", byte);
            if(app->partial_len + 3 >= US_COLS) us_flush_line(app);
            for(uint8_t i = 0; hex[i] && app->partial_len < US_COLS - 1; i++)
                app->partial[app->partial_len++] = hex[i];
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Clear terminal
// ─────────────────────────────────────────────────────────────────────────────

void us_clear(UsApp* app) {
    app->line_head    = 0;
    app->line_count   = 0;
    app->partial_len  = 0;
    app->total_bytes  = 0;
    app->alert_count  = 0;
    memset(app->lines, 0, sizeof(app->lines));
}

// ─────────────────────────────────────────────────────────────────────────────
// App lifecycle
// ─────────────────────────────────────────────────────────────────────────────

static UsApp* us_alloc(void) {
    UsApp* app = malloc(sizeof(UsApp));
    memset(app, 0, sizeof(UsApp));

    app->gui           = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->scene_manager = scene_manager_alloc(&us_scene_handlers, app);
    app->baud_idx      = US_BAUD_DEFAULT;
    app->display_mode  = UsDisplayAscii;

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(app->view_dispatcher);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    app->baud_view = view_alloc();
    view_allocate_model(app->baud_view, ViewModelTypeLocking, 256);
    view_dispatcher_add_view(app->view_dispatcher, UsViewBaud, app->baud_view);

    app->sniff_view = view_alloc();
    view_allocate_model(app->sniff_view, ViewModelTypeLocking, 512);
    view_dispatcher_add_view(app->view_dispatcher, UsViewSniff, app->sniff_view);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui,
                                  ViewDispatcherTypeFullscreen);
    return app;
}

static void us_free(UsApp* app) {
    us_uart_stop(app);

    view_dispatcher_remove_view(app->view_dispatcher, UsViewBaud);
    view_dispatcher_remove_view(app->view_dispatcher, UsViewSniff);
    view_free(app->baud_view);
    view_free(app->sniff_view);
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    free(app);
}

int32_t uart_sniffer_app(void* p) {
    UNUSED(p);
    UsApp* app = us_alloc();
    scene_manager_next_scene(app->scene_manager, UsSceneBaud);
    view_dispatcher_run(app->view_dispatcher);
    us_free(app);
    return 0;
}
