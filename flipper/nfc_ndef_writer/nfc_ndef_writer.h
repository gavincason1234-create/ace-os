#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <notification/notification_messages.h>
#include <nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight_poller.h>
#include <string.h>
#include <stdio.h>

// ─────────────────────────────────────────────────────────────────────────────
// NDEF payload presets
// ─────────────────────────────────────────────────────────────────────────────

// Max NDEF payload bytes (NTAG213 has 137 bytes user data)
#define NW_NDEF_MAX   137
// First user-data page on NTAG213/215/216 (pages 0-3 are UID/config/OTP)
#define NW_FIRST_PAGE 4

typedef enum {
    NwPayloadUrl = 0,    // URI Well-Known record → https://...
    NwPayloadWifi,       // WiFi tap-to-connect text record
    NwPayloadVcard,      // MIME text/vcard
    NwPayloadText,       // Plain text Well-Known record
    NwPayloadCount,
} NwPayloadType;

typedef struct {
    NwPayloadType type;
    const char*   label;      // short menu label
    const char*   icon_str;   // 3-char tag displayed in list
    const char*   desc;       // one-line description
    const char*   preview;    // shown in write scene
} NwPreset;

#define NW_PRESET_COUNT 4
static const NwPreset NW_PRESETS[NW_PRESET_COUNT] = {
    {
        NwPayloadUrl,
        "URL Redirect",
        "URL",
        "Redirect tap to phishing URL",
        "https://signalstack.io/demo",
    },
    {
        NwPayloadWifi,
        "WiFi Credential",
        "WFI",
        "Auto-connect victim to rogue AP",
        "WIFI:T:WPA;S:FreeHotelWifi;P:Pa$$w0rd;;",
    },
    {
        NwPayloadVcard,
        "Fake vCard",
        "VCR",
        "Social-eng: tap to add fake contact",
        "IT Helpdesk / John Smith",
    },
    {
        NwPayloadText,
        "Text Message",
        "TXT",
        "Plain text — instructions / lure",
        "AUTHORIZED SECURITY TEST - SignalStack",
    },
};

// ─────────────────────────────────────────────────────────────────────────────
// NDEF builder helpers
// ─────────────────────────────────────────────────────────────────────────────

// Returns total bytes written to buf (including TLV wrapper + terminator)
static inline uint16_t nw_build_url(uint8_t* buf, uint16_t cap) {
    // URL = "signalstack.io/demo" (prefix 0x04 = "https://")
    static const char url[] = "signalstack.io/demo";
    uint8_t ulen = (uint8_t)strlen(url);
    uint8_t payload_len = (uint8_t)(1 + ulen);   // prefix + url
    // NDEF record: D1 01 PL 55 04 <url>
    uint8_t rec_len = (uint8_t)(5 + ulen);        // header(1)+typelen(1)+payloadlen(1)+type(1)+prefix(1)+url
    if(rec_len + 3 > cap) return 0;               // 0x03 + LL + ... + 0xFE
    uint8_t i = 0;
    buf[i++] = 0x03;           // NDEF TLV
    buf[i++] = rec_len;        // NDEF message length
    buf[i++] = 0xD1;           // MB|ME|SR, TNF=1 (Well-Known)
    buf[i++] = 0x01;           // Type length
    buf[i++] = payload_len;    // Payload length
    buf[i++] = 0x55;           // Type 'U' (URI)
    buf[i++] = 0x04;           // URI prefix "https://"
    memcpy(buf + i, url, ulen);
    i += ulen;
    buf[i++] = 0xFE;           // Terminator TLV
    return i;
}

static inline uint16_t nw_build_wifi(uint8_t* buf, uint16_t cap) {
    // WiFi tap-to-connect: plain text record containing WPA config string
    static const char wifi[] = "WIFI:T:WPA;S:FreeHotelWifi;P:Pa$$w0rd;;";
    uint8_t wlen = (uint8_t)strlen(wifi);
    // Text record: D1 01 (wlen+3) 54 02 65 6E <text>
    //   54='T', 02=status (UTF-8, 2-char lang), 65 6E = "en"
    uint8_t payload_len = (uint8_t)(3 + wlen);    // status + "en" + text
    uint8_t rec_len = (uint8_t)(4 + payload_len); // header+typelen+payloadlen+type
    if(rec_len + 3 > cap) return 0;
    uint8_t i = 0;
    buf[i++] = 0x03;
    buf[i++] = rec_len;
    buf[i++] = 0xD1;           // MB|ME|SR, TNF=Well-Known
    buf[i++] = 0x01;           // Type length
    buf[i++] = payload_len;
    buf[i++] = 0x54;           // Type 'T' (Text)
    buf[i++] = 0x02;           // Status: UTF-8, 2-char lang
    buf[i++] = 'e';
    buf[i++] = 'n';
    memcpy(buf + i, wifi, wlen);
    i += wlen;
    buf[i++] = 0xFE;
    return i;
}

