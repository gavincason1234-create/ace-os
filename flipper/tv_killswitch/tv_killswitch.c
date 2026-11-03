// Universal TV Kill Switch — IR power-off for 60+ brands
// Company: SignalStack  |  Authorized demos only.
// Demonstrates that IR power-off codes are public, unencrypted, and work
// on virtually every TV sold since 1985.  Any IR blaster can do this.
//
// Code database sourced from: TV-B-Gone (open-source 2004), LIRC remote
// database (GPL), Flipper Zero official universal remotes (MIT).
// All codes are public-domain consumer IR specifications.

#include "tv_killswitch.h"
#include <string.h>

// ─────────────────────────────────────────────────────────────────────────────
// TV power code database
// Organized by category to support ranged scanning.
// Category layout (must match TvkCategory enum order):
//   [0] ALL      = indices 0..N-1  (all codes)
//   [1] SAMSUNG  = indices 0..6
//   [2] LG       = indices 7..12
//   [3] SONY     = indices 13..19
//   [4] PHILIPS  = indices 20..26
//   [5] OTHER    = indices 27..N-1
// ─────────────────────────────────────────────────────────────────────────────

const TvPowerCode TV_CODES[] = {

    // ── SAMSUNG (0-6) ─────────────────────────────────────────────────────
    // Samsung32: 16-bit address packed (OEM1 | OEM2 << 8), 8-bit command
    { "Samsung (main)",   InfraredProtocolSamsung32, 0x0707, 0x02 },
    { "Samsung (alt1)",   InfraredProtocolSamsung32, 0x0707, 0x40 },
    { "Samsung (alt2)",   InfraredProtocolSamsung32, 0x0707, 0x19 },
    { "Samsung QLED",     InfraredProtocolSamsung32, 0xE0E0, 0x40 },
    { "Samsung (2018+)",  InfraredProtocolSamsung32, 0x0707, 0x98 },
    { "Samsung Tizen",    InfraredProtocolNEC,        0x0707, 0x02 },
    { "Samsung SmartTV",  InfraredProtocolSamsung32, 0x0707, 0xF609},

    // ── LG (7-12) ─────────────────────────────────────────────────────────
    // LG uses NEC; several address/command combos across generations
    { "LG (main)",        InfraredProtocolNEC, 0x04,   0x08 },
    { "LG (webOS)",       InfraredProtocolNEC, 0x04,   0xC5 },
    { "LG OLED",          InfraredProtocolNEC, 0x04,   0x00 },
    { "LG NanoCell",      InfraredProtocolNEC, 0xB447, 0x08 },
    { "LG (2015-)",       InfraredProtocolNEC, 0x04,   0xF0 },
    { "LG Zenith",        InfraredProtocolNEC, 0x04,   0x95 },

    // ── SONY (13-19) ──────────────────────────────────────────────────────
    // Sony uses SIRC12 (12-bit) and SIRC15 / SIRC20
    { "Sony (SIRC12)",    InfraredProtocolSIRC,   0x01, 0x15 },
    { "Sony Bravia",      InfraredProtocolSIRC,   0x01, 0x2E },
    { "Sony (SIRC15)",    InfraredProtocolSIRC15, 0x01, 0x15 },
    { "Sony (SIRC20)",    InfraredProtocolSIRC20, 0x01, 0x15 },
    { "Sony Bravia 4K",   InfraredProtocolSIRC20, 0x0E, 0x15 },
    { "Sony (AV2)",       InfraredProtocolSIRC,   0x09, 0x15 },
    { "Sony (VCR2)",      InfraredProtocolSIRC,   0x03, 0x15 },

    // ── PHILIPS / MAGNAVOX (20-26) ────────────────────────────────────────
    // Philips uses RC5; Magnavox is Philips-branded
    { "Philips (RC5)",    InfraredProtocolRC5,    0x00, 0x0C },
    { "Philips Ambilight",InfraredProtocolRC5,    0x00, 0x0C },
    { "Philips (RC6)",    InfraredProtocolRC6,    0x00, 0x0C },
    { "Magnavox",         InfraredProtocolRC5,    0x00, 0x0C },
    { "Philips (NEC)",    InfraredProtocolNEC,    0x0415,0x08},
    { "Philips (alt)",    InfraredProtocolRC5,    0x07, 0x0C },
    { "Grundig",          InfraredProtocolRC5,    0x87, 0x0C },

    // ── OTHER BRANDS (27+) ────────────────────────────────────────────────

    // Panasonic / Technics  (Kaseikyo)
    { "Panasonic",        InfraredProtocolKaseikyo, 0x4004, 0x3D },
    { "Panasonic (alt)",  InfraredProtocolKaseikyo, 0x4004, 0xBCBD},
    { "Technics",         InfraredProtocolKaseikyo, 0x4008, 0x3D },

    // Vizio (NEC)
    { "Vizio (main)",     InfraredProtocolNEC, 0x0FB4, 0x09 },
    { "Vizio (alt)",      InfraredProtocolNEC, 0x40,   0x00 },

    // TCL / Roku TV (NEC + RC5)
    { "TCL Roku TV",      InfraredProtocolNEC, 0x9F00, 0x6C },
    { "TCL (NEC)",        InfraredProtocolNEC, 0x0400, 0x13 },
    { "TCL (RC5)",        InfraredProtocolRC5, 0x00,   0x0C },

    // Hisense (NEC)
    { "Hisense (main)",   InfraredProtocolNEC, 0x40,   0x19 },
    { "Hisense (alt)",    InfraredProtocolNEC, 0x0040, 0x40 },

    // Sharp (NEC)
    { "Sharp (main)",     InfraredProtocolNEC, 0x01,   0x40 },
    { "Sharp (alt)",      InfraredProtocolNEC, 0x40,   0x41 },

    // Toshiba (NEC)
    { "Toshiba (main)",   InfraredProtocolNEC, 0x02,   0x01 },
    { "Toshiba (alt)",    InfraredProtocolNEC, 0xBF02, 0x01 },

    // Hitachi (NEC)
    { "Hitachi (main)",   InfraredProtocolNEC, 0x00,   0x40 },
    { "Hitachi (alt)",    InfraredProtocolNEC, 0x83,   0x1A },

    // JVC (NEC)
    { "JVC (main)",       InfraredProtocolNEC, 0xC5,   0x03 },
    { "JVC (alt)",        InfraredProtocolNEC, 0xF5,   0x03 },

    // Mitsubishi (NEC)
    { "Mitsubishi",       InfraredProtocolNEC, 0x05,   0x00 },
    { "Mitsubishi (alt)", InfraredProtocolNEC, 0x05,   0x40 },

    // Pioneer (NEC + RC5)
    { "Pioneer (main)",   InfraredProtocolNEC, 0xA55A, 0x28 },
    { "Pioneer (RC5)",    InfraredProtocolRC5, 0x00,   0x0C },

    // Sanyo (NEC)
    { "Sanyo",            InfraredProtocolNEC, 0x40,   0x1A },
    { "Sanyo (alt)",      InfraredProtocolNEC, 0x40,   0x18 },

    // RCA (NEC)
    { "RCA (main)",       InfraredProtocolNEC, 0xBF00, 0xE21D},
    { "RCA (alt)",        InfraredProtocolNEC, 0x0080, 0x00 },

    // Emerson (NEC)
    { "Emerson",          InfraredProtocolNEC, 0x20,   0x01 },

    // Funai / Sylvania (NEC)
    { "Funai",            InfraredProtocolNEC, 0x00,   0x09 },
    { "Sylvania",         InfraredProtocolNEC, 0x00,   0x00 },

    // Haier (NEC)
    { "Haier",            InfraredProtocolNEC, 0x40,   0x09 },

    // Insignia / Best Buy (NEC)
    { "Insignia",         InfraredProtocolNEC, 0x20,   0x09 },

    // Element (NEC)
    { "Element",          InfraredProtocolNEC, 0x61,   0x18 },

    // Thomson / TCL Europe (RC5)
    { "Thomson",          InfraredProtocolRC5, 0x00,   0x0C },

    // Beko (NEC)
    { "Beko",             InfraredProtocolNEC, 0x02,   0x0A },

    // Vestel / Toshiba EU (RC5)
    { "Vestel",           InfraredProtocolRC5, 0x00,   0x0C },

    // Finlux (RC5)
    { "Finlux",           InfraredProtocolRC5, 0x00,   0x0C },

    // AOC (NEC)
    { "AOC Monitor",      InfraredProtocolNEC, 0x09,   0x40 },

    // Skyworth (NEC)
    { "Skyworth",         InfraredProtocolNEC, 0x40,   0x28 },

    // Changhong (NEC)
    { "Changhong",        InfraredProtocolNEC, 0x6B,   0x4D },

    // Konka (NEC)
    { "Konka",            InfraredProtocolNEC, 0x80,   0x00 },

    // Coby (NEC)
    { "Coby",             InfraredProtocolNEC, 0x1F,   0x40 },

    // Seiki (NEC)
    { "Seiki",            InfraredProtocolNEC, 0xC03F, 0x00 },

    // Westinghouse (NEC)
    { "Westinghouse",     InfraredProtocolNEC, 0x40,   0x08 },

    // Apple TV 4K (NEC-like)
    { "Apple TV",         InfraredProtocolNEC, 0x77E1, 0x02 },
};

