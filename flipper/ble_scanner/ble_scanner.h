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
#include <bt/bt_service/bt.h>
#include <gap.h>

// ─────────────────────────────────────────────────────────────────────────────
// Device entry
// ─────────────────────────────────────────────────────────────────────────────

#define BS_DEVICE_MAX   48   // max tracked devices
#define BS_NAME_LEN     20
#define BS_MFR_LEN      18

// MAC address type
typedef enum {
    BsMacPublic = 0,  // fixed hardware MAC — trackable
    BsMacRandom,      // random/private — privacy-preserving
} BsMacType;

// Rough device category from AD type flags + appearance
typedef enum {
    BsDevUnknown = 0,
    BsDevPhone,
    BsDevLaptop,
    BsDevHeadset,
    BsDevSmartwatch,
    BsDevBeacon,
    BsDevSensor,
    BsDevTV,
    BsDevCount,
} BsDevCategory;

typedef struct {
    uint8_t      addr[6];
    BsMacType    mac_type;
    BsDevCategory category;
    char         name[BS_NAME_LEN];
    char         mfr[BS_MFR_LEN];    // from company ID in ad payload
    int8_t       rssi;               // dBm
    int8_t       rssi_peak;
    uint32_t     last_seen_tick;     // furi_get_tick() at last advertisement
    uint32_t     seen_count;         // total advertisements received
    bool         connectable;
    bool         valid;
} BsDevice;

// ─────────────────────────────────────────────────────────────────────────────
// OUI / company ID lookup (abbreviated — top 30 most common)
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    uint16_t company_id;
    const char* name;
} BsCompany;

#define BS_COMPANY_COUNT 28

static const BsCompany BS_COMPANIES[BS_COMPANY_COUNT] = {
    { 0x004C, "Apple" },
    { 0x0006, "Microsoft" },
    { 0x00E0, "Google" },
    { 0x0157, "Samsung" },
    { 0x0075, "Samsung" },
    { 0x038F, "Samsung" },
    { 0x0310, "Xiaomi" },
    { 0x038A, "Xiaomi" },
    { 0x0499, "Ruuvi Innov." },
    { 0x004D, "Qualcomm" },
    { 0x0056, "Sony" },
    { 0x0036, "Motorola" },
    { 0x0046, "LG Elec." },
    { 0x01D7, "Lenovo" },
    { 0x008C, "Intel" },
    { 0x00F0, "SONY Mob." },
    { 0x0059, "Nordic Semi." },
    { 0x0171, "Amazon" },
    { 0x052E, "Amazon" },
    { 0x004E, "Intel" },
    { 0x02FF, "Huawei" },
    { 0x0089, "Jabra/GN" },
    { 0x0A8C, "Tile" },
    { 0x0112, "Fitbit" },
    { 0x009E, "Garmin" },
    { 0x01D9, "Bose" },
    { 0x0171, "Amazon Echo" },
    { 0x0131, "Texas Instr." },
};

static inline const char* bs_company_name(uint16_t cid) {
    for(uint8_t i = 0; i < BS_COMPANY_COUNT; i++)
        if(BS_COMPANIES[i].company_id == cid)
            return BS_COMPANIES[i].name;
    return NULL;
}

// ─────────────────────────────────────────────────────────────────────────────
// Scenes / Views
// ─────────────────────────────────────────────────────────────────────────────

typedef enum {
    BsSceneScan = 0,
    BsSceneDetail,
    BsSceneStats,
    BsSceneCount,
} BsScene;

typedef enum {
    BsViewScan = 0,
    BsViewDetail,
    BsViewStats,
} BsView;

typedef enum {
    BsEventDeviceFound = 0x60,
    BsEventBack,
    BsEventNext,
    BsEventPrev,
    BsEventSelect,
    BsEventFilter,
} BsEvent;

// ─────────────────────────────────────────────────────────────────────────────
// App state
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    Gui*             gui;
    NotificationApp* notifications;
    SceneManager*    scene_manager;
    ViewDispatcher*  view_dispatcher;
    View*            scan_view;
    View*            detail_view;
    View*            stats_view;

    // Device table (sorted by RSSI)
    BsDevice  devices[BS_DEVICE_MAX];
    uint8_t   device_count;
    uint8_t   list_sel;      // selected device in scan list
    uint8_t   detail_idx;    // device being detailed

    // Stats
    uint32_t  total_seen;    // total advertisement frames
    uint32_t  public_count;  // fixed-MAC devices
    uint32_t  random_count;  // random-MAC devices
    uint8_t   category_counts[BsDevCount];

    // Filter: 0=all, 1=public only, 2=random only
    uint8_t   filter;
    bool      scanning;
} BsApp;

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static inline const char* bs_category_icon(BsDevCategory c) {
    switch(c) {
    case BsDevPhone:     return "PHN";
    case BsDevLaptop:    return "LAP";
    case BsDevHeadset:   return "EAR";
    case BsDevSmartwatch:return "WCH";
    case BsDevBeacon:    return "BCN";
    case BsDevSensor:    return "SNS";
    case BsDevTV:        return " TV";
    default:             return "UNK";
    }
}

static inline const char* bs_category_name(BsDevCategory c) {
    switch(c) {
    case BsDevPhone:     return "Phone/Tablet";
    case BsDevLaptop:    return "Laptop/PC";
    case BsDevHeadset:   return "Headset/Audio";
    case BsDevSmartwatch:return "Smartwatch";
    case BsDevBeacon:    return "Beacon/AirTag";
    case BsDevSensor:    return "Sensor/IoT";
    case BsDevTV:        return "Smart TV";
    default:             return "Unknown Device";
    }
}

// RSSI → signal bar level 0-4
static inline uint8_t bs_rssi_bars(int8_t rssi) {
    if(rssi > -55) return 4;
    if(rssi > -65) return 3;
    if(rssi > -75) return 2;
    if(rssi > -85) return 1;
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Scene declarations
// ─────────────────────────────────────────────────────────────────────────────

extern const SceneManagerHandlers bs_scene_handlers;

void bs_scene_scan_on_enter(void* ctx);
bool bs_scene_scan_on_event(void* ctx, SceneManagerEvent ev);
void bs_scene_scan_on_exit(void* ctx);

void bs_scene_detail_on_enter(void* ctx);
bool bs_scene_detail_on_event(void* ctx, SceneManagerEvent ev);
void bs_scene_detail_on_exit(void* ctx);

void bs_scene_stats_on_enter(void* ctx);
bool bs_scene_stats_on_event(void* ctx, SceneManagerEvent ev);
void bs_scene_stats_on_exit(void* ctx);
