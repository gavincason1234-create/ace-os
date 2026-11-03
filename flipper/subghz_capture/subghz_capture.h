#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_subghz.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <string.h>
#include <stdio.h>

// ─────────────────────────────────────────────────────────────────────────────
// Signal storage
// ─────────────────────────────────────────────────────────────────────────────

#define SC_TRANSITION_MAX  512   // max level/duration pairs per capture
#define SC_VAULT_MAX       8     // max stored signals
#define SC_GAP_US          50000 // silence >50ms = end of signal
#define SC_MIN_PULSES      8     // discard captures with fewer transitions

// RC4 stealth vault
#define SC_VAULT_PATH      "/ext/.system/calibration/SG_VAULT.bin"
#define SC_VAULT_SALT      0x53475243UL   // "SGRC"

// Each raw transition: duration in µs + level (high/low)
typedef struct {
    uint32_t duration_us;
    bool     level;
} ScTransition;

// One captured signal
typedef struct {
    uint32_t     freq_hz;
    ScTransition transitions[SC_TRANSITION_MAX];
    uint16_t     count;         // number of valid transitions
    uint32_t     total_us;      // total signal duration
    uint32_t     min_pulse_us;
    uint32_t     max_pulse_us;
    char         label[20];     // e.g. "315M sig#1"
    bool         valid;
} ScSignal;

// ─────────────────────────────────────────────────────────────────────────────
// RC4 helper
// ─────────────────────────────────────────────────────────────────────────────

static inline void sc_rc4(uint8_t* data, uint32_t len, uint32_t key) {
    uint8_t S[256];
    for(uint16_t i = 0; i < 256; i++) S[i] = (uint8_t)i;
    uint8_t j = 0;
    uint8_t k[4];
    k[0] = (uint8_t)(key);
    k[1] = (uint8_t)(key >> 8);
    k[2] = (uint8_t)(key >> 16);
    k[3] = (uint8_t)(key >> 24);
    for(uint16_t i = 0; i < 256; i++) {
        j = (uint8_t)(j + S[i] + k[i % 4]);
        uint8_t t = S[i]; S[i] = S[j]; S[j] = t;
    }
    uint8_t ii = 0; j = 0;
    for(uint32_t n = 0; n < len; n++) {
        ii++;
        j = (uint8_t)(j + S[ii]);
        uint8_t t = S[ii]; S[ii] = S[j]; S[j] = t;
        data[n] ^= S[(uint8_t)(S[ii] + S[j])];
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Frequency presets
// ─────────────────────────────────────────────────────────────────────────────

#define SC_FREQ_COUNT 5
typedef struct {
    uint32_t    hz;
    const char* label;
    const char* use;
} ScFreq;

static const ScFreq SC_FREQS[SC_FREQ_COUNT] = {
    { 315000000, "315.000 MHz", "US garage/car fob" },
    { 433920000, "433.920 MHz", "EU remote/sensor"  },
    { 868350000, "868.350 MHz", "EU IoT/alarm"      },
    { 915000000, "915.000 MHz", "US ISM band"       },
    { 433075000, "433.075 MHz", "Some car fobs"     },
};

// ─────────────────────────────────────────────────────────────────────────────
// Capture state machine
// ─────────────────────────────────────────────────────────────────────────────

typedef enum {
    ScCaptureStateIdle = 0,
    ScCaptureStateArmed,     // radio on, waiting for first pulse
    ScCaptureStateReceiving, // capturing transitions
    ScCaptureStateDone,      // signal ended (gap detected)
    ScCaptureStateOverflow,  // too many transitions
    ScCaptureStateTooShort,  // discarded — too few pulses
} ScCaptureState;

typedef enum {
    ScReplayStateIdle = 0,
    ScReplayStateActive,
    ScReplayStateDone,
} ScReplayState;

// ─────────────────────────────────────────────────────────────────────────────
// Events
// ─────────────────────────────────────────────────────────────────────────────

typedef enum {
    ScEventFreqSelected = 0xD0,
    ScEventBack,
    ScEventCaptureDone,
    ScEventCaptureTooShort,
    ScEventCaptureOverflow,
    ScEventReplayDone,
    ScEventVaultSelected,
} ScEvent;

// ─────────────────────────────────────────────────────────────────────────────
// Scenes / Views
// ─────────────────────────────────────────────────────────────────────────────

typedef enum {
    ScSceneFreq = 0,
    ScSceneCapture,
    ScSceneVault,
    ScSceneCount,
} ScScene;

typedef enum {
    ScViewFreq = 0,
    ScViewCapture,
    ScViewVault,
} ScView;

// ─────────────────────────────────────────────────────────────────────────────
// App state
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    Gui*             gui;
    NotificationApp* notifications;
    SceneManager*    scene_manager;
    ViewDispatcher*  view_dispatcher;
    View*            freq_view;
    View*            capture_view;
    View*            vault_view;

    uint8_t          freq_idx;       // index into SC_FREQS

    // Live capture buffer (written from ISR callback)
    volatile ScTransition cap_buf[SC_TRANSITION_MAX];
    volatile uint16_t     cap_count;
    volatile bool         cap_active;
    ScCaptureState        cap_state;

    // Last complete capture (copied from cap_buf when done)
    ScSignal              current;

    // Vault
    ScSignal              vault[SC_VAULT_MAX];
    uint8_t               vault_count;
    uint8_t               vault_sel;   // selected item in vault scene

    // TX state
    uint16_t              tx_pos;       // current position in TX playback
    ScReplayState         replay_state;
} ScApp;

// ─────────────────────────────────────────────────────────────────────────────
// Vault I/O
// ─────────────────────────────────────────────────────────────────────────────

void sc_vault_save(ScApp* app);
void sc_vault_load(ScApp* app);

// Sub-GHz TX callback (called by HAL to get next level+duration)
LevelDuration sc_tx_callback(void* ctx);

// ─────────────────────────────────────────────────────────────────────────────
// Scene declarations
// ─────────────────────────────────────────────────────────────────────────────

extern const SceneManagerHandlers sc_scene_handlers;

void sc_scene_freq_on_enter(void* ctx);
bool sc_scene_freq_on_event(void* ctx, SceneManagerEvent ev);
void sc_scene_freq_on_exit(void* ctx);

void sc_scene_capture_on_enter(void* ctx);
bool sc_scene_capture_on_event(void* ctx, SceneManagerEvent ev);
void sc_scene_capture_on_exit(void* ctx);

void sc_scene_vault_on_enter(void* ctx);
bool sc_scene_vault_on_event(void* ctx, SceneManagerEvent ev);
void sc_scene_vault_on_exit(void* ctx);