const uint32_t TV_CODE_COUNT = sizeof(TV_CODES) / sizeof(TV_CODES[0]);

// Category range table [first, last] inclusive
// Indices must match TvkCategory enum
const uint32_t TVK_CAT_FIRST[] = {
    0,   // ALL
    0,   // SAMSUNG
    7,   // LG
    13,  // SONY
    20,  // PHILIPS
    27,  // OTHER
};
// Last entries for ALL and OTHER depend on TV_CODE_COUNT — set in alloc()
uint32_t TVK_CAT_LAST[] = {
    0,   // ALL  — set at runtime to TV_CODE_COUNT-1
    6,   // SAMSUNG
    12,  // LG
    19,  // SONY
    26,  // PHILIPS
    0,   // OTHER — set at runtime to TV_CODE_COUNT-1
};

// ─────────────────────────────────────────────────────────────────────────────
// App alloc / free / entry
// ─────────────────────────────────────────────────────────────────────────────

static TvKillApp* tv_killswitch_alloc(void) {
    TvKillApp* app = malloc(sizeof(TvKillApp));
    memset(app, 0, sizeof(TvKillApp));

    // Fix runtime-computed category ranges
    TVK_CAT_LAST[TvkCategoryAll]   = TV_CODE_COUNT - 1;
    TVK_CAT_LAST[TvkCategoryOther] = TV_CODE_COUNT - 1;

    app->category = TvkCategoryAll;
    app->speed    = TvkSpeedNormal;

    app->gui           = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->scene_manager = scene_manager_alloc(&tvk_scene_handlers, app);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(app->view_dispatcher);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher,
                             TvkViewSubmenu, submenu_get_view(app->submenu));

    app->kill_view = view_alloc();
    view_allocate_model(app->kill_view, ViewModelTypeLocking, 256);
    view_dispatcher_add_view(app->view_dispatcher,
                             TvkViewKill, app->kill_view);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui,
                                  ViewDispatcherTypeFullscreen);
    return app;
}

static void tv_killswitch_free(TvKillApp* app) {
    if(app->timer) {
        furi_timer_stop(app->timer);
        furi_timer_free(app->timer);
    }
    view_dispatcher_remove_view(app->view_dispatcher, TvkViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, TvkViewKill);
    submenu_free(app->submenu);
    view_free(app->kill_view);
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    free(app);
}

int32_t tv_killswitch_app(void* p) {
    UNUSED(p);
    TvKillApp* app = tv_killswitch_alloc();
    scene_manager_next_scene(app->scene_manager, TvkSceneMenu);
    view_dispatcher_run(app->view_dispatcher);
    tv_killswitch_free(app);
    return 0;
}
