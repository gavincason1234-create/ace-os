#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_bt.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/modules/text_box.h>
#include <gui/modules/variable_item_list.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <toolbox/stream/stream.h>
#include <toolbox/stream/file_stream.h>
#include <furi_hal_rtc.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
#include <nfc/protocols/iso15693_3/iso15693_3.h>
#include <nfc/protocols/iso15693_3/iso15693_3_poller.h>
#include <storage/storage.h>
#include <lib/nfc/nfc_poller.h>

// ── App paths ─────────────────────────────────────────────────────────────────
#define GSA_APP_DIR       EXT_PATH("SignalStack/GasAudit")
#define GSA_LOG_FILE      EXT_PATH("SignalStack/GasAudit/audit_log.csv")

// ── BT skimmer — known module names & MAC OUI prefixes ────────────────────────
// Based on USENIX 2019 research + SparkFun + law enforcement reports
#define GSA_SKIMMER_NAME_COUNT 14
static const char* const GSA_SKIMMER_NAMES[GSA_SKIMMER_NAME_COUNT] = {
    "HC-05",
    "HC-06",
    "HC-08",
    "JDY-30",
    "JDY-31",
    "JDY-33",
    "RNBT",       // Roving Networks RN41/42 prefix
    "linvor",
    "Bolutek",
    "FireFly",    // RN41 legacy
    "slave",      // HC-05 default in slave mode
    "BT-CARD",
    "SKIMMER",
    "FREE",       // some skimmers named "FREE" to attract connections
};

// MAC OUI prefixes for known skimmer modules (first 3 bytes)
#define GSA_SKIMMER_OUI_COUNT 4
static const uint8_t GSA_SKIMMER_OUIS[GSA_SKIMMER_OUI_COUNT][3] = {
    {0x00, 0x06, 0x66},  // Roving Networks RN41/42
    {0x20, 0xC3, 0x8F},  // HC-05/06 common OUI
    {0xE0, 0x98, 0x06},  // JDY modules
    {0x00, 0x07, 0x80},  // Bluegiga (used in some skimmers)
};

// ── Limits ────────────────────────────────────────────────────────────────────
#define GSA_MAX_BT_DEVICES   32
#define GSA_MAX_NFC_SCANS     8
#define GSA_UID_MAX_LEN      10
#define GSA_LOG_LINE_MAX    256

// ── BT device record ──────────────────────────────────────────────────────────
typedef struct {
    char     name[32];
    uint8_t  mac[6];
    int8_t   rssi;          // dBm, negative
    bool     name_match;    // name in skimmer list
    bool     oui_match;     // MAC prefix in skimmer OUI list
    bool     flagged;       // either match = suspicious
} GsaBtDevice;

// ── NFC/RFID scan result ──────────────────────────────────────────────────────
typedef enum {
    GsaNfcProtoUnknown = 0,
    GsaNfcProtoISO14443A,
    GsaNfcProtoISO15693,
} GsaNfcProto;

typedef struct {
    GsaNfcProto proto;
    uint8_t     uid[GSA_UID_MAX_LEN];
    uint8_t     uid_len;
    char        uid_str[32];
    char        info[128];   // decoded card type / fleet card info
    bool        cloneable;   // true if card uses no crypto
    char        grade;       // 'A'-'F' security grade
} GsaNfcResult;

// ── Sub-GHz monitor event ─────────────────────────────────────────────────────
typedef struct {
    uint32_t frequency;
    int8_t   rssi;
    uint32_t timestamp;
    char     note[32];
} GsaRfEvent;

#define GSA_MAX_RF_EVENTS 16

// ── Pump security report ──────────────────────────────────────────────────────
typedef struct {
    uint8_t     pump_score;        // 0-100
    char        pump_grade;        // A-F
    uint8_t     bt_flagged_count;
    uint8_t     bt_total_count;
    uint8_t     nfc_cloneable_count;
    uint8_t     nfc_total_count;
    uint8_t     rf_signals_count;
    char        timestamp[24];
    char        summary[256];
} GsaReport;

// ── Scenes & views ────────────────────────────────────────────────────────────
typedef enum {
    GsaSceneSplash = 0,
    GsaSceneMainMenu,
    GsaSceneBtScan,
    GsaSceneNfcScan,
    GsaSceneRfMonitor,
    GsaSceneReport,
    GsaSceneLog,
    GsaScenePriceSign,
    GsaSceneChecklist,
    GsaSceneCount,
} GsaScene;

typedef enum {
    GsaViewSplash = 0,
    GsaViewSubmenu,
    GsaViewBt,
    GsaViewNfc,
    GsaViewRf,
    GsaViewReport,
    GsaViewTextBox,
} GsaView;

typedef enum {
    GsaEventMenuBtScan = 10,
    GsaEventMenuNfcScan,
    GsaEventMenuRfMonitor,
    GsaEventMenuReport,
    GsaEventMenuLog,
    GsaEventBtDone,
    GsaEventNfcFound,
    GsaEventNfcDone,
    GsaEventRfSignal,
    GsaEventSplashDone = 30,
    GsaEventBack,
} GsaCustomEvent;

