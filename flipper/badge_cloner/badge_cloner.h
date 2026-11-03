#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/popup.h>
#include <notification/notification_messages.h>
#include <lib/lfrfid/lfrfid_worker.h>
#include <lib/lfrfid/protocols/lfrfid_protocol.h>
#include <storage/storage.h>

// ─────────────────────────────────────────────────────────────────────────────
// Badge card types we handle
// ─────────────────────────────────────────────────────────────────────────────

typedef enum {
    BcCardUnknown = 0,
    BcCardEM4100,      // 64-bit: 8-byte, standard EM/HID-EM
    BcCardHIDProx,     // HID 26-bit Proximity (H10301)
    BcCardHIDCorp1000, // HID Corporate 1000 (35-bit)
    BcCardIOProx,      // Kantech IO Prox (XSF)
    BcCardIndala,      // Motorola/Indala 64-bit
    BcCardFDX,         // FDX-B (ISO 11784/85, animal microchips)
    BcCardCount,
} BcCardType;

// ─────────────────────────────────────────────────────────────────────────────
// Stored badge record
// ─────────────────────────────────────────────────────────────────────────────

#define BC_DATA_MAX 8
#define BC_SAVED_MAX 8
#define BC_LABEL_LEN 20

typedef struct {
    BcCardType type;
    uint8_t    data[BC_DATA_MAX];
    uint8_t    data_len;
    char       label[BC_LABEL_LEN]; // user-editable nickname
    bool       valid;
} BcBadge;

// ─────────────────────────────────────────────────────────────────────────────
// Scenes / Views
// ─────────────────────────────────────────────────────────────────────────────

typedef enum {
    BcSceneMenu = 0,
    BcSceneRead,
    BcSceneVault,
    BcSceneEmulate,
    BcSceneCount,
} BcScene;

typedef enum {
    BcViewSubmenu = 0,
    BcViewRead,
    BcViewVault,
    BcViewEmulate,
} BcView;

typedef enum {
    BcEventCardRead   = 0x20,
    BcEventSaveOk,
    BcEventBack,
    BcEventEmulStart,
    BcEventEmulStop,
} BcEvent;

// ─────────────────────────────────────────────────────────────────────────────
// App state
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    Gui*             gui;
    NotificationApp* notifications;
    SceneManager*    scene_manager;
    ViewDispatcher*  view_dispatcher;
    Submenu*         submenu;
    View*            read_view;
    View*            vault_view;
    View*            emulate_view;

    LFRFIDWorker*    worker;

    // Currently-read badge (live)
    BcBadge current;

    // Vault of saved badges (RAM — persisted to stealth file on save)
    BcBadge saved[BC_SAVED_MAX];
    uint8_t saved_count;

    // Which saved badge is selected in vault/emulate
    uint8_t vault_sel;

    // Emulation running?
    bool emulating;
} BcApp;

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static inline const char* bc_card_name(BcCardType t) {
    switch(t) {
    case BcCardEM4100:      return "EM4100";
    case BcCardHIDProx:     return "HID Prox";
    case BcCardHIDCorp1000: return "HID Corp1000";
    case BcCardIOProx:      return "IO Prox";
    case BcCardIndala:      return "Indala";
    case BcCardFDX:         return "FDX-B";
    default:                return "Unknown";
    }
}

// Map LFRFIDProtocol -> BcCardType
static inline BcCardType bc_proto_to_type(LFRFIDProtocol p) {
    switch(p) {
    case LFRFIDProtocolEM4100:      return BcCardEM4100;
    case LFRFIDProtocolHIDProx:     return BcCardHIDProx;
    case LFRFIDProtocolHIDCorp1000: return BcCardHIDCorp1000;
    case LFRFIDProtocolIOProx:      return BcCardIOProx;
    case LFRFIDProtocolIndala26:    return BcCardIndala;
    case LFRFIDProtocolFDXB:        return BcCardFDX;
    default:                         return BcCardUnknown;
    }
}

// Map BcCardType -> LFRFIDProtocol for emulation
static inline LFRFIDProtocol bc_type_to_proto(BcCardType t) {
    switch(t) {
    case BcCardEM4100:      return LFRFIDProtocolEM4100;
    case BcCardHIDProx:     return LFRFIDProtocolHIDProx;
    case BcCardHIDCorp1000: return LFRFIDProtocolHIDCorp1000;
    case BcCardIOProx:      return LFRFIDProtocolIOProx;
    case BcCardIndala:      return LFRFIDProtocolIndala26;
    case BcCardFDX:         return LFRFIDProtocolFDXB;
    default:                return LFRFIDProtocolEM4100;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Stealth storage path (no footprint — hides among system calibration files)
// ─────────────────────────────────────────────────────────────────────────────

#define BC_VAULT_PATH "/ext/.system/calibration/BADGE_VAULT.bin"

// ─────────────────────────────────────────────────────────────────────────────
// Scene declarations
// ─────────────────────────────────────────────────────────────────────────────

extern const SceneManagerHandlers bc_scene_handlers;

void bc_scene_menu_on_enter(void* ctx);
bool bc_scene_menu_on_event(void* ctx, SceneManagerEvent ev);
void bc_scene_menu_on_exit(void* ctx);

void bc_scene_read_on_enter(void* ctx);
bool bc_scene_read_on_event(void* ctx, SceneManagerEvent ev);
void bc_scene_read_on_exit(void* ctx);

void bc_scene_vault_on_enter(void* ctx);
bool bc_scene_vault_on_event(void* ctx, SceneManagerEvent ev);
void bc_scene_vault_on_exit(void* ctx);

void bc_scene_emulate_on_enter(void* ctx);
bool bc_scene_emulate_on_event(void* ctx, SceneManagerEvent ev);
void bc_scene_emulate_on_exit(void* ctx);
