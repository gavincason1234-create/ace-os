// Settings scene — toggle auto-save, vibrate, beep, save raw .nfc

#include "signalstack_scene.h"

typedef enum {
    SettingsAutoSave = 0,
    SettingsVibrate,
    SettingsBeep,
    SettingsSaveRaw,
    SettingsCount,
} SettingsItem;

static const char* on_off[] = {"Off", "On"};

static void settings_change_callback(VariableItem* item) {
    SignalStackApp* app = variable_item_get_context(item);
    uint8_t idx  = variable_item_list_get_selected_item_index(app->var_list);
    uint8_t val  = variable_item_get_current_value_index(item);

    switch(idx) {
    case SettingsAutoSave:   app->auto_save       = (val == 1); break;
    case SettingsVibrate:    app->vibrate_on_scan = (val == 1); break;
    case SettingsBeep:       app->beep_on_scan    = (val == 1); break;
    case SettingsSaveRaw:    app->save_raw_nfc    = (val == 1); break;
    }
    variable_item_set_current_value_text(item, on_off[val]);
}

void signalstack_scene_settings_on_enter(void* ctx) {
    SignalStackApp* app = ctx;
    variable_item_list_reset(app->var_list);

    VariableItem* item;

    item = variable_item_list_add(app->var_list, "Auto Save", 2,
        settings_change_callback, app);
    variable_item_set_current_value_index(item, app->auto_save ? 1 : 0);
    variable_item_set_current_value_text(item, on_off[app->auto_save ? 1 : 0]);

    item = variable_item_list_add(app->var_list, "Vibrate", 2,
        settings_change_callback, app);
    variable_item_set_current_value_index(item, app->vibrate_on_scan ? 1 : 0);
    variable_item_set_current_value_text(item, on_off[app->vibrate_on_scan ? 1 : 0]);

    item = variable_item_list_add(app->var_list, "Beep", 2,
        settings_change_callback, app);
    variable_item_set_current_value_index(item, app->beep_on_scan ? 1 : 0);
    variable_item_set_current_value_text(item, on_off[app->beep_on_scan ? 1 : 0]);

    item = variable_item_list_add(app->var_list, "Save .nfc File", 2,
        settings_change_callback, app);
    variable_item_set_current_value_index(item, app->save_raw_nfc ? 1 : 0);
    variable_item_set_current_value_text(item, on_off[app->save_raw_nfc ? 1 : 0]);

    view_dispatcher_switch_to_view(app->view_dispatcher, SignalStackViewVarList);
}

bool signalstack_scene_settings_on_event(void* ctx, SceneManagerEvent event) {
    UNUSED(ctx);
    if(event.type == SceneManagerEventTypeBack) return false;
    return false;
}

void signalstack_scene_settings_on_exit(void* ctx) {
    SignalStackApp* app = ctx;
    variable_item_list_reset(app->var_list);
}
