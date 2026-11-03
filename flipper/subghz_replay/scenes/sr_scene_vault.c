// Vault scene — browse captures, view waveform, or replay

#include "../subghz_replay.h"

typedef struct {
    uint8_t  count;
    uint8_t  sel;
    char     label[20];
    char     freq_label[14];
    char     freq_hint[24];
    uint16_t duration_count;
    uint32_t total_ms;
    uint32_t min_pulse_us;
    uint32_t max_pulse_us;
    bool     transmitting;
    uint8_t  tick;
} VaultViewModel;

static void vault_sync(SrApp* app) {
    with_view_model(app->vault_view, VaultViewModel* m, {
        m->count = app->vault_count;
        m->sel   = app->vault_sel;
        m->transmitting = app->transmitting;
        if(app->vault_count == 0) {
            strncpy(m->label, "(empty)", 19);
            m->duration_count = 0;
            m->total_ms = 0;
        } else {
            const SrCapture* cap = &app->vault[app->vault_sel];
            strncpy(m->label,      cap->label,      19);
            strncpy(m->freq_label, cap->freq_label,  13);
            strncpy(m->freq_hint,  cap->hint,        23);
            m->duration_count = cap->duration_count;
            m->total_ms       = cap->total_us / 1000;
            m->min_pulse_us   = (uint32_t)cap->min_pulse_us;
            m->max_pulse_us   = (uint32_t)cap->max_pulse_us;
        }
    }, true);
}

static void vault_draw(Canvas* canvas, void* model) {
    VaultViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_draw_rbox(canvas, 0, 0, 128, 11, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    char hdr[20];
    snprintf(hdr, sizeof(hdr), "VAULT [%u/%u]",
             (unsigned)m->sel + 1, (unsigned)m->count);
    canvas_draw_str(canvas, 2, 8, "SUBGHZ REPLAY");
    canvas_draw_str(canvas, 76, 8, hdr + 6);
    canvas_set_color(canvas, ColorBlack);

    if(m->count == 0) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignTop, "Vault Empty");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignTop, "Capture a signal first");
    } else {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 22, m->label);

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 30, m->freq_label);

        char dur[28];
        snprintf(dur, sizeof(dur), "%u pulses  %lums",
                 (unsigned)m->duration_count, (unsigned long)m->total_ms);
        canvas_draw_str(canvas, 2, 38, dur);

        char pw[28];
        snprintf(pw, sizeof(pw), "pw: %luus-%luus",
                 (unsigned long)m->min_pulse_us, (unsigned long)m->max_pulse_us);
        canvas_draw_str(canvas, 2, 46, pw);

        // TX indicator
        if(m->transmitting && (m->tick & 1)) {
            canvas_draw_box(canvas, 90, 30, 36, 10);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, 92, 38, "TXING");
            canvas_set_color(canvas, ColorBlack);
        } else if(!m->transmitting) {
            canvas_draw_str(canvas, 90, 38, "[OK]TX");
        }
    }

    canvas_draw_line(canvas, 0, 56, 127, 56);
    if(m->count > 0)
        canvas_draw_str(canvas, 0, 63, "[^v]sel [OK]TX [>]wave [<]back");
    else
        canvas_draw_str(canvas, 0, 63, "[<]back");
}

// Async TX: feeds durations from the selected capture to the radio
static const SrCapture* s_tx_cap = NULL;
static uint16_t s_tx_pos = 0;

static bool sr_tx_callback(uint32_t* duration, bool* level, void* ctx) {
    UNUSED(ctx);
    if(!s_tx_cap || s_tx_pos >= s_tx_cap->duration_count) {
        *duration = 0;
        *level    = false;
        return false;  // done
    }
    int32_t d = s_tx_cap->durations[s_tx_pos++];
    *level    = (d > 0);  // positive = carrier on
    *duration = (uint32_t)(d < 0 ? -d : d);
    return true;
}

