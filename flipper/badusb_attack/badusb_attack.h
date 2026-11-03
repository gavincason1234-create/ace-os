#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_usb.h>
#include <furi_hal_usb_hid.h>
#include <furi_hal_power.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <notification/notification_messages.h>

// ─────────────────────────────────────────────────────────────────────────────
// HID helper macros
// ─────────────────────────────────────────────────────────────────────────────

// Lower-case letter a-z → HID keycode
#define BA_KEY(lc)  ((uint16_t)(HID_KEYBOARD_A + ((lc) - 'a')))

// Modifier bit shortcuts (matches furi_hal_usb_hid.h)
#define BA_MOD_CTRL   KEY_MOD_LEFT_CTRL
#define BA_MOD_SHIFT  KEY_MOD_LEFT_SHIFT
#define BA_MOD_ALT    KEY_MOD_LEFT_ALT
#define BA_MOD_GUI    KEY_MOD_LEFT_GUI   // Windows / Command key

// ─────────────────────────────────────────────────────────────────────────────
// Enums
// ─────────────────────────────────────────────────────────────────────────────

typedef enum {
    BaSceneMenu = 0,
    BaSceneExecute,
    BaSceneCount,
} BaScene;

typedef enum {
    BaViewSubmenu = 0,
    BaViewExecute,
} BaView;

typedef enum {
    BaEventBack    = 0x10,
    BaEventExecDone,
    BaEventCountTick,
} BaEvent;

typedef enum {
    BaExecCountdown = 0,
    BaExecRunning,
    BaExecDone,
    BaExecError,
} BaExecSubState;

// ─────────────────────────────────────────────────────────────────────────────
// Payload descriptor
// ─────────────────────────────────────────────────────────────────────────────

typedef struct BaBadUsbApp BaBadUsbApp;

typedef struct {
    const char* name;
    const char* target_os;
    const char* risk_level;   // "DEMO" / "HIGH" / "CRITICAL"
    const char* description;
    void (*execute_fn)(BaBadUsbApp*);
} BaPayload;

#define BA_PAYLOAD_COUNT 5
extern const BaPayload BA_PAYLOADS[BA_PAYLOAD_COUNT];

// ─────────────────────────────────────────────────────────────────────────────
// App state
// ─────────────────────────────────────────────────────────────────────────────

struct BaBadUsbApp {
    Gui*             gui;
    NotificationApp* notifications;
    SceneManager*    scene_manager;
    ViewDispatcher*  view_dispatcher;
    Submenu*         submenu;
    View*            execute_view;

    uint8_t          selected;           // index into BA_PAYLOADS
    FuriHalUsbInterface* saved_usb_cfg;  // to restore on exit
    FuriThread*      exec_thread;
    bool             thread_done;
};

// ─────────────────────────────────────────────────────────────────────────────
// Execute scene view model — shared so payload functions can update the log
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    BaExecSubState substate;
    uint8_t        countdown;
    uint8_t        tick;
    bool           usb_ok;
    char           payload_name[24];
    char           risk[10];
    char           target_os[12];
    char           log[4][32];
    uint8_t        log_count;
    bool           success;
} ExecuteViewModel;

// ─────────────────────────────────────────────────────────────────────────────
// HID typing helpers (defined in badusb_attack.c)
// ─────────────────────────────────────────────────────────────────────────────

void ba_press(BaBadUsbApp* app, uint16_t mod, uint16_t key);
void ba_type_char(BaBadUsbApp* app, char c);
void ba_type_string(BaBadUsbApp* app, const char* s);
void ba_push_log(BaBadUsbApp* app, const char* line);

// ─────────────────────────────────────────────────────────────────────────────
// Scene declarations
// ─────────────────────────────────────────────────────────────────────────────

extern const SceneManagerHandlers ba_scene_handlers;

void ba_scene_menu_on_enter(void* ctx);
bool ba_scene_menu_on_event(void* ctx, SceneManagerEvent ev);
void ba_scene_menu_on_exit(void* ctx);

void ba_scene_execute_on_enter(void* ctx);
bool ba_scene_execute_on_event(void* ctx, SceneManagerEvent ev);
void ba_scene_execute_on_exit(void* ctx);
