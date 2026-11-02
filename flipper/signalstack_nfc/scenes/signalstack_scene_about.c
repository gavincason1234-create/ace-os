// About scene — company info + stats

#include "signalstack_scene.h"

void signalstack_scene_about_on_enter(void* ctx) {
    SignalStackApp* app = ctx;
    text_box_reset(app->text_box);
    text_box_set_font(app->text_box, TextBoxFontText);

    FuriString* s = furi_string_alloc();
    furi_string_cat_printf(s,
        "SignalStack NFC Scanner\n"
        "Version 1.0.0\n\n"
        "Built for SignalStack\n"
        "13.56 MHz Full Spectrum\n\n"
        "Protocols:\n"
        " ISO 14443-A / B\n"
        " ISO 14443-4A (T=CL)\n"
        " ISO 15693 (vicinity)\n"
        " FeliCa (NFC-F)\n"
        " MIFARE Classic 1K/2K/4K\n"
        " MIFARE Ultralight\n"
        " MIFARE DESFire\n\n"
        "Session stats:\n"
    );
    furi_string_cat_printf(s,
        " Scans this session: %lu\n"
        " Total logged:       %lu\n\n"
        "Logs: SD/SignalStack/\n"
        "  scan_log.csv\n"
        "  nfc/<uid>.nfc\n",
        app->session_scans,
        app->total_scans);

    text_box_set_text(app->text_box, furi_string_get_cstr(s));
    furi_string_free(s);

    view_dispatcher_switch_to_view(app->view_dispatcher, SignalStackViewTextBox);
}

bool signalstack_scene_about_on_event(void* ctx, SceneManagerEvent event) {
    UNUSED(ctx);
    UNUSED(event);
    return false;
}

void signalstack_scene_about_on_exit(void* ctx) {
    SignalStackApp* app = ctx;
    text_box_reset(app->text_box);
}
