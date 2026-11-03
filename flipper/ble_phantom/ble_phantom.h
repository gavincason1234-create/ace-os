#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_bt.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <notification/notification_messages.h>

#include "bp_payloads.h"

// ─────────────────────────────────────────────────────────────────────────────
// Enums
// ─────────────────────────────────────────────────────────────────────────────

typedef enum {
    PhantomModeAppleAll = 0,
    PhantomModeNewIPhone,
    PhantomModeAirPodsStorm,
    PhantomModeAndroidBlitz,
    PhantomModeWindows,
    PhantomModeChaos,
    PhantomModeCount,
} PhantomMode;

typedef enum {
    PhantomSpeedSlow    = 0,   // 1000 ms
    PhantomSpeedNormal,         // 400 ms
    PhantomSpeedFast,           // 150 ms
    PhantomSpeedInsane,         // 40 ms
    PhantomSpeedCount,
} PhantomSpeed;

typedef enum {
    PhantomSceneMenu = 0,
    PhantomSceneAttack,
    PhantomSceneCount,
} PhantomScene;

typedef enum {
    PhantomViewSubmenu = 0,
    PhantomViewAttack,
} PhantomView;

typedef enum {
    PhantomEventBack  = 0x10,
    PhantomEventTick,
} PhantomEvent;

// ─────────────────────────────────────────────────────────────────────────────
// App state
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    Gui*             gui;
    NotificationApp* notifications;
    SceneManager*    scene_manager;
    ViewDispatcher*  view_dispatcher;
    Submenu*         submenu;
    View*            attack_view;

    PhantomMode  mode;
    PhantomSpeed speed;
    bool         attacking;
    bool         paused;
    FuriTimer*   timer;

    uint32_t packets_sent;
    uint32_t ios_hits;
    uint32_t android_hits;
    uint32_t win_hits;
    uint32_t template_idx;  // current template within mode range
    uint32_t chaos_seed;    // pseudo-random walk for chaos mode

    // Mutable advertisement buffer (filled from template + randomization)
    uint8_t  adv_buf[BP_ADV_MAX_LEN];
} BlePhantomApp;

// ─────────────────────────────────────────────────────────────────────────────
// Scene handler declarations
// ─────────────────────────────────────────────────────────────────────────────

extern const SceneManagerHandlers bp_scene_handlers;

void bp_scene_menu_on_enter(void* ctx);
bool bp_scene_menu_on_event(void* ctx, SceneManagerEvent ev);
void bp_scene_menu_on_exit(void* ctx);

void bp_scene_attack_on_enter(void* ctx);
bool bp_scene_attack_on_event(void* ctx, SceneManagerEvent ev);
void bp_scene_attack_on_exit(void* ctx);

// ─────────────────────────────────────────────────────────────────────────────
// Speed → interval table (ms)
// ─────────────────────────────────────────────────────────────────────────────

static inline uint32_t bp_speed_ms(PhantomSpeed s) {
    switch(s) {
    case PhantomSpeedSlow:   return 1000;
    case PhantomSpeedNormal: return 400;
    case PhantomSpeedFast:   return 150;
    case PhantomSpeedInsane: return 40;
    default:                 return 400;
    }
}

static inline const char* bp_speed_name(PhantomSpeed s) {
    switch(s) {
    case PhantomSpeedSlow:   return "SLOW";
    case PhantomSpeedNormal: return "NORMAL";
    case PhantomSpeedFast:   return "FAST";
    case PhantomSpeedInsane: return "INSANE";
    default:                 return "?";
    }
}

static inline const char* bp_mode_name(PhantomMode m) {
    switch(m) {
    case PhantomModeAppleAll:     return "Apple iOS Blitz";
    case PhantomModeNewIPhone:    return "New iPhone Spam";
    case PhantomModeAirPodsStorm: return "AirPods Storm";
    case PhantomModeAndroidBlitz: return "Android Blitz";
    case PhantomModeWindows:      return "Windows Swift Pair";
    case PhantomModeChaos:        return "CHAOS (All)";
    default:                      return "?";
    }
}
