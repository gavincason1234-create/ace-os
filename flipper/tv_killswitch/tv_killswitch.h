#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <infrared.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <notification/notification_messages.h>

// ─────────────────────────────────────────────────────────────────────────────
// TV power code entry
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    const char*      brand;
    InfraredProtocol protocol;
    uint32_t         address;
    uint32_t         command;
} TvPowerCode;

// ─────────────────────────────────────────────────────────────────────────────
// Attack categories
// ─────────────────────────────────────────────────────────────────────────────

typedef enum {
    TvkCategoryAll = 0,       // all 60+ codes
    TvkCategorySamsung,
    TvkCategoryLG,
    TvkCategorySony,
    TvkCategoryPhilips,
    TvkCategoryOther,
    TvkCategoryCount,
} TvkCategory;

typedef enum {
    TvkSpeedSlow   = 0,  // 600 ms between codes
    TvkSpeedNormal,       // 300 ms
    TvkSpeedFast,         // 180 ms
    TvkSpeedBlast,        // 100 ms (max IR hardware throughput)
    TvkSpeedCount,
} TvkSpeed;

typedef enum {
    TvkSceneMenu = 0,
    TvkSceneKill,
    TvkSceneCount,
} TvkScene;

typedef enum {
    TvkViewSubmenu = 0,
    TvkViewKill,
} TvkView;

typedef enum {
    TvkEventBack = 0x10,
    TvkEventTick,
} TvkEvent;

// ─────────────────────────────────────────────────────────────────────────────
// App state
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    Gui*             gui;
    NotificationApp* notifications;
    SceneManager*    scene_manager;
    ViewDispatcher*  view_dispatcher;
    Submenu*         submenu;
    View*            kill_view;

    TvkCategory  category;
    TvkSpeed     speed;
    bool         paused;
    FuriTimer*   timer;

    uint32_t     code_idx;       // absolute index into TV_CODES[]
    uint32_t     codes_sent;
    uint32_t     range_first;    // first code index for current category
    uint32_t     range_last;     // last code index for current category
} TvKillApp;

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static inline uint32_t tvk_speed_ms(TvkSpeed s) {
    switch(s) {
    case TvkSpeedSlow:   return 600;
    case TvkSpeedNormal: return 300;
    case TvkSpeedFast:   return 180;
    case TvkSpeedBlast:  return 100;
    default:             return 300;
    }
}

static inline const char* tvk_speed_name(TvkSpeed s) {
    switch(s) {
    case TvkSpeedSlow:   return "SLOW";
    case TvkSpeedNormal: return "NORMAL";
    case TvkSpeedFast:   return "FAST";
    case TvkSpeedBlast:  return "BLAST";
    default:             return "?";
    }
}

static inline const char* tvk_category_name(TvkCategory c) {
    switch(c) {
    case TvkCategoryAll:     return "All Brands (60+)";
    case TvkCategorySamsung: return "Samsung";
    case TvkCategoryLG:      return "LG";
    case TvkCategorySony:    return "Sony";
    case TvkCategoryPhilips: return "Philips / Magnavox";
    case TvkCategoryOther:   return "Other Brands";
    default:                 return "?";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Scene declarations
// ─────────────────────────────────────────────────────────────────────────────

extern const SceneManagerHandlers tvk_scene_handlers;

void tvk_scene_menu_on_enter(void* ctx);
bool tvk_scene_menu_on_event(void* ctx, SceneManagerEvent ev);
void tvk_scene_menu_on_exit(void* ctx);

void tvk_scene_kill_on_enter(void* ctx);
bool tvk_scene_kill_on_event(void* ctx, SceneManagerEvent ev);
void tvk_scene_kill_on_exit(void* ctx);
