// Vault scene — browse saved badges, select one for emulation

#include "../badge_cloner.h"

// ─────────────────────────────────────────────────────────────────────────────
// View model
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    uint8_t    count;
    uint8_t    sel;
    // Copy of current selection for drawing
    BcCardType type;
    char       label[BC_LABEL_LEN];
    char       hex[BC_DATA_MAX * 3 + 1];
    uint32_t   facility;
    uint32_t   card_id;
    bool       has_decoded;
} VaultViewModel;

static void vault_sync_model(BcApp* app) {
    with_view_model(app->vault_view, VaultViewModel* m, {
        m->count = app->saved_count;
        m->sel   = app->vault_sel;
        if(app->saved_count == 0) {
            m->type = BcCardUnknown;
            strncpy(m->label, "(empty)", BC_LABEL_LEN - 1);
            m->hex[0] = '\0';
            m->has_decoded = false;
        } else {
            const BcBadge* b = &app->saved[app->vault_sel];
            m->type = b->type;
            strncpy(m->label, b->label, BC_LABEL_LEN - 1);

            char* p = m->hex;
            for(uint8_t i = 0; i < b->data_len; i++) {
                if(i > 0) *p++ = ' ';
                p += snprintf(p, 4, "%02X", b->data[i]);
            }
            *p = '\0';

            m->has_decoded = false;
            if(b->type == BcCardHIDProx && b->data_len >= 4) {
                uint32_t bits = ((uint32_t)b->data[0] << 24) | ((uint32_t)b->data[1] << 16)
                              | ((uint32_t)b->data[2] <<  8) |  (uint32_t)b->data[3];
                m->facility  = (bits >> 17) & 0xFF;
                m->card_id   = (bits >>  1) & 0xFFFF;
                m->has_decoded = true;
            } else if(b->type == BcCardEM4100 && b->data_len >= 5) {
                m->facility = b->data[0];
                m->card_id  = ((uint32_t)b->data[1] << 24) | ((uint32_t)b->data[2] << 16)
                            | ((uint32_t)b->data[3] <<  8) |  (uint32_t)b->data[4];
                m->has_decoded = true;
            }
        }
    }, true);
}

// ─────────────────────────────────────────────────────────────────────────────
// Draw
// ─────────────────────────────────────────────────────────────────────────────

static void vault_draw(Canvas* canvas, void* model) {
    VaultViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_draw_rbox(canvas, 0, 0, 128, 11, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    char hdr[28];
    snprintf(hdr, sizeof(hdr), "VAULT  [%u/%u]", (unsigned)m->sel + 1, (unsigned)m->count);
    canvas_draw_str(canvas, 2, 8, "BADGE CLONER");
    canvas_draw_str(canvas, 72, 8, hdr + 6);  // show just the count part
    canvas_set_color(canvas, ColorBlack);

    if(m->count == 0) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignTop, "Vault Empty");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignTop, "Read a badge first");
    } else {
        // Label + type
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 22, m->label);

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 31, bc_card_name(m->type));
        canvas_draw_str(canvas, 2, 39, m->hex);

        if(m->has_decoded) {
            char fac[20], cid[24];
            snprintf(fac, sizeof(fac), "FAC:%lu", (unsigned long)m->facility);
            snprintf(cid, sizeof(cid), "ID:%lu",  (unsigned long)m->card_id);
            canvas_draw_str(canvas, 2, 47, fac);
            canvas_draw_str(canvas, 50, 47, cid);
        }
    }

    canvas_draw_line(canvas, 0, 56, 127, 56);
    if(m->count > 0)
        canvas_draw_str(canvas, 0, 63, "[^v]sel [OK]emulate [<]back");
    else
        canvas_draw_str(canvas, 0, 63, "[<]back");
}

// ─────────────────────────────────────────────────────────────────────────────
// Input
// ─────────────────────────────────────────────────────────────────────────────

static bool vault_input(InputEvent* ev, void* ctx) {
    BcApp* app = ctx;
    if(ev->type != InputTypeShort) return false;

    if(ev->key == InputKeyBack) {
        view_dispatcher_send_custom_event(app->view_dispatcher, BcEventBack);
        return true;
    }
    if(ev->key == InputKeyOk && app->saved_count > 0) {
        view_dispatcher_send_custom_event(app->view_dispatcher, BcEventEmulStart);
        return true;
    }
    if(ev->key == InputKeyUp && app->saved_count > 0) {
        if(app->vault_sel > 0) app->vault_sel--;
        vault_sync_model(app);
        return true;
    }
    if(ev->key == InputKeyDown && app->saved_count > 0) {
        if(app->vault_sel < app->saved_count - 1) app->vault_sel++;
        vault_sync_model(app);
        return true;
    }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Scene lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void bc_scene_vault_on_enter(void* ctx) {
    BcApp* app = ctx;
    if(app->vault_sel >= app->saved_count && app->saved_count > 0)
        app->vault_sel = app->saved_count - 1;

    view_set_draw_callback(app->vault_view,  vault_draw);
    view_set_input_callback(app->vault_view, vault_input);
    view_set_context(app->vault_view, app);

    vault_sync_model(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, BcViewVault);
}

bool bc_scene_vault_on_event(void* ctx, SceneManagerEvent ev) {
    BcApp* app = ctx;
    if(ev.type == SceneManagerEventTypeCustom) {
        if(ev.event == BcEventEmulStart) {
            // Copy selected badge to current for emulation
            app->current = app->saved[app->vault_sel];
            scene_manager_next_scene(app->scene_manager, BcSceneEmulate);
            return true;
        }
        if(ev.event == BcEventBack) {
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
    }
    if(ev.type == SceneManagerEventTypeBack) return false;
    return false;
}

void bc_scene_vault_on_exit(void* ctx) {
    UNUSED(ctx);
}