static void vault_start_tx(SrApp* app) {
    const SrCapture* cap = &app->vault[app->vault_sel];
    furi_hal_subghz_reset();
    furi_hal_subghz_idle();
    furi_hal_subghz_load_preset(FuriHalSubGhzPresetOokAsync);
    furi_hal_subghz_set_frequency_and_path(cap->freq);

    s_tx_cap  = cap;
    s_tx_pos  = 0;
    app->transmitting = true;
    furi_hal_subghz_start_async_tx(sr_tx_callback, app);
}

static bool vault_input(InputEvent* ev, void* ctx) {
    SrApp* app = ctx;
    if(ev->type != InputTypeShort) return false;

    switch(ev->key) {
    case InputKeyBack:
        view_dispatcher_send_custom_event(app->view_dispatcher, SrEventBack);
        return true;
    case InputKeyUp:
        if(app->vault_count > 0 && app->vault_sel > 0) {
            app->vault_sel--;
            vault_sync(app);
        }
        return true;
    case InputKeyDown:
        if(app->vault_count > 0 && app->vault_sel < app->vault_count - 1) {
            app->vault_sel++;
            vault_sync(app);
        }
        return true;
    case InputKeyOk:
        if(app->vault_count > 0 && !app->transmitting) {
            vault_start_tx(app);
            notification_message(app->notifications, &sequence_single_vibro);
            vault_sync(app);
        }
        return true;
    case InputKeyRight:
        if(app->vault_count > 0)
            view_dispatcher_send_custom_event(app->view_dispatcher, SrEventNext);
        return true;
    default:
        return false;
    }
}

// Tick timer to update TX state and blink
static FuriTimer* s_vault_timer = NULL;
static void vault_tick_cb(void* ctx) {
    SrApp* app = ctx;
    if(app->transmitting) {
        // Check if TX complete
        if(!furi_hal_subghz_is_async_tx_complete()) {
            with_view_model(app->vault_view, VaultViewModel* m, {
                m->tick++;
            }, true);
        } else {
            furi_hal_subghz_stop_async_tx();
            furi_hal_subghz_idle();
            furi_hal_subghz_sleep();
            app->transmitting = false;
            s_tx_cap = NULL;
            vault_sync(app);
            notification_message(app->notifications, &sequence_success);
        }
    } else {
        with_view_model(app->vault_view, VaultViewModel* m, {
            m->tick++;
        }, true);
    }
}

void sr_scene_vault_on_enter(void* ctx) {
    SrApp* app = ctx;
    if(app->vault_sel >= app->vault_count && app->vault_count > 0)
        app->vault_sel = app->vault_count - 1;

    vault_sync(app);
    view_set_draw_callback(app->vault_view,  vault_draw);
    view_set_input_callback(app->vault_view, vault_input);
    view_set_context(app->vault_view, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, SrViewVault);

    s_vault_timer = furi_timer_alloc(vault_tick_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(s_vault_timer, 200);
}

bool sr_scene_vault_on_event(void* ctx, SceneManagerEvent ev) {
    SrApp* app = ctx;
    if(ev.type == SceneManagerEventTypeCustom) {
        if(ev.event == SrEventNext) {
            scene_manager_next_scene(app->scene_manager, SrSceneWaveform);
            return true;
        }
        if(ev.event == SrEventBack) {
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
    }
    if(ev.type == SceneManagerEventTypeBack) return false;
    return false;
}

void sr_scene_vault_on_exit(void* ctx) {
    SrApp* app = ctx;
    if(s_vault_timer) {
        furi_timer_stop(s_vault_timer);
        furi_timer_free(s_vault_timer);
        s_vault_timer = NULL;
    }
    if(app->transmitting) {
        furi_hal_subghz_stop_async_tx();
        furi_hal_subghz_idle();
        furi_hal_subghz_sleep();
        app->transmitting = false;
    }
}
