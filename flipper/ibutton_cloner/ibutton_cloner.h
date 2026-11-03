#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <notification/notification_messages.h>
#include <ibutton/ibutton_worker.h>
#include <ibutton/ibutton_key.h>
#include <storage/storage.h>
#include <string.h>
#include <stdio.h>

// ─────────────────────────────────────────────────────────────────────────────
// Key types supported
// ─────────────────────────────────────────────────────────────────────────────

// Human-readable use-case for each family code
typedef struct {
    uint8_t     family_code;
    const char* type_name;   // "DS1990A"
    const char* use_case;    // "Apartments / elevators"
    const char* grade;       // security verdict
} IbcFamilyInfo;

// DS1990A family=0x01, Dallas Semiconductor — most common iButton
// DS1991   family=0x02 — password protected (rare)
// DS1992   family=0x08 — 1Kb SRAM
// DS1993   family=0x06 — 4Kb SRAM
// DS1995   family=0x0A — 16Kb SRAM
// DS1996   family=0x0C — 64Kb SRAM

#define IBC_FAMILY_COUNT 6
static const IbcFamilyInfo IBC_FAMILIES[IBC_FAMILY_COUNT] = {
    { 0x01, "DS1990A",  "Apt/elevator/gym",     "GRADE: F (no crypto)" },
    { 0x02, "DS1991",   "Password iButton",     "GRADE: D (weak pwd)"  },
    { 0x06, "DS1993",   "4KB SRAM key",         "GRADE: F (copyable)"  },
    { 0x08, "DS1992",   "1KB SRAM key",         "GRADE: F (copyable)"  },
    { 0x0A, "DS1995",   "16KB SRAM key",        "GRADE: F (copyable)"  },
    { 0x0C, "DS1996",   "64KB SRAM key",        "GRADE: F (copyable)"  },
};

static inline const IbcFamilyInfo* ibc_family(uint8_t code) {
    for(uint8_t i = 0; i < IBC_FAMILY_COUNT; i++)
        if(IBC_FAMILIES[i].family_code == code) return &IBC_FAMILIES[i];
    return NULL;
}

// ─────────────────────────────────────────────────────────────────────────────
// Stored key entry
// ─────────────────────────────────────────────────────────────────────────────

#define IBC_KEY_DATA_MAX 16   // DS1990A = 8 bytes; future-proof to 16
#define IBC_VAULT_MAX    8
#define IBC_VAULT_PATH   "/ext/.system/calibration/IB_VAULT.bin"

typedef struct {
    uint8_t  data[IBC_KEY_DATA_MAX];
    uint8_t  data_len;
    char     type_str[16];    // from ibutton_key_get_type_string()
    char     label[20];       // "Key #N"
    char     use_case[24];    // from family lookup
    char     grade[24];       // security verdict
    bool     valid;
} IbcKey;

// ─────────────────────────────────────────────────────────────────────────────
// Scenes / Views
// ─────────────────────────────────────────────────────────────────────────────

typedef enum {
    IbcSceneRead = 0,
    IbcSceneEmulate,
    IbcSceneCount,
} IbcScene;

typedef enum {
    IbcViewRead = 0,
    IbcViewEmulate,
} IbcView;

typedef enum {
    IbcEventKeyRead    = 0x80,
    IbcEventReadError,
    IbcEventBack,
    IbcEventSelect,
} IbcEvent;

// ─────────────────────────────────────────────────────────────────────────────
// App state
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    Gui*             gui;
    NotificationApp* notifications;
    SceneManager*    scene_manager;
    ViewDispatcher*  view_dispatcher;
    View*            read_view;
    View*            emulate_view;

    IButtonWorker*   worker;
    IButtonKey*      key;          // current key being read / emulated

    // Last read key
    IbcKey           current;
    bool             has_current;

    // Vault
    IbcKey           vault[IBC_VAULT_MAX];
    uint8_t          vault_count;
    uint8_t          vault_sel;    // index into vault for emulate scene
    uint32_t         emulate_count; // touch-to-reader counter
} IbcApp;

// ─────────────────────────────────────────────────────────────────────────────
// RC4 vault encryption
// ─────────────────────────────────────────────────────────────────────────────

#define IBC_VAULT_SALT 0x69427443UL  // "iBtC"

static inline void ibc_rc4(uint8_t* buf, uint32_t len, const uint8_t* key, uint8_t klen) {
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

extern const SceneManagerHandlers ibc_scene_handlers;

void ibc_scene_read_on_enter(void* ctx);
bool ibc_scene_read_on_event(void* ctx, SceneManagerEvent ev);
void ibc_scene_read_on_exit(void* ctx);

void ibc_scene_emulate_on_enter(void* ctx);
bool ibc_scene_emulate_on_event(void* ctx, SceneManagerEvent ev);
void ibc_scene_emulate_on_exit(void* ctx);
