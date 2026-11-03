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
#include <lib/subghz/protocols/raw.h>
#include <lib/subghz/receiver.h>
#include <lib/subghz/transmitter.h>
#include <lib/subghz/environment.h>

// ─────────────────────────────────────────────────────────────────────────────
// Capture frequencies and modulations
// ─────────────────────────────────────────────────────────────────────────────

#define SR_FREQ_COUNT 6

static const uint32_t SR_FREQUENCIES[SR_FREQ_COUNT] = {
    315000000UL,  // US garage doors, car fobs (older)
    390000000UL,  // Some US/CA rolling-code remotes
    433920000UL,  // EU/global OOK — most common
    434420000UL,  // EU alt
    868350000UL,  // EU Sub-GHz ISM
    915000000UL,  // US ISM
};

static const char* const SR_FREQ_LABELS[SR_FREQ_COUNT] = {
    "315.000 MHz",
    "390.000 MHz",
    "433.920 MHz",
    "434.420 MHz",
    "868.350 MHz",
    "915.000 MHz",
};

// Typical use-case label for demo narration
static const char* const SR_FREQ_HINTS[SR_FREQ_COUNT] = {
    "US garage / car fob",
    "US rolling-code remote",
    "EU remotes / sensors",
    "EU alt sensors",
    "EU smart home",
    "US smart home / IoT",
};

// ─────────────────────────────────────────────────────────────────────────────
// Raw capture storage
// ─────────────────────────────────────────────────────────────────────────────

// Raw signal: array of pulse/gap durations in microseconds.
// OOK signal at 433MHz: typical pulse ~300-600µs, up to ~200 transitions.
#define SR_RAW_MAX_PULSES  512   // max pulse/gap pairs stored
#define SR_CAPTURE_MAX       8   // vault slots

typedef struct {
    uint32_t freq;
    char     freq_label[14];
    char     hint[24];
    // Raw pulse/gap durations (µs). Interleaved: [pulse, gap, pulse, gap …]
    int32_t  durations[SR_RAW_MAX_PULSES];
    uint16_t duration_count;  // actual number of entries
    bool     valid;
    char     label[20];       // auto-label "Cap #N"

    // Waveform stats for display
    uint32_t total_us;        // total signal duration
    uint16_t pulse_count;     // number of pulses (high periods)
    int32_t  min_pulse_us;    // shortest pulse
    int32_t  max_pulse_us;    // longest pulse
} SrCapture;

// ─────────────────────────────────────────────────────────────────────────────
// Scenes / Views
// ─────────────────────────────────────────────────────────────────────────────

typedef enum {
    SrSceneMenu = 0,
    SrSceneCapture,
    SrSceneVault,
    SrSceneWaveform,
    SrSceneCount,
} SrScene;

typedef enum {
    SrViewSubmenu = 0,
    SrViewCapture,
    SrViewVault,
    SrViewWaveform,
} SrView;

typedef enum {
    SrEventSignalCaptured = 0x50,
    SrEventTransmitDone,
    SrEventBack,
    SrEventNext,
    SrEventFreqUp,
    SrEventFreqDown,
} SrEvent;

// ─────────────────────────────────────────────────────────────────────────────
// App state
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    Gui*             gui;
    NotificationApp* notifications;
    SceneManager*    scene_manager;
    ViewDispatcher*  view_dispatcher;
    Submenu*         submenu;
    View*            capture_view;
    View*            vault_view;
    View*            waveform_view;

    SubGhzWorker*      worker;
    SubGhzEnvironment* environment;
    SubGhzReceiver*    receiver;
    SubGhzTransmitter* transmitter;

    uint8_t  freq_idx;

    // Capture vault
    SrCapture vault[SR_CAPTURE_MAX];
    uint8_t   vault_count;
    uint8_t   vault_sel;

    // Live capture accumulation buffer (raw durations from ISR)
    int32_t   raw_buf[SR_RAW_MAX_PULSES];
    uint16_t  raw_count;
    bool      capturing;
    bool      transmitting;
} SrApp;

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static inline uint32_t sr_current_freq(SrApp* app) {
    return SR_FREQUENCIES[app->freq_idx];
}

// Analyze a capture and fill stats fields
static inline void sr_analyze(SrCapture* cap) {
    cap->total_us   = 0;
    cap->pulse_count = 0;
    cap->min_pulse_us = 0x7FFFFFFF;
    cap->max_pulse_us = 0;

    for(uint16_t i = 0; i < cap->duration_count; i++) {
        int32_t d = cap->durations[i];
        uint32_t abs_d = (uint32_t)(d < 0 ? -d : d);
        cap->total_us += abs_d;
        if(d > 0) {
            // Positive = pulse (high)
            cap->pulse_count++;
            if((int32_t)abs_d < cap->min_pulse_us) cap->min_pulse_us = (int32_t)abs_d;
            if((int32_t)abs_d > cap->max_pulse_us) cap->max_pulse_us = (int32_t)abs_d;
        }
    }
    if(cap->min_pulse_us == 0x7FFFFFFF) cap->min_pulse_us = 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Scene declarations
// ─────────────────────────────────────────────────────────────────────────────

extern const SceneManagerHandlers sr_scene_handlers;

void sr_scene_menu_on_enter(void* ctx);
bool sr_scene_menu_on_event(void* ctx, SceneManagerEvent ev);
void sr_scene_menu_on_exit(void* ctx);

void sr_scene_capture_on_enter(void* ctx);
bool sr_scene_capture_on_event(void* ctx, SceneManagerEvent ev);
void sr_scene_capture_on_exit(void* ctx);

void sr_scene_vault_on_enter(void* ctx);
bool sr_scene_vault_on_event(void* ctx, SceneManagerEvent ev);
void sr_scene_vault_on_exit(void* ctx);

void sr_scene_waveform_on_enter(void* ctx);
bool sr_scene_waveform_on_event(void* ctx, SceneManagerEvent ev);
void sr_scene_waveform_on_exit(void* ctx);
