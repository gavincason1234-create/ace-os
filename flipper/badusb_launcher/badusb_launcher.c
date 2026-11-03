// BadUSB Payload Launcher — app entry, HID injection engine
//
// Injection flow:
//   1. Switch USB to HID mode: furi_hal_usb_set_config(&usb_hid, NULL)
//   2. Wait 500ms for host enumeration
//   3. Execute payload keystrokes in FuriThread (not main thread)
//   4. Fire BuEventInjectDone / BuEventInjectError via view_dispatcher
//   5. On scene exit: restore USB config + free thread

#include "badusb_launcher.h"

// ─────────────────────────────────────────────────────────────────────────────
// ASCII → HID keycode table (printable 0x20-0x7E)
// ─────────────────────────────────────────────────────────────────────────────

// Returns (modifier<<8 | keycode) for a given ASCII char.
// Returns 0 for unsupported chars.
static uint16_t ascii_to_hid(char c) {
    // Lowercase a-z
    if(c >= 'a' && c <= 'z')
        return HID_KEY(0, (uint8_t)(HID_KEYBOARD_A + (c - 'a')));
    // Uppercase A-Z → shift + lowercase key
    if(c >= 'A' && c <= 'Z')
        return HID_KEY(MOD_LSHIFT, (uint8_t)(HID_KEYBOARD_A + (c - 'A')));
    // Digits 0-9
    if(c >= '1' && c <= '9')
        return HID_KEY(0, (uint8_t)(HID_KEYBOARD_1 + (c - '1')));
    if(c == '0')
        return HID_KEY(0, HID_KEYBOARD_0);

    // Symbols
    switch(c) {
    case ' ':  return HID_KEY(0, HID_KEYBOARD_SPACEBAR);
    case '\n': return KEY_ENTER;
    case '\t': return KEY_TAB;
    case '-':  return HID_KEY(0, HID_KEYBOARD_MINUS);
    case '_':  return HID_KEY(MOD_LSHIFT, HID_KEYBOARD_MINUS);
    case '=':  return HID_KEY(0, HID_KEYBOARD_EQUAL_SIGN);
    case '+':  return HID_KEY(MOD_LSHIFT, HID_KEYBOARD_EQUAL_SIGN);
    case '[':  return HID_KEY(0, HID_KEYBOARD_OPEN_BRACKET);
    case ']':  return HID_KEY(0, HID_KEYBOARD_CLOSE_BRACKET);
    case '\\': return HID_KEY(0, HID_KEYBOARD_BACKSLASH);
    case ';':  return HID_KEY(0, HID_KEYBOARD_SEMICOLON);
    case '\'': return HID_KEY(0, HID_KEYBOARD_APOSTROPHE);
    case '`':  return HID_KEY(0, HID_KEYBOARD_GRAVE_ACCENT);
    case ',':  return HID_KEY(0, HID_KEYBOARD_COMMA);
    case '.':  return HID_KEY(0, HID_KEYBOARD_DOT);
    case '/':  return HID_KEY(0, HID_KEYBOARD_SLASH);
    case '!':  return HID_KEY(MOD_LSHIFT, HID_KEYBOARD_1);
    case '@':  return HID_KEY(MOD_LSHIFT, HID_KEYBOARD_2);
    case '#':  return HID_KEY(MOD_LSHIFT, HID_KEYBOARD_3);
    case '$':  return HID_KEY(MOD_LSHIFT, HID_KEYBOARD_4);
    case '%':  return HID_KEY(MOD_LSHIFT, HID_KEYBOARD_5);
    case '^':  return HID_KEY(MOD_LSHIFT, HID_KEYBOARD_6);
    case '&':  return HID_KEY(MOD_LSHIFT, HID_KEYBOARD_7);
    case '*':  return HID_KEY(MOD_LSHIFT, HID_KEYBOARD_8);
    case '(':  return HID_KEY(MOD_LSHIFT, HID_KEYBOARD_9);
    case ')':  return HID_KEY(MOD_LSHIFT, HID_KEYBOARD_0);
    case ':':  return HID_KEY(MOD_LSHIFT, HID_KEYBOARD_SEMICOLON);
    case '"':  return HID_KEY(MOD_LSHIFT, HID_KEYBOARD_APOSTROPHE);
    case '<':  return HID_KEY(MOD_LSHIFT, HID_KEYBOARD_COMMA);
    case '>':  return HID_KEY(MOD_LSHIFT, HID_KEYBOARD_DOT);
    case '?':  return HID_KEY(MOD_LSHIFT, HID_KEYBOARD_SLASH);
    case '{':  return HID_KEY(MOD_LSHIFT, HID_KEYBOARD_OPEN_BRACKET);
    case '}':  return HID_KEY(MOD_LSHIFT, HID_KEYBOARD_CLOSE_BRACKET);
    case '|':  return HID_KEY(MOD_LSHIFT, HID_KEYBOARD_BACKSLASH);
    case '~':  return HID_KEY(MOD_LSHIFT, HID_KEYBOARD_GRAVE_ACCENT);
    default:   return 0;
    }
}

