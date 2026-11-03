#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_subghz.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <notification/notification_messages.h>
#include <string.h>
#include <stdio.h>

// ─────────────────────────────────────────────────────────────────────────────
// Frequency range presets
// ─────────────────────────────────────────────────────────────────────────────

#define SS_BINS 128    // one pixel column per bin

typedef struct {
    uint32_t    start_hz;     // lowest frequency in range
    uint32_t    end_hz;       // highest frequency in range
    const char* label;        // display name
    const char* desc;         // what lives here
} SsRange;

#define SS_RANGE_COUNT 5
static const SsRange SS_RANGES[SS_RANGE_COUNT] = {
    { 300000000UL, 350000000UL, "300-350 MHz", "US garage/car remotes" },
    { 400000000UL, 450000000UL, "400-450 MHz", "EU keyfobs / sensors"  },
    { 860000000UL, 870000000UL, "860-870 MHz", "EU 868 smart meters"   },
    { 902000000UL, 928000000UL, "902-928 MHz", "US 915 ISM / LoRa"     },
    { 300000000UL, 960000000UL, "300-960 MHz", "Full sweep (slow)"     },
};

// Well-known marker frequencies (shown as tick marks on spectrum floor)
typedef struct {
    uint32_t    freq_hz;
    const char* label;  // 3 chars max
} SsMarker;

#define SS_MARKER_COUNT 6
static const SsMarker SS_MARKERS[SS_MARKER_COUNT] = {
    { 315000000UL, "315" },
    { 433920000UL, "434" },
    { 868000000UL, "868" },
    { 915000000UL, "915" },
    { 345000000UL, "345" },
    { 916500000UL, "916" },
};

// ─────────────────────────────────────────────────────────────────────────────
// Spectrum data
// ─────────────────────────────────────────────────────────────────────────────

// RSSI floor / ceiling for normalization
#define SS_RSSI_FLOOR   (-120)   // dBm
#define SS_RSSI_CEIL    (-30)    // dBm
#define SS_BAR_MAX       48      // max bar height in pixels (screen height - header - footer)

static inline uint8_t ss_rssi_to_px(int8_t rssi) {
    if(rssi <= SS_RSSI_FLOOR) return 0;
    if(rssi >= SS_RSSI_CEIL)  return SS_BAR_MAX;
    return (uint8_t)(((int16_t)(rssi - SS_RSSI_FLOOR) * SS_BAR_MAX)
                     / (SS_RSSI_CEIL - SS_RSSI_FLOOR));
}

// ─────────────────────────────────────────────────────────────────────────────
// Scenes / Views
// ─────────────────────────────────────────────────────────────────────────────

typedef enum {
    SsSceneRange = 0,
    SsSceneSpectrum,
    SsSceneCount,
} SsScene;

typedef enum {
    SsViewRange = 0,
    SsViewSpectrum,
} SsView;

typedef enum {
    SsEventRangeSelected = 0xA0,
    SsEventBack,
    SsEventSweepDone,
    SsEventCursorLeft,
    SsEventCursorRight,
    SsEventTogglePeak,
} SsEvent;

// ─────────────────────────────────────────────────────────────────────────────
// App state
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    Gui*             gui;
    NotificationApp* notifications;
    SceneManager*    scene_manager;
    ViewDispatcher*  view_dispatcher;
    View*            range_view;
    View*            spectrum_view;

    // Spectrum data (updated by timer, read by draw)
    uint8_t  rssi_px[SS_BINS];       // current RSSI bar height per bin
    uint8_t  peak_px[SS_BINS];       // peak hold per bin
    uint8_t  scan_pos;               // current scan bin (0..127)
    bool     peak_hold;              // peak hold mode

    // Frequency range
    uint8_t  range_idx;

    // Cursor
    uint8_t  cursor;                 // bin index under cursor
    int8_t   cursor_rssi;            // RSSI at cursor bin

    // Sweep stats
    uint32_t sweep_count;
    bool     scanning;
} SsApp;

// ─────────────────────────────────────────────────────────────────────────────
// Scene declarations
// ─────────────────────────────────────────────────────────────────────────────

extern const SceneManagerHandlers ss_scene_handlers;

void ss_scene_range_on_enter(void* ctx);
bool ss_scene_range_on_event(void* ctx, SceneManagerEvent ev);
void ss_scene_range_on_exit(void* ctx);

void ss_scene_spectrum_on_enter(void* ctx);
bool ss_scene_spectrum_on_event(void* ctx, SceneManagerEvent ev);
void ss_scene_spectrum_on_exit(void* ctx);
