// iButton Key Cloner — read scene
// Hold a Dallas iButton key to the Flipper's bottom iButton port.
// Shows animated contact rings, then displays the 8-byte ROM ID
// (family code + 48-bit serial + CRC), use-case, and security grade.
// OK = save to vault + go to emulate, Back = exit.

#include "../ibutton_cloner.h"

typedef enum {
    ReadStateWaiting = 0,
    ReadStateScanning,
    ReadStateDone,
    ReadStateError,
} ReadState;

typedef struct {
    ReadState state;
    // Key data display
    char     hex_line1[20];  // "FC:01  XX:XX:XX:XX"
    char     hex_line2[20];  // "XX:XX  CRC:XX"
    char     type_str[16];
    char     use_case[24];
    char     grade[24];
    uint8_t  tick;
    uint8_t  ring_phase;    // contact-ring animation counter
} ReadViewModel;

// Draw animated concentric rings at the bottom of the screen
// to indicate "touch key here" — iButton port is at the bottom of Flipper
static void draw_contact_rings(Canvas* canvas, uint8_t phase) {
    uint8_t cx = 64;
    uint8_t cy = 58;  // near bottom

    // Inner solid dot
    canvas_draw_box(canvas, cx - 3, cy - 3, 7, 7);

    // Expanding rings, cycling with phase
    for(uint8_t r = 0; r < 3; r++) {
        uint8_t rad = (uint8_t)(8 + r * 7);
        uint8_t age = (uint8_t)((phase + r * 3) % 9);
        if(age < 6) {
            canvas_draw_circle(canvas, cx, cy, rad);
        }
    }
}

static void read_sync(IbcApp* app, ReadState state) {
    with_view_model(app->read_view, ReadViewModel* m, {
        m->state = state;
        if(state == ReadStateDone && app->has_current) {
            const IbcKey* k = &app->current;
            // Line 1: family code + first 3 serial bytes
            if(k->data_len >= 4)
                snprintf(m->hex_line1, sizeof(m->hex_line1),
                         "FC:%02X  %02X:%02X:%02X",
                         k->data[0], k->data[1], k->data[2], k->data[3]);
            // Line 2: remaining serial + CRC
            if(k->data_len >= 8)
                snprintf(m->hex_line2, sizeof(m->hex_line2),
                         "%02X:%02X:%02X  CRC:%02X",
                         k->data[4], k->data[5], k->data[6], k->data[7]);
            strncpy(m->type_str, k->type_str,  15);
            strncpy(m->use_case, k->use_case, 23);
            strncpy(m->grade,    k->grade,    23);
        }
    }, true);
}

static void read_draw(Canvas* canvas, void* model) {
    ReadViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Header
    canvas_draw_rbox(canvas, 0, 0, 128, 11, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, "iBUTTON KEY CLONER");
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontSecondary);

    if(m->state == ReadStateWaiting || m->state == ReadStateScanning) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignTop,
                                "Touch key to port");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignTop,
                                "(bottom of Flipper)");

        draw_contact_rings(canvas, m->ring_phase);

        if(m->state == ReadStateScanning) {
            const char* anim[] = {"READING.", "READING..", "READING..."};
            canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignTop,
                                    anim[m->tick % 3]);
        }

    } else if(m->state == ReadStateError) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignTop, "Read Error");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignTop,
                                "Hold key steady and retry");

    } else {  // ReadStateDone
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 22, "KEY CLONED!");
        canvas_set_font(canvas, FontSecondary);

        canvas_draw_str(canvas, 2, 31, m->hex_line1);
        canvas_draw_str(canvas, 2, 39, m->hex_line2);
        canvas_draw_str(canvas, 2, 47, m->use_case);

        // Grade — inverted box for visual impact
        uint8_t gw = (uint8_t)(strlen(m->grade) * 5 + 4);
        canvas_draw_box(canvas, 0, 52, gw, 9);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, 2, 59, m->grade);
        canvas_set_color(canvas, ColorBlack);

        canvas_draw_line(canvas, 0, 56, 127, 56);
        canvas_draw_str(canvas, gw + 2, 63, "[OK]clone+emulate");
    }
}

static bool read_input(InputEvent* ev, void* ctx) {
    IbcApp* app = ctx;
    if(ev->type != InputTypeShort) return false;
    switch(ev->key) {
    case InputKeyBack:
        view_dispatcher_send_custom_event(app->view_dispatcher, IbcEventBack);
        return true;
    case InputKeyOk:
        if(app->has_current) {
            ibc_save_current(app);
            view_dispatcher_send_custom_event(app->view_dispatcher, IbcEventSelect);
        }
        return true;
    default:
        return false;
    }
}

static FuriTimer* s_read_timer = NULL;

static void read_tick_cb(void* ctx) {
    IbcApp* app = ctx;
    with_view_model(app->read_view, ReadViewModel* m, {
        m->tick++;
        m->ring_phase = (uint8_t)((m->ring_phase + 1) % 9);
        // Transition waiting → scanning after first tick
        if(m->state == ReadStateWaiting) m->state = ReadStateScanning;
    }, true);
}

void ibc_scene_read_on_enter(void* ctx) {
    IbcApp* app = ctx;

    view_set_draw_callback(app->read_view,  read_draw);
    view_set_input_callback(app->read_view, read_input);
    view_set_context(app->read_view, app);

    app->has_current = false;
    read_sync(app, ReadStateWaiting);
    view_dispatcher_switch_to_view(app->view_dispatcher, IbcViewRead);

    // Start worker read
    ibutton_worker_read_start(app->worker, ibc_read_callback, app);

    s_read_timer = furi_timer_alloc(read_tick_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(s_read_timer, 250);
}

bool ibc_scene_read_on_event(void* ctx, SceneManagerEvent ev) {
    IbcApp* app = ctx;
    if(ev.type == SceneManagerEventTypeCustom) {
        if(ev.event == IbcEventKeyRead) {
            notification_message(app->notifications, &sequence_success);
            read_sync(app, ReadStateDone);
            return true;
        }
        if(ev.event == IbcEventReadError) {
            read_sync(app, ReadStateError);
            return true;
        }
        if(ev.event == IbcEventSelect) {
            scene_manager_next_scene(app->scene_manager, IbcSceneEmulate);
            return true;
        }
        if(ev.event == IbcEventBack) {
            view_dispatcher_stop(app->view_dispatcher);
            return true;
        }
    }
    if(ev.type == SceneManagerEventTypeBack) return false;
    return false;
}

void ibc_scene_read_on_exit(void* ctx) {
    IbcApp* app = ctx;
    if(s_read_timer) {
        furi_timer_stop(s_read_timer);
        furi_timer_free(s_read_timer);
        s_read_timer = NULL;
    }
    ibutton_worker_stop(app->worker);
}