// ── App struct ────────────────────────────────────────────────────────────────
typedef struct {
    // Core services
    Gui*                  gui;
    NotificationApp*      notifications;
    Storage*              storage;

    // Scene + view dispatcher
    SceneManager*         scene_manager;
    ViewDispatcher*       view_dispatcher;

    // Views
    Widget*               widget_splash;
    Submenu*              submenu;
    View*                 bt_view;
    View*                 nfc_view;
    View*                 rf_view;
    View*                 report_view;
    TextBox*              text_box;

    // NFC
    Nfc*                  nfc;
    NfcPoller*            nfc_poller;
    bool                  nfc_scanning;

    // BT scan state
    GsaBtDevice           bt_devices[GSA_MAX_BT_DEVICES];
    uint8_t               bt_count;
    uint8_t               bt_flagged;
    bool                  bt_scanning;
    FuriTimer*            bt_timer;

    // NFC scan state
    GsaNfcResult          nfc_results[GSA_MAX_NFC_SCANS];
    uint8_t               nfc_count;

    // RF monitor state
    GsaRfEvent            rf_events[GSA_MAX_RF_EVENTS];
    uint8_t               rf_event_count;
    bool                  rf_monitoring;
    FuriTimer*            rf_timer;

    // Report
    GsaReport             report;
    bool                  report_ready;

    // Shared scan timer tick
    uint32_t              scan_tick;
} GasStationAuditApp;

// ── Scene handler externs ─────────────────────────────────────────────────────
extern const SceneManagerHandlers gsa_scene_handlers;

void gsa_scene_splash_on_enter(void* ctx);
bool gsa_scene_splash_on_event(void* ctx, SceneManagerEvent ev);
void gsa_scene_splash_on_exit(void* ctx);

void gsa_scene_main_menu_on_enter(void* ctx);
bool gsa_scene_main_menu_on_event(void* ctx, SceneManagerEvent ev);
void gsa_scene_main_menu_on_exit(void* ctx);

void gsa_scene_bt_scan_on_enter(void* ctx);
bool gsa_scene_bt_scan_on_event(void* ctx, SceneManagerEvent ev);
void gsa_scene_bt_scan_on_exit(void* ctx);

void gsa_scene_nfc_scan_on_enter(void* ctx);
bool gsa_scene_nfc_scan_on_event(void* ctx, SceneManagerEvent ev);
void gsa_scene_nfc_scan_on_exit(void* ctx);

void gsa_scene_rf_monitor_on_enter(void* ctx);
bool gsa_scene_rf_monitor_on_event(void* ctx, SceneManagerEvent ev);
void gsa_scene_rf_monitor_on_exit(void* ctx);

void gsa_scene_report_on_enter(void* ctx);
bool gsa_scene_report_on_event(void* ctx, SceneManagerEvent ev);
void gsa_scene_report_on_exit(void* ctx);

void gsa_scene_log_on_enter(void* ctx);
bool gsa_scene_log_on_event(void* ctx, SceneManagerEvent ev);
void gsa_scene_log_on_exit(void* ctx);

void gsa_scene_price_sign_on_enter(void* ctx);
bool gsa_scene_price_sign_on_event(void* ctx, SceneManagerEvent ev);
void gsa_scene_price_sign_on_exit(void* ctx);

void gsa_scene_checklist_on_enter(void* ctx);
bool gsa_scene_checklist_on_event(void* ctx, SceneManagerEvent ev);
void gsa_scene_checklist_on_exit(void* ctx);

void gsa_price_sign_view_draw(Canvas* canvas, void* model);
bool gsa_price_sign_view_input(InputEvent* ev, void* ctx);
void gsa_checklist_view_draw(Canvas* canvas, void* model);
bool gsa_checklist_view_input(InputEvent* ev, void* ctx);

// ── View draw/input ───────────────────────────────────────────────────────────
void gsa_bt_view_draw(Canvas* canvas, void* model);
bool gsa_bt_view_input(InputEvent* ev, void* ctx);
void gsa_nfc_view_draw(Canvas* canvas, void* model);
bool gsa_nfc_view_input(InputEvent* ev, void* ctx);
void gsa_rf_view_draw(Canvas* canvas, void* model);
bool gsa_rf_view_input(InputEvent* ev, void* ctx);
void gsa_report_view_draw(Canvas* canvas, void* model);
bool gsa_report_view_input(InputEvent* ev, void* ctx);

// ── Helpers ───────────────────────────────────────────────────────────────────
void gsa_uid_to_str(const uint8_t* uid, uint8_t len, char* out);
void gsa_get_timestamp(char* out);
void gsa_save_log(GasStationAuditApp* app);
bool gsa_bt_name_is_suspicious(const char* name);
bool gsa_bt_oui_is_suspicious(const uint8_t* mac);
char gsa_nfc_security_grade(GsaNfcProto proto, bool has_crypto);
void gsa_build_report(GasStationAuditApp* app);
