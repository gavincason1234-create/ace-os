#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <notification/notification_messages.h>
#include <infrared_worker.h>
#include <infrared.h>
#include <storage/storage.h>
#include <string.h>
#include <stdio.h>

// ─────────────────────────────────────────────────────────────────────────────
// Stored signal entry
// ─────────────────────────────────────────────────────────────────────────────

#define IRC_TIMING_MAX  512   // raw timings per capture (uint32_t)
#define IRC_VAULT_MAX   8     // max stored signals
#define IRC_VAULT_PATH  "/ext/.system/calibration/IR_VAULT.bin"

typedef struct {
    uint32_t timings[IRC_TIMING_MAX];
    uint32_t timing_count;
    uint32_t frequency;     // Hz, typically 38000
    float    duty_cycle;    // 0.33 typical
    char     label[20];
    // Decoded overlay (if protocol matched)
    bool     has_decoded;
    char     protocol[12];  // "NEC", "Samsung32", etc.
    uint32_t address;
    uint32_t command;
    bool     valid;
} IrcSignal;

// ─────────────────────────────────────────────────────────────────────────────
// Scenes / Views
// ─────────────────────────────────────────────────────────────────────────────

typedef enum {
    IrcSceneLearn = 0,
    IrcSceneVault,
    IrcSceneCount,
} IrcScene;

typedef enum {
    IrcViewLearn = 0,
    IrcViewVault,
} IrcView;

typedef enum {
    IrcEventSignalCaptured = 0x70,
    IrcEventBack,
    IrcEventNext,
    IrcEventPrev,
    IrcEventSelect,
} IrcEvent;

// ─────────────────────────────────────────────────────────────────────────────
// App state
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    Gui*             gui;
    NotificationApp* notifications;
    SceneManager*    scene_manager;
    ViewDispatcher*  view_dispatcher;
    View*            learn_view;
    View*            vault_view;

    InfraredWorker*  worker;

    // Capture buffer (raw signal from worker callback)
    uint32_t raw_timings[IRC_TIMING_MAX];
    uint32_t raw_count;
    uint32_t raw_freq;
    float    raw_duty;

    // Decoded overlay from last capture
    bool     last_decoded;
    char     last_protocol[12];
    uint32_t last_address;
    uint32_t last_command;

    // Vault
    IrcSignal vault[IRC_VAULT_MAX];
    uint8_t   vault_count;
    uint8_t   vault_sel;
    bool      transmitting;
} IrcApp;

// ─────────────────────────────────────────────────────────────────────────────
// RC4 helpers (same pattern as other apps in suite)
// ─────────────────────────────────────────────────────────────────────────────

#define IRC_VAULT_SALT 0x49524343UL  // "IRCC"

static inline void irc_rc4(uint8_t* buf, uint32_t len, const uint8_t* key, uint8_t klen) {
    uint8_t S[256];
    for(int i = 0; i < 256; i++) S[i] = (uint8_t)i;
    uint8_t j = 0;
    for(int i = 0; i < 256; i++) {
        j = (uint8_t)(j + S[i] + key[i % klen]);
        uint8_t t = S[i]; S[i] = S[j]; S[j] = t;
    }
    uint8_t si = 0, sj = 0;
    for(uint32_t n = 0; n < len; n++) {
        si = (uint8_t)(si + 1);
        sj = (uint8_t)(sj + S[si]);
        uint8_t t = S[si]; S[si] = S[sj]; S[sj] = t;
        buf[n] ^= S[(uint8_t)(S[si] + S[sj])];
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Scene declarations
// ─────────────────────────────────────────────────────────────────────────────

extern const SceneManagerHandlers irc_scene_handlers;

void irc_scene_learn_on_enter(void* ctx);
bool irc_scene_learn_on_event(void* ctx, SceneManagerEvent ev);
void irc_scene_learn_on_exit(void* ctx);

void irc_scene_vault_on_enter(void* ctx);
bool irc_scene_vault_on_event(void* ctx, SceneManagerEvent ev);
void irc_scene_vault_on_exit(void* ctx);
