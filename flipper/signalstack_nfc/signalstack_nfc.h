#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/modules/text_box.h>
#include <gui/modules/variable_item_list.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/protocols/nfc_protocol.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/iso14443_3b/iso14443_3b.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a.h>
#include <nfc/protocols/iso15693_3/iso15693_3.h>
#include <nfc/protocols/felica/felica.h>
#include <nfc/protocols/mf_classic/mf_classic.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight.h>
#include <nfc/protocols/mf_desfire/mf_desfire.h>
#include <nfc/nfc_poller.h>
#include <toolbox/stream/stream.h>
#include <toolbox/stream/file_stream.h>
#include <datetime/datetime.h>

#define SIGNALSTACK_APP_PATH_PREFIX EXT_PATH("SignalStack")
#define SIGNALSTACK_LOG_FILE        EXT_PATH("SignalStack/scan_log.csv")
#define SIGNALSTACK_NFC_DIR         EXT_PATH("SignalStack/nfc")
#define SIGNALSTACK_MAX_HISTORY     200
#define SIGNALSTACK_UID_MAX_LEN     10
#define SIGNALSTACK_INFO_BUF_SIZE   512

// ── Scene IDs ────────────────────────────────────────────────────────────────
typedef enum {
    SignalStackSceneSplash,
    SignalStackSceneMainMenu,
    SignalStackSceneScan,
    SignalStackSceneResult,
    SignalStackSceneHistory,
    SignalStackSceneHistoryDetail,
    SignalStackSceneSettings,
    SignalStackSceneAbout,
    SignalStackSceneCount,
} SignalStackScene;

// ── View IDs ─────────────────────────────────────────────────────────────────
typedef enum {
    SignalStackViewSplash,
    SignalStackViewSubmenu,
    SignalStackViewScan,
    SignalStackViewResult,
    SignalStackViewHistory,
    SignalStackViewTextBox,
    SignalStackViewVarList,
    SignalStackViewCount,
} SignalStackView;

// ── Tag protocols the app tracks ─────────────────────────────────────────────
typedef enum {
    TagProtoUnknown = 0,
    TagProtoISO14443A,
    TagProtoISO14443B,
    TagProtoISO14443_4A,
    TagProtoISO15693,
    TagProtoFeliCa,
    TagProtoMifareClassic,
    TagProtoMifareUltralight,
    TagProtoDESFire,
    TagProtoCount,
} TagProtocol;

// ── Single scan result ────────────────────────────────────────────────────────
typedef struct {
    TagProtocol  protocol;
    char         proto_name[32];
    uint8_t      uid[SIGNALSTACK_UID_MAX_LEN];
    uint8_t      uid_len;
    char         uid_str[32];         // hex string
    char         extra[256];          // ATQA/SAK, DSFID, AFI, etc.
    char         timestamp[20];       // YYYY-MM-DD HH:MM:SS
    uint32_t     unix_ts;
    bool         saved;
} SignalStackScanResult;

// ── Application state ─────────────────────────────────────────────────────────
typedef struct {
    // Core GUI
    Gui*               gui;
    ViewDispatcher*    view_dispatcher;
    SceneManager*      scene_manager;
    NotificationApp*   notifications;

    // Views
    Submenu*           submenu;
    Widget*            widget_splash;
    Widget*            widget_result;
    TextBox*           text_box;
    VariableItemList*  var_list;

    // Custom scan view
    View*              scan_view;

    // NFC
    Nfc*               nfc;
    NfcDevice*         nfc_device;
    NfcPoller*         poller;

    // Storage
    Storage*           storage;

    // State
    SignalStackScanResult  last_scan;
    SignalStackScanResult  history[SIGNALSTACK_MAX_HISTORY];
    uint32_t               history_count;
    uint32_t               history_selected;
    bool                   scanning;
    bool                   scan_found;

    // Settings
    bool               auto_save;
    bool               vibrate_on_scan;
    bool               beep_on_scan;
    bool               save_raw_nfc;

    // Stats
    uint32_t           total_scans;
    uint32_t           session_scans;
} SignalStackApp;

// ── Forward declarations ──────────────────────────────────────────────────────
SignalStackApp* signalstack_app_alloc(void);
void            signalstack_app_free(SignalStackApp* app);

// Helpers
void            signalstack_uid_to_str(const uint8_t* uid, uint8_t len, char* out);
void            signalstack_get_timestamp(char* out, uint32_t* unix_out);
void            signalstack_save_to_log(SignalStackApp* app, const SignalStackScanResult* r);
void            signalstack_save_nfc_file(SignalStackApp* app, const SignalStackScanResult* r);
void            signalstack_load_history_from_log(SignalStackApp* app);
const char*     signalstack_proto_name(TagProtocol p);
const char*     signalstack_proto_description(TagProtocol p);
