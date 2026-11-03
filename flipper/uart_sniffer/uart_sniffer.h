#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_uart.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

// ─────────────────────────────────────────────────────────────────────────────
// Terminal ring buffer
// ─────────────────────────────────────────────────────────────────────────────

#define US_COLS         21    // chars per terminal line (128px / 6px per char)
#define US_LINES        4     // visible lines on screen
#define US_LINE_BUF     US_COLS + 1
#define US_TOTAL_LINES  32    // total ring-buffer lines (scroll history)
#define US_BYTE_BUF     256   // ISR → main thread byte queue

// Each line of terminal output
typedef struct {
    char     text[US_LINE_BUF];
    uint8_t  len;
    bool     alert;    // true = contains credential keyword → highlight
} UsLine;

// ─────────────────────────────────────────────────────────────────────────────
// Baud rate table
// ─────────────────────────────────────────────────────────────────────────────

#define US_BAUD_COUNT 9
static const uint32_t US_BAUDS[US_BAUD_COUNT] = {
    300, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200
};
// Default to 115200 (most common IoT debug port)
#define US_BAUD_DEFAULT 8  // index into US_BAUDS

// ─────────────────────────────────────────────────────────────────────────────
// Credential keyword detection
// ─────────────────────────────────────────────────────────────────────────────

// Strings that suggest a line contains a credential or shell prompt
#define US_KEYWORD_COUNT 10
static const char* const US_KEYWORDS[US_KEYWORD_COUNT] = {
    "password", "passwd", "login", "username", "user:",
    "pass:", "root", "admin", "token", "$",
};

static inline bool us_is_alert(const char* line) {
    for(uint8_t i = 0; i < US_KEYWORD_COUNT; i++) {
        // Case-insensitive substring search (manual — no strcasestr in Flipper libc)
        const char* kw = US_KEYWORDS[i];
        size_t kwlen = strlen(kw);
        size_t llen  = strlen(line);
        if(kwlen > llen) continue;
        for(size_t j = 0; j <= llen - kwlen; j++) {
            bool match = true;
            for(size_t k = 0; k < kwlen; k++) {
                char a = line[j + k];
                char b = kw[k];
                if(a >= 'A' && a <= 'Z') a = (char)(a + 32);
                if(a != b) { match = false; break; }
            }
            if(match) return true;
        }
    }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Display mode
// ─────────────────────────────────────────────────────────────────────────────

typedef enum {
    UsDisplayAscii = 0,  // printable chars, non-printable as [XX]
    UsDisplayHex,        // every byte as "XX " hex dump
} UsDisplayMode;

// ─────────────────────────────────────────────────────────────────────────────
// Scenes / Views
// ─────────────────────────────────────────────────────────────────────────────

typedef enum {
    UsSceneBaud = 0,
    UsSceneSniff,
    UsSceneCount,
} UsScene;

typedef enum {
    UsViewBaud = 0,
    UsViewSniff,
} UsView;

typedef enum {
    UsEventBaudSelected = 0x90,
    UsEventBack,
    UsEventDataReady,    // bytes available in queue
    UsEventToggleMode,
    UsEventClear,
} UsEvent;

// ─────────────────────────────────────────────────────────────────────────────
// App state
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    Gui*             gui;
    NotificationApp* notifications;
    SceneManager*    scene_manager;
    ViewDispatcher*  view_dispatcher;
    View*            baud_view;
    View*            sniff_view;

    // UART config
    uint8_t    baud_idx;      // index into US_BAUDS
    bool       uart_active;

    // ISR → main thread byte queue (lock-free ring)
    uint8_t    byte_buf[US_BYTE_BUF];
    volatile uint16_t buf_head;  // ISR writes here
    volatile uint16_t buf_tail;  // main thread reads here

    // Terminal line buffer
    UsLine     lines[US_TOTAL_LINES];
    uint16_t   line_head;     // next line to write (mod US_TOTAL_LINES)
    uint16_t   line_count;    // total lines written
    char       partial[US_LINE_BUF];  // current incomplete line
    uint8_t    partial_len;

    // Display
    UsDisplayMode display_mode;
    uint32_t   total_bytes;   // byte counter
    uint32_t   alert_count;   // credential keyword hits
} UsApp;

// ─────────────────────────────────────────────────────────────────────────────
// Scene declarations
// ─────────────────────────────────────────────────────────────────────────────

extern const SceneManagerHandlers us_scene_handlers;

void us_scene_baud_on_enter(void* ctx);
bool us_scene_baud_on_event(void* ctx, SceneManagerEvent ev);
void us_scene_baud_on_exit(void* ctx);

void us_scene_sniff_on_enter(void* ctx);
bool us_scene_sniff_on_event(void* ctx, SceneManagerEvent ev);
void us_scene_sniff_on_exit(void* ctx);
