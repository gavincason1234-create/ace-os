#pragma once

#include "../signalstack_nfc.h"

// ── Custom event IDs shared across scenes ─────────────────────────────────────
typedef enum {
    SignalStackEventScanStart    = 0,
    SignalStackEventScanStop,
    SignalStackEventTagFound,
    SignalStackEventTagLost,
    SignalStackEventSaved,
    SignalStackEventBack,
    SignalStackEventMenuMain     = 10,
    SignalStackEventMenuScan,
    SignalStackEventMenuHistory,
    SignalStackEventMenuSettings,
    SignalStackEventMenuAbout,
    SignalStackEventMenuUhfBridge,
    SignalStackEventSettingsAutoSave,
    SignalStackEventSettingsVibrate,
    SignalStackEventSettingsBeep,
    SignalStackEventSettingsSaveRaw,
    SignalStackEventSplashDone   = 30,
} SignalStackCustomEvent;

// ── Scan view callbacks (implemented in scene_scan.c) ─────────────────────────
void signalstack_scan_view_draw(Canvas* canvas, void* model);
bool signalstack_scan_view_input(InputEvent* event, void* ctx);

// ── UHF view callbacks (implemented in scene_uhf.c) ──────────────────────────
void uhf_view_draw(Canvas* canvas, void* model);
bool uhf_view_input(InputEvent* event, void* ctx);

// ── Scene on_enter / on_event / on_exit prototypes ────────────────────────────
void signalstack_scene_splash_on_enter(void* ctx);
bool signalstack_scene_splash_on_event(void* ctx, SceneManagerEvent event);
void signalstack_scene_splash_on_exit(void* ctx);

void signalstack_scene_main_menu_on_enter(void* ctx);
bool signalstack_scene_main_menu_on_event(void* ctx, SceneManagerEvent event);
void signalstack_scene_main_menu_on_exit(void* ctx);

void signalstack_scene_scan_on_enter(void* ctx);
bool signalstack_scene_scan_on_event(void* ctx, SceneManagerEvent event);
void signalstack_scene_scan_on_exit(void* ctx);

void signalstack_scene_result_on_enter(void* ctx);
bool signalstack_scene_result_on_event(void* ctx, SceneManagerEvent event);
void signalstack_scene_result_on_exit(void* ctx);

void signalstack_scene_history_on_enter(void* ctx);
bool signalstack_scene_history_on_event(void* ctx, SceneManagerEvent event);
void signalstack_scene_history_on_exit(void* ctx);

void signalstack_scene_history_detail_on_enter(void* ctx);
bool signalstack_scene_history_detail_on_event(void* ctx, SceneManagerEvent event);
void signalstack_scene_history_detail_on_exit(void* ctx);

void signalstack_scene_settings_on_enter(void* ctx);
bool signalstack_scene_settings_on_event(void* ctx, SceneManagerEvent event);
void signalstack_scene_settings_on_exit(void* ctx);

void signalstack_scene_about_on_enter(void* ctx);
bool signalstack_scene_about_on_event(void* ctx, SceneManagerEvent event);
void signalstack_scene_about_on_exit(void* ctx);

void signalstack_scene_uhf_on_enter(void* ctx);
bool signalstack_scene_uhf_on_event(void* ctx, SceneManagerEvent event);
void signalstack_scene_uhf_on_exit(void* ctx);

extern const SceneManagerHandlers signalstack_scene_handlers;