bool bu_type_string(const char* str) {
    while(*str) {
        uint16_t k = ascii_to_hid(*str);
        if(k != 0) {
            furi_hal_hid_kb_press(k);
            furi_delay_ms(20);
            furi_hal_hid_kb_release(k);
            furi_delay_ms(20);
        }
        str++;
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Payload executors (run inside FuriThread)
// ─────────────────────────────────────────────────────────────────────────────

static void payload_lock(BuApp* app) {
    // Win + L → immediate screen lock
    bu_key(KEY_WIN_L);
    furi_delay_ms(200);
    app->bytes_typed = 1;
}

static void payload_notepad(BuApp* app) {
    // Win + R
    bu_key(KEY_WIN_R);
    furi_delay_ms(500);
    // Type "notepad"
    bu_type_string("notepad");
    furi_delay_ms(100);
    bu_key(KEY_ENTER);
    furi_delay_ms(800);
    // Type warning message
    static const char msg[] =
        "AUTHORIZED USB SECURITY TEST\r\n"
        "This machine accepted a USB HID injection.\r\n"
        "Contact your security team: signalstack.io\r\n";
    bu_type_string(msg);
    app->bytes_typed = (uint32_t)strlen(msg);
}

static void payload_cmd_recon(BuApp* app) {
    // Win + R
    bu_key(KEY_WIN_R);
    furi_delay_ms(500);
    // Open cmd hidden via cmd /K
    bu_type_string("cmd");
    furi_delay_ms(100);
    bu_key(KEY_ENTER);
    furi_delay_ms(800);
    // Run recon commands
    static const char cmds[] = "whoami && ipconfig\r\n";
    bu_type_string(cmds);
    furi_delay_ms(1500);
    // Second command: show open connections
    bu_type_string("netstat -an | findstr ESTABLISHED\r\n");
    furi_delay_ms(500);
    app->bytes_typed = (uint32_t)(strlen(cmds) + 38);
}

static void payload_browser(BuApp* app) {
    // Win + R → start <url>
    bu_key(KEY_WIN_R);
    furi_delay_ms(500);
    bu_type_string("start https://signalstack.io/demo");
    furi_delay_ms(100);
    bu_key(KEY_ENTER);
    furi_delay_ms(300);
    app->bytes_typed = 33;
}

// ─────────────────────────────────────────────────────────────────────────────
// Injection thread
// ─────────────────────────────────────────────────────────────────────────────

static int32_t inject_thread_fn(void* ctx) {
    BuApp* app = ctx;

    // Switch to HID mode
    app->saved_usb = furi_hal_usb_get_config();
    if(furi_hal_usb_is_locked()) {
        view_dispatcher_send_custom_event(app->view_dispatcher, BuEventNoUsb);
        return 0;
    }

    furi_hal_usb_set_config(&usb_hid, NULL);
    furi_delay_ms(500);   // Wait for host enumeration

    switch(app->payload_type) {
    case BuPayloadLock:     payload_lock(app);      break;
    case BuPayloadNotepad:  payload_notepad(app);   break;
    case BuPayloadCmdRecon: payload_cmd_recon(app); break;
    case BuPayloadBrowser:  payload_browser(app);   break;
    default: break;
    }

    furi_hal_hid_kb_release_all();
    // Restore USB
    furi_hal_usb_set_config(app->saved_usb, NULL);
    app->saved_usb = NULL;

    view_dispatcher_send_custom_event(app->view_dispatcher, BuEventInjectDone);
    return 0;
}

void bu_inject_start(BuApp* app) {
    if(app->inject_thread) return;
    app->run_state  = BuRunStateInjecting;
    app->bytes_typed = 0;
    app->inject_thread = furi_thread_alloc_ex(
        "bu_inject", 2048, inject_thread_fn, app);
    furi_thread_start(app->inject_thread);
}

void bu_inject_stop(BuApp* app) {
    if(!app->inject_thread) return;
    furi_thread_join(app->inject_thread);
    furi_thread_free(app->inject_thread);
    app->inject_thread = NULL;
    // Ensure USB restored if thread exited early
    if(app->saved_usb) {
        furi_hal_usb_set_config(app->saved_usb, NULL);
        app->saved_usb = NULL;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// ViewDispatcher callbacks
// ─────────────────────────────────────────────────────────────────────────────

static bool bu_nav_callback(void* ctx) {
    BuApp* app = ctx;
    scene_manager_handle_back_event(app->scene_manager);
    return true;
}

static bool bu_custom_event_callback(void* ctx, uint32_t event) {
    BuApp* app = ctx;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

// ─────────────────────────────────────────────────────────────────────────────
// App alloc / free
// ─────────────────────────────────────────────────────────────────────────────

static BuApp* bu_app_alloc(void) {
    BuApp* app = malloc(sizeof(BuApp));
    memset(app, 0, sizeof(BuApp));

    app->gui           = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->scene_manager = scene_manager_alloc(&bu_scene_handlers, app);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, bu_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, bu_nav_callback);
    view_dispatcher_attach_to_gui(
        app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->menu_view = view_alloc();
    app->run_view  = view_alloc();
    view_allocate_model(app->menu_view, ViewModelTypeLocking, 256);
    view_allocate_model(app->run_view,  ViewModelTypeLocking, 256);

    view_dispatcher_add_view(app->view_dispatcher, BuViewMenu, app->menu_view);
    view_dispatcher_add_view(app->view_dispatcher, BuViewRun,  app->run_view);

    app->payload_type = BuPayloadLock;
    app->run_state    = BuRunStateIdle;
    app->countdown    = 3;

    return app;
}

static void bu_app_free(BuApp* app) {
    bu_inject_stop(app);

    view_dispatcher_remove_view(app->view_dispatcher, BuViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher, BuViewRun);
    view_free(app->menu_view);
    view_free(app->run_view);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    free(app);
}

// ─────────────────────────────────────────────────────────────────────────────
// Entry point
// ─────────────────────────────────────────────────────────────────────────────

int32_t badusb_launcher_app(void* p) {
    UNUSED(p);
    BuApp* app = bu_app_alloc();

    scene_manager_next_scene(app->scene_manager, BuSceneMenu);
    view_dispatcher_run(app->view_dispatcher);

    bu_app_free(app);
    return 0;
}
