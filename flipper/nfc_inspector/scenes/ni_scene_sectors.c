// Sector dump scene — scrollable hex view of MIFARE Classic sectors
// Shows key status, block data, access conditions for each sector

#include "../nfc_inspector.h"

typedef struct {
    uint8_t  sector_count;
    uint8_t  sector_sel;
    uint8_t  block_sel;
    // Current sector display
    char     sector_hdr[24];    // "Sector N  [KEY:DEFAULT]"
    char     key_a_str[16];     // "A: FF:FF:FF:FF:FF:FF"
    char     key_b_str[16];
    char     block_hex[NI_BLOCK_MAX][NI_BLOCK_BYTES * 3 + 1];
    bool     block_read[NI_BLOCK_MAX];
    NiKeyStatus key_a_status;
    NiKeyStatus key_b_status;
} NiSectorsViewModel;

static void sectors_sync(NiApp* app) {
    const NiCard* c = &app->card;
    uint8_t s = app->sector_sel;

    with_view_model(app->sectors_view, NiSectorsViewModel* m, {
        m->sector_count = c->sector_count;
        m->sector_sel   = s;
        m->block_sel    = app->block_sel;

        if(s >= c->sector_count) {
            strncpy(m->sector_hdr, "(empty)", 23);
            return;
        }
        const NiSector* sec = &c->sectors[s];
        m->key_a_status = sec->key_a_status;
        m->key_b_status = sec->key_b_status;

        // Header
        const char* ka_tag = (sec->key_a_status == NiKeyDefault) ? "DEFAULT" : "LOCKED";
        snprintf(m->sector_hdr, sizeof(m->sector_hdr), "Sec %02u [%s]", s, ka_tag);

        // Key strings
        char* p = m->key_a_str;
        for(uint8_t i = 0; i < 6; i++) {
            if(i > 0) *p++ = ':';
            p += snprintf(p, 4, "%02X", sec->key_a[i]);
        }
        p = m->key_b_str;
        for(uint8_t i = 0; i < 6; i++) {
            if(i > 0) *p++ = ':';
            p += snprintf(p, 4, "%02X", sec->key_b[i]);
        }

        // Block hex strings
        for(uint8_t b = 0; b < NI_BLOCK_MAX; b++) {
            m->block_read[b] = sec->block_read[b];
            if(!sec->block_read[b]) {
                strncpy(m->block_hex[b], "-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --",
                        NI_BLOCK_BYTES * 3);
                continue;
            }
            char* q = m->block_hex[b];
            for(uint8_t i = 0; i < NI_BLOCK_BYTES; i++) {
                if(i > 0) *q++ = ' ';
                q += snprintf(q, 4, "%02X", sec->block_data[b][i]);
            }
        }
    }, true);
}

static void sectors_draw(Canvas* canvas, void* model) {
    NiSectorsViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Header
    canvas_draw_rbox(canvas, 0, 0, 128, 11, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, m->sector_hdr);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontSecondary);

    // Key A / B status
    const char* ka_label = (m->key_a_status == NiKeyDefault) ? "KA:DEFAULT" : "KA:LOCKED";
    const char* kb_label = (m->key_b_status == NiKeyDefault) ? "KB:DEFAULT" : "KB:LOCKED";
    canvas_draw_str(canvas, 2,  20, ka_label);
    canvas_draw_str(canvas, 68, 20, kb_label);

    // If key A is default, show it
    if(m->key_a_status == NiKeyDefault) {
        canvas_draw_str(canvas, 2, 28, m->key_a_str);
    } else {
        canvas_draw_str(canvas, 2, 28, "** ** ** ** ** **");
    }

    // Block data (blocks 0-2 are data; block 3 is trailer)
    // Show selected block highlighted
    for(uint8_t b = 0; b < NI_BLOCK_MAX; b++) {
        uint8_t y = 38 + b * 7;
        if(y > 55) break;
        // Only show first 8 bytes of each block to fit 128px width
        char short_hex[26] = "";
        if(m->block_read[b]) {
            // Take first 8 bytes (24 chars with spaces)
            strncpy(short_hex, m->block_hex[b], 23);
            short_hex[23] = '\0';
        } else {
            strncpy(short_hex, "?? ?? ?? ?? ?? ?? ?? ??", 25);
        }
        char line[32];
        snprintf(line, sizeof(line), "B%u %s", b, short_hex);
        if(b == m->block_sel) {
            // Invert selected block row
            canvas_draw_box(canvas, 0, y - 6, 128, 7);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, 2, y, line);
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_str(canvas, 2, y, line);
        }
    }

    canvas_draw_line(canvas, 0, 56, 127, 56);
    char hint[32];
    snprintf(hint, sizeof(hint), "[^v]blk [<>]sec %u/%u",
             (unsigned)m->sector_sel + 1, (unsigned)m->sector_count);
    canvas_draw_str(canvas, 0, 63, hint);
}

static bool sectors_input(InputEvent* ev, void* ctx) {
    NiApp* app = ctx;
    if(ev->type != InputTypeShort) return false;

    switch(ev->key) {
    case InputKeyBack:
        view_dispatcher_send_custom_event(app->view_dispatcher, NiEventBack);
        return true;
    case InputKeyUp:
        if(app->block_sel > 0) {
            app->block_sel--;
            sectors_sync(app);
        }
        return true;
    case InputKeyDown:
        if(app->block_sel < NI_BLOCK_MAX - 1) {
            app->block_sel++;
            sectors_sync(app);
        }
        return true;
    case InputKeyLeft:
        if(app->sector_sel > 0) {
            app->sector_sel--;
            app->block_sel = 0;
            sectors_sync(app);
        }
        return true;
    case InputKeyRight:
        if(app->sector_sel < app->card.sector_count - 1) {
            app->sector_sel++;
            app->block_sel = 0;
            sectors_sync(app);
        }
        return true;
    default:
        return false;
    }
}

void ni_scene_sectors_on_enter(void* ctx) {
    NiApp* app = ctx;
    app->sector_sel = 0;
    app->block_sel  = 0;
    sectors_sync(app);

    view_set_draw_callback(app->sectors_view,  sectors_draw);
    view_set_input_callback(app->sectors_view, sectors_input);
    view_set_context(app->sectors_view, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, NiViewSectors);
}

bool ni_scene_sectors_on_event(void* ctx, SceneManagerEvent ev) {
    NiApp* app = ctx;
    if(ev.type == SceneManagerEventTypeCustom) {
        if(ev.event == NiEventBack) {
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
    }
    if(ev.type == SceneManagerEventTypeBack) return false;
    return false;
}

void ni_scene_sectors_on_exit(void* ctx) {
    UNUSED(ctx);
}
