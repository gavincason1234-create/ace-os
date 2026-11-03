#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_usb_hid.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <notification/notification_messages.h>
#include <string.h>
#include <stdio.h>

// ─────────────────────────────────────────────────────────────────────────────
// HID helpers
// ─────────────────────────────────────────────────────────────────────────────

// Key press: modifier in high byte, HID keycode in low byte
#define HID_KEY(mod, key)  ((uint16_t)(((mod) << 8) | (key)))

// Modifier bitmasks (shifted into high byte)
#define MOD_LCTRL   0x01
#define MOD_LSHIFT  0x02
#define MOD_LALT    0x04
#define MOD_LGUI    0x08   // Windows key / Cmd

// Convenience macros
#define KEY_WIN_R   HID_KEY(MOD_LGUI,   HID_KEYBOARD_R_R)
#define KEY_WIN_L   HID_KEY(MOD_LGUI,   HID_KEYBOARD_L_L)
#define KEY_ENTER   HID_KEY(0,          HID_KEYBOARD_RETURN)
#define KEY_SPACE   HID_KEY(0,          HID_KEYBOARD_SPACEBAR)
#define KEY_TAB     HID_KEY(0,          HID_KEYBOARD_TAB)
#define KEY_ESC     HID_KEY(0,          HID_KEYBOARD_ESCAPE)
#define KEY_NONE    0

// Type a printable ASCII string, one keystroke per char
// Returns false on first HID error
bool bu_type_string(const char* str);

// Press and immediately release a key
static inline void bu_key(uint16_t key) {
    furi_hal_hid_kb_press(key);
    furi_delay_ms(20);
    furi_hal_hid_kb_release(key);
    furi_delay_ms(20);
}

// ─────────────────────────────────────────────────────────────────────────────
// Payload definitions
// ─────────────────────────────────────────────────────────────────────────────

typedef enum {
    BuPayloadLock = 0,    // Win+L — instant screen lock
    BuPayloadNotepad,     // Win+R → notepad → type message
    BuPayloadCmdRecon,    // Win+R → cmd → whoami && ipconfig
    BuPayloadBrowser,     // Win+R → start <URL>
    BuPayloadCount,
} BuPayloadType;

typedef enum {
    BuRunStateIdle = 0,
    BuRunStateCountdown,
    BuRunStateInjecting,
    BuRunStateDone,
    BuRunStateError,
    BuRunStateNoUsb,
} BuRunState;

typedef struct {
    BuPayloadType type;
    const char*   label;
    const char*   icon;      // 3-char tag
    const char*   target_os; // "WIN" / "MAC" / "ANY"
    const char*   desc;
    const char*   detail;    // shown during countdown
} BuPreset;

#define BU_PRESET_COUNT 4
static const BuPreset BU_PRESETS[BU_PRESET_COUNT] = {
    {
        BuPayloadLock,
        "Screen Lock",
        "LCK",
        "WIN",
        "Instant Win+L screen lock",
        "Locks screen — demonstrates unattended workstation risk",
    },
    {
        BuPayloadNotepad,
        "Notepad Drop",
        "TXT",
        "WIN",
        "Win+R → notepad → type warning",
        "Opens Notepad and types AUTHORIZED TEST message",
    },
    {
        BuPayloadCmdRecon,
        "CMD Recon",
        "CMD",
        "WIN",
        "Win+R → cmd → whoami && ipconfig",
        "Demonstrates attacker recon in <3 seconds via USB",
    },
    {
        BuPayloadBrowser,
        "Browser Lure",
        "URL",
        "WIN",
        "Win+R → start phishing URL",
        "Opens browser to demo lure page (signalstack.io/demo)",
    },
};

// ─────────────────────────────────────────────────────────────────────────────
// Events
// ─────────────────────────────────────────────────────────────────────────────

typedef enum {
    BuEventPayloadSelected = 0xC0,
    BuEventStartCountdown,
    BuEventCountdownTick,
    BuEventInjectDone,
    BuEventInjectError,
    BuEventNoUsb,
    BuEventBack,
} BuEvent;

// ─────────────────────────────────────────────────────────────────────────────
// Scenes / Views
// ─────────────────────────────────────────────────────────────────────────────

typedef enum {
    BuSceneMenu = 0,
    BuSceneRun,
    BuSceneCount,
} BuScene;

typedef enum {
    BuViewMenu = 0,
    BuViewRun,
} BuView;

// ─────────────────────────────────────────────────────────────────────────────
// App state
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    Gui*             gui;
    NotificationApp* notifications;
    SceneManager*    scene_manager;
    ViewDispatcher*  view_dispatcher;
    View*            menu_view;
    View*            run_view;

    BuPayloadType    payload_type;
    BuRunState       run_state;

    FuriThread*      inject_thread;
    uint8_t          countdown;      // 3..0
    uint32_t         bytes_typed;    // chars injected

    // Saved USB config for restore on exit
    FuriHalUsbInterface* saved_usb;
} BuApp;

// ─────────────────────────────────────────────────────────────────────────────
// Scene declarations
// ─────────────────────────────────────────────────────────────────────────────

extern const SceneManagerHandlers bu_scene_handlers;

void bu_scene_menu_on_enter(void* ctx);
bool bu_scene_menu_on_event(void* ctx, SceneManagerEvent ev);
void bu_scene_menu_on_exit(void* ctx);

void bu_scene_run_on_enter(void* ctx);
bool bu_scene_run_on_event(void* ctx, SceneManagerEvent ev);
void bu_scene_run_on_exit(void* ctx);
