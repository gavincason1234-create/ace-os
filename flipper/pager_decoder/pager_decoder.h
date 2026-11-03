#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <notification/notification_messages.h>
#include <lib/subghz/subghz_worker.h>
#include <lib/subghz/protocols/pocsag.h>
#include <lib/subghz/protocols/base.h>
#include <lib/subghz/receiver.h>
#include <lib/subghz/environment.h>

// ─────────────────────────────────────────────────────────────────────────────
// POCSAG constants
// ─────────────────────────────────────────────────────────────────────────────

// Standard medical/hospital pager frequencies (North America)
#define PD_FREQ_COUNT 4
static const uint32_t PD_FREQUENCIES[PD_FREQ_COUNT] = {
    152000000UL,   // 152.000 MHz — most common US hospital
    157450000UL,   // 157.450 MHz — alternate medical
    158100000UL,   // 158.100 MHz — alternate
    929637500UL,   // 929.637 MHz — national wide-area (POCSAG 512/1200)
};
static const char* const PD_FREQ_LABELS[PD_FREQ_COUNT] = {
    "152.000 MHz",
    "157.450 MHz",
    "158.100 MHz",
    "929.637 MHz",
};

// POCSAG baud rates
typedef enum {
    PdBaud512  = 0,
    PdBaud1200,
    PdBaud2400,
    PdBaudCount,
} PdBaud;

// ─────────────────────────────────────────────────────────────────────────────
// Decoded message
// ─────────────────────────────────────────────────────────────────────────────

#define PD_MSG_TEXT_LEN  64
#define PD_MSG_LOG_SIZE   8

typedef enum {
    PdMsgNumeric = 0,
    PdMsgAlpha,
    PdMsgTone,
} PdMsgType;

typedef struct {
    uint32_t  capcode;          // POCSAG CAP code (address)
    PdMsgType msg_type;
    char      text[PD_MSG_TEXT_LEN];
    uint32_t  freq;
    uint32_t  baud;
    bool      valid;
} PdMessage;

// ─────────────────────────────────────────────────────────────────────────────
// Scenes / Views
// ─────────────────────────────────────────────────────────────────────────────

typedef enum {
    PdSceneMenu = 0,
    PdSceneScan,
    PdSceneLog,
    PdSceneCount,
} PdScene;

typedef enum {
    PdViewSubmenu = 0,
    PdViewScan,
    PdViewLog,
} PdView;

typedef enum {
    PdEventMsgReceived = 0x30,
    PdEventBack,
    PdEventFreqUp,
    PdEventFreqDown,
    PdEventBaudCycle,
} PdEvent;

// ─────────────────────────────────────────────────────────────────────────────
// App state
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    Gui*             gui;
    NotificationApp* notifications;
    SceneManager*    scene_manager;
    ViewDispatcher*  view_dispatcher;
    Submenu*         submenu;
    View*            scan_view;
    View*            log_view;

    SubGhzWorker*    worker;
    SubGhzEnvironment* environment;
    SubGhzReceiver*    receiver;

    uint8_t  freq_idx;   // index into PD_FREQUENCIES
    PdBaud   baud;

    // Circular message log
    PdMessage log[PD_MSG_LOG_SIZE];
    uint8_t   log_count;   // total received (wraps)
    uint8_t   log_sel;     // selected in log view

    // Last received message (for scan view live display)
    PdMessage last;
    uint32_t  total_rx;    // total frames received
} PdApp;

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static inline uint32_t pd_baud_value(PdBaud b) {
    switch(b) {
    case PdBaud512:  return 512;
    case PdBaud1200: return 1200;
    case PdBaud2400: return 2400;
    default:         return 1200;
    }
}

static inline const char* pd_baud_label(PdBaud b) {
    switch(b) { case PdBaud512: return "512"; case PdBaud1200: return "1200";
    case PdBaud2400: return "2400"; default: return "?"; }
}

static inline const char* pd_msg_type_label(PdMsgType t) {
    switch(t) { case PdMsgNumeric: return "NUM"; case PdMsgAlpha: return "ALPHA";
    case PdMsgTone: return "TONE"; default: return "?"; }
}

// ─────────────────────────────────────────────────────────────────────────────
// Scene declarations
// ─────────────────────────────────────────────────────────────────────────────

extern const SceneManagerHandlers pd_scene_handlers;

void pd_scene_menu_on_enter(void* ctx);
bool pd_scene_menu_on_event(void* ctx, SceneManagerEvent ev);
void pd_scene_menu_on_exit(void* ctx);

void pd_scene_scan_on_enter(void* ctx);
bool pd_scene_scan_on_event(void* ctx, SceneManagerEvent ev);
void pd_scene_scan_on_exit(void* ctx);

void pd_scene_log_on_enter(void* ctx);
bool pd_scene_log_on_event(void* ctx, SceneManagerEvent ev);
void pd_scene_log_on_exit(void* ctx);
