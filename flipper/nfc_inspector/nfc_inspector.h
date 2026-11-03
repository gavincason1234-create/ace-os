#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <notification/notification_messages.h>
#include <lib/nfc/nfc.h>
#include <lib/nfc/nfc_device.h>
#include <lib/nfc/protocols/mf_classic/mf_classic.h>
#include <lib/nfc/protocols/mf_ultralight/mf_ultralight.h>
#include <lib/nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <lib/nfc/helpers/nfc_data_generator.h>

// ─────────────────────────────────────────────────────────────────────────────
// Card classification
// ─────────────────────────────────────────────────────────────────────────────

typedef enum {
    NiCardUnknown = 0,
    NiCardMifareClassic1K,
    NiCardMifareClassic4K,
    NiCardMifareUltralight,
    NiCardMifareUltralightC,
    NiCardNTAG213,
    NiCardNTAG215,
    NiCardNTAG216,
    NiCardISO14443A,    // generic — couldn't identify further
    NiCardCount,
} NiCardType;

// ─────────────────────────────────────────────────────────────────────────────
// MIFARE Classic sector result
// ─────────────────────────────────────────────────────────────────────────────

#define NI_SECTOR_MAX    16    // MIFARE Classic 1K has 16 sectors
#define NI_BLOCK_MAX      4    // 4 blocks per sector (Classic 1K)
#define NI_BLOCK_BYTES   16

#define NI_KEY_COUNT     10    // default keys to try

typedef enum {
    NiKeyNone = 0,
    NiKeyDefault,     // cracked with default key
    NiKeyUnknown,     // encrypted with non-default key
} NiKeyStatus;

typedef struct {
    NiKeyStatus key_a_status;
    NiKeyStatus key_b_status;
    uint8_t     key_a[6];
    uint8_t     key_b[6];
    uint8_t     block_data[NI_BLOCK_MAX][NI_BLOCK_BYTES];
    bool        block_read[NI_BLOCK_MAX];
    bool        available;  // sector responded
} NiSector;

// ─────────────────────────────────────────────────────────────────────────────
// Card info
// ─────────────────────────────────────────────────────────────────────────────

#define NI_UID_MAX 10
#define NI_ATQ_LEN  2

typedef struct {
    NiCardType type;
    uint8_t    uid[NI_UID_MAX];
    uint8_t    uid_len;
    uint8_t    atqa[NI_ATQ_LEN];
    uint8_t    sak;
    // MIFARE Classic specific
    uint8_t    sector_count;
    NiSector   sectors[NI_SECTOR_MAX];
    uint8_t    cracked_sectors;   // sectors opened with default key
    uint8_t    total_sectors;
    // NTAG/Ultralight
    uint16_t   page_count;
    uint8_t    ntag_pages[256];   // raw page bytes (up to 64 pages × 4 bytes)
    bool       valid;
} NiCard;

// ─────────────────────────────────────────────────────────────────────────────
// Known default MIFARE Classic keys
// ─────────────────────────────────────────────────────────────────────────────

static const uint8_t NI_DEFAULT_KEYS[NI_KEY_COUNT][6] = {
    { 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF },  // factory default
    { 0xA0,0xA1,0xA2,0xA3,0xA4,0xA5 },  // NXP transport key A
    { 0xB0,0xB1,0xB2,0xB3,0xB4,0xB5 },  // NXP transport key B
    { 0x4D,0x3A,0x99,0xC3,0x51,0xDD },  // common hotel
    { 0x1A,0x98,0x2C,0x7E,0x45,0x9A },  // HID iCLASS equivalent
    { 0xD3,0xF7,0xD3,0xF7,0xD3,0xF7 },  // NDEF default
    { 0xAA,0xBB,0xCC,0xDD,0xEE,0xFF },  // common default #2
    { 0x00,0x00,0x00,0x00,0x00,0x00 },  // blank
    { 0xAB,0xCD,0xEF,0x12,0x34,0x56 },  // corporate common
    { 0x71,0x4C,0x5C,0x88,0x6E,0x97 },  // transit common
};

// ─────────────────────────────────────────────────────────────────────────────
// Scenes / Views
// ─────────────────────────────────────────────────────────────────────────────

typedef enum {
    NiSceneMenu = 0,
    NiSceneRead,
    NiSceneResult,
    NiSceneSectors,
    NiSceneCount,
} NiScene;

typedef enum {
    NiViewSubmenu = 0,
    NiViewRead,
    NiViewResult,
    NiViewSectors,
} NiView;

typedef enum {
    NiEventCardDetected = 0x40,
    NiEventScanDone,
    NiEventBack,
    NiEventNext,
    NiEventPrev,
} NiEvent;

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
    View*            result_view;
    View*            sectors_view;

    Nfc*             nfc;
    NfcDevice*       nfc_device;

    NiCard           card;

    // Sector browser
    uint8_t          sector_sel;
    uint8_t          block_sel;
} NiApp;

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static inline const char* ni_card_name(NiCardType t) {
    switch(t) {
    case NiCardMifareClassic1K:    return "MIFARE Classic 1K";
    case NiCardMifareClassic4K:    return "MIFARE Classic 4K";
    case NiCardMifareUltralight:   return "MIFARE Ultralight";
    case NiCardMifareUltralightC:  return "MIFARE Ultralight C";
    case NiCardNTAG213:            return "NTAG213";
    case NiCardNTAG215:            return "NTAG215";
    case NiCardNTAG216:            return "NTAG216";
    case NiCardISO14443A:          return "ISO14443-A";
    default:                       return "Unknown";
    }
}

// Security grade: based on card type + key crackability
static inline const char* ni_security_grade(const NiCard* c) {
    if(c->type == NiCardMifareClassic1K || c->type == NiCardMifareClassic4K) {
        if(c->cracked_sectors == c->total_sectors) return "GRADE: F (all keys default)";
        if(c->cracked_sectors > 0)                 return "GRADE: D (partial default)";
        return "GRADE: C (non-default keys)";
    }
    if(c->type == NiCardMifareUltralight ||
       c->type == NiCardNTAG213 ||
       c->type == NiCardNTAG215 ||
       c->type == NiCardNTAG216) {
        return "GRADE: D (no auth sectors)";
    }
    return "GRADE: ? (unknown)";
}

// ─────────────────────────────────────────────────────────────────────────────
// Scene declarations
// ─────────────────────────────────────────────────────────────────────────────

extern const SceneManagerHandlers ni_scene_handlers;

void ni_scene_menu_on_enter(void* ctx);
bool ni_scene_menu_on_event(void* ctx, SceneManagerEvent ev);
void ni_scene_menu_on_exit(void* ctx);

void ni_scene_read_on_enter(void* ctx);
bool ni_scene_read_on_event(void* ctx, SceneManagerEvent ev);
void ni_scene_read_on_exit(void* ctx);

void ni_scene_result_on_enter(void* ctx);
bool ni_scene_result_on_event(void* ctx, SceneManagerEvent ev);
void ni_scene_result_on_exit(void* ctx);

void ni_scene_sectors_on_enter(void* ctx);
bool ni_scene_sectors_on_event(void* ctx, SceneManagerEvent ev);
void ni_scene_sectors_on_exit(void* ctx);