static inline uint16_t nw_build_vcard(uint8_t* buf, uint16_t cap) {
    // Minimal vCard 3.0 — IT helpdesk lure
    static const char vcard[] =
        "BEGIN:VCARD\r\n"
        "VERSION:3.0\r\n"
        "FN:John Smith\r\n"
        "ORG:IT Helpdesk\r\n"
        "TEL:+1-555-0199\r\n"
        "EMAIL:helpdesk@corp-internal.com\r\n"
        "NOTE:Tap to save IT contact\r\n"
        "END:VCARD\r\n";
    static const char mime[] = "text/vcard";
    uint8_t vlen = (uint8_t)strlen(vcard);
    uint8_t mlen = (uint8_t)strlen(mime);
    // MIME record: header flags=0x12 (SR,TNF=MIME), type_len, payload_len, type, payload
    uint8_t rec_len = (uint8_t)(3 + mlen + vlen); // header+typelen+payloadlen+type+payload
    if(rec_len + 3 > cap) return 0;
    uint8_t i = 0;
    buf[i++] = 0x03;
    buf[i++] = rec_len;
    buf[i++] = 0xD2;           // MB|ME|SR, TNF=2 (MIME)
    buf[i++] = mlen;           // Type length
    buf[i++] = vlen;           // Payload length
    memcpy(buf + i, mime, mlen); i += mlen;
    memcpy(buf + i, vcard, vlen); i += vlen;
    buf[i++] = 0xFE;
    return i;
}

static inline uint16_t nw_build_text(uint8_t* buf, uint16_t cap) {
    static const char txt[] = "AUTHORIZED SECURITY TEST - SignalStack";
    uint8_t tlen = (uint8_t)strlen(txt);
    uint8_t payload_len = (uint8_t)(3 + tlen);
    uint8_t rec_len = (uint8_t)(4 + payload_len);
    if(rec_len + 3 > cap) return 0;
    uint8_t i = 0;
    buf[i++] = 0x03;
    buf[i++] = rec_len;
    buf[i++] = 0xD1;
    buf[i++] = 0x01;
    buf[i++] = payload_len;
    buf[i++] = 0x54;           // 'T'
    buf[i++] = 0x02;
    buf[i++] = 'e'; buf[i++] = 'n';
    memcpy(buf + i, txt, tlen); i += tlen;
    buf[i++] = 0xFE;
    return i;
}

static inline uint16_t nw_build_ndef(NwPayloadType type, uint8_t* buf, uint16_t cap) {
    switch(type) {
    case NwPayloadUrl:   return nw_build_url(buf, cap);
    case NwPayloadWifi:  return nw_build_wifi(buf, cap);
    case NwPayloadVcard: return nw_build_vcard(buf, cap);
    case NwPayloadText:  return nw_build_text(buf, cap);
    default:             return 0;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Write state
// ─────────────────────────────────────────────────────────────────────────────

typedef enum {
    NwWriteStateWaiting = 0,
    NwWriteStateDetected,
    NwWriteStateWriting,
    NwWriteStateDone,
    NwWriteStateError,
    NwWriteStateProtected,
} NwWriteState;

// ─────────────────────────────────────────────────────────────────────────────
// Scenes / Views
// ─────────────────────────────────────────────────────────────────────────────

typedef enum {
    NwSceneMenu = 0,
    NwSceneWrite,
    NwSceneCount,
} NwScene;

typedef enum {
    NwViewMenu = 0,
    NwViewWrite,
} NwView;

typedef enum {
    NwEventPayloadSelected = 0xB0,
    NwEventTagDetected,
    NwEventWriteDone,
    NwEventWriteError,
    NwEventWriteProtected,
    NwEventBack,
} NwEvent;

// ─────────────────────────────────────────────────────────────────────────────
// App state
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    Gui*             gui;
    NotificationApp* notifications;
    SceneManager*    scene_manager;
    ViewDispatcher*  view_dispatcher;
    View*            menu_view;
    View*            write_view;

    Nfc*             nfc;
    NfcPoller*       poller;

    NwPayloadType    payload_type;
    NwWriteState     write_state;

    // NDEF bytes to write (built once before write starts)
    uint8_t          ndef_buf[NW_NDEF_MAX];
    uint16_t         ndef_len;

    // Result info
    char             tag_type[20];   // "NTAG213", "NTAG215", etc.
    uint32_t         write_count;    // pages written
} NwApp;

// ─────────────────────────────────────────────────────────────────────────────
// Scene declarations
// ─────────────────────────────────────────────────────────────────────────────

extern const SceneManagerHandlers nw_scene_handlers;

void nw_scene_menu_on_enter(void* ctx);
bool nw_scene_menu_on_event(void* ctx, SceneManagerEvent ev);
void nw_scene_menu_on_exit(void* ctx);

void nw_scene_write_on_enter(void* ctx);
bool nw_scene_write_on_event(void* ctx, SceneManagerEvent ev);
void nw_scene_write_on_exit(void* ctx);
