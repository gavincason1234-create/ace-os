// iButton Key Cloner — clone Dallas 1-Wire access keys
// Company: SignalStack  |  Authorized demonstrations only.
// Demonstrates that Dallas iButton keys used in apartments, elevators, gyms,
// and server rooms have ZERO cryptographic protection — the 64-bit serial
// number is broadcast in cleartext on every read and can be cloned instantly.

#include "ibutton_cloner.h"

// ─────────────────────────────────────────────────────────────────────────────
// Vault persistence
// ─────────────────────────────────────────────────────────────────────────────

static void ibc_derive_key(IbcApp* app, uint8_t out[8]) {
    UNUSED(app);
    const uint8_t* uid = furi_hal_version_uid();
    uint32_t u0 = uid
        ? (((uint32_t)uid[0] << 24) | ((uint32_t)uid[1] << 16) |
           ((uint32_t)uid[2] << 8)  | uid[3])
        : 0xDEADBEEFUL;
    uint32_t k0 = u0 ^ IBC_VAULT_SALT;
    uint32_t k1 = ~k0 ^ 0xC3C3C3C3UL;
    memcpy(out,     &k0, 4);
    memcpy(out + 4, &k1, 4);
}

void ibc_vault_save(IbcApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, "/ext/.system");
    storage_common_mkdir(storage, "/ext/.system/calibration");

    uint32_t blob_size = sizeof(uint8_t) + (uint32_t)app->vault_count * sizeof(IbcKey);
    uint8_t* blob = malloc(blob_size);
    if(!blob) { furi_record_close(RECORD_STORAGE); return; }
    blob[0] = app->vault_count;
    for(uint8_t i = 0; i < app->vault_count; i++)
        memcpy(blob + 1 + (uint32_t)i * sizeof(IbcKey), &app->vault[i], sizeof(IbcKey));

    uint8_t key[8];
    ibc_derive_key(app, key);
    ibc_rc4(blob, blob_size, key, 8);

    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, IBC_VAULT_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(f, blob, blob_size);
        storage_file_close(f);
    }
    storage_file_free(f);
    free(blob);
    furi_record_close(RECORD_STORAGE);
}

void ibc_vault_load(IbcApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(storage);
    if(!storage_file_open(f, IBC_VAULT_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(f);
        furi_record_close(RECORD_STORAGE);
        return;
    }
    uint64_t fsize = storage_file_size(f);
    uint32_t max_size = sizeof(uint8_t) + IBC_VAULT_MAX * sizeof(IbcKey);
    if(fsize < 1 || fsize > max_size) {
        storage_file_close(f);
        storage_file_free(f);
        furi_record_close(RECORD_STORAGE);
        return;
    }
    uint8_t* blob = malloc((uint32_t)fsize);
    if(!blob) {
        storage_file_close(f);
        storage_file_free(f);
        furi_record_close(RECORD_STORAGE);
        return;
    }
    storage_file_read(f, blob, (uint32_t)fsize);
    storage_file_close(f);
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);

    uint8_t key[8];
    ibc_derive_key(app, key);
    ibc_rc4(blob, (uint32_t)fsize, key, 8);

    uint8_t count = blob[0];
    if(count > IBC_VAULT_MAX) count = IBC_VAULT_MAX;
    app->vault_count = count;
    for(uint8_t i = 0; i < count; i++)
        memcpy(&app->vault[i], blob + 1 + (uint32_t)i * sizeof(IbcKey), sizeof(IbcKey));
    free(blob);
}

// ─────────────────────────────────────────────────────────────────────────────
// Save current read key to vault
// ─────────────────────────────────────────────────────────────────────────────

void ibc_save_current(IbcApp* app) {
    if(!app->has_current) return;

    // Deduplicate: check if same data already stored
    for(uint8_t i = 0; i < app->vault_count; i++) {
        if(app->vault[i].data_len == app->current.data_len &&
           memcmp(app->vault[i].data, app->current.data, app->current.data_len) == 0)
            return;  // already in vault
    }

    uint8_t slot;
    if(app->vault_count < IBC_VAULT_MAX) {
        slot = app->vault_count++;
    } else {
        // Evict oldest
        for(uint8_t i = 0; i < IBC_VAULT_MAX - 1; i++)
            app->vault[i] = app->vault[i + 1];
        slot = IBC_VAULT_MAX - 1;
    }

    app->vault[slot] = app->current;
    snprintf(app->vault[slot].label, sizeof(app->vault[slot].label),
             "Key #%u", (unsigned)app->vault_count);

    ibc_vault_save(app);
}

// ─────────────────────────────────────────────────────────────────────────────
// Worker read callback — called from iButton worker thread
// ─────────────────────────────────────────────────────────────────────────────

void ibc_read_callback(void* ctx, IButtonWorkerReadResult result, IButtonKey* key) {
    IbcApp* app = ctx;

    if(result != IButtonWorkerReadOK) {
        view_dispatcher_send_custom_event(app->view_dispatcher, IbcEventReadError);
        return;
    }

    // Copy key data
    IbcKey* k = &app->current;
    memset(k, 0, sizeof(IbcKey));

    uint8_t data_size = (uint8_t)ibutton_key_get_data_size(key);
    if(data_size > IBC_KEY_DATA_MAX) data_size = IBC_KEY_DATA_MAX;
    k->data_len = data_size;
    memcpy(k->data, ibutton_key_get_data(key), data_size);

    // Type string
    const char* ts = ibutton_key_get_type_string(key);
    if(ts) strncpy(k->type_str, ts, 15);

    // Use-case from family code lookup
    if(data_size >= 1) {
        const IbcFamilyInfo* fi = ibc_family(k->data[0]);
        if(fi) {
            strncpy(k->use_case, fi->use_case, 23);
            strncpy(k->grade,    fi->grade,    23);
        } else {
            strncpy(k->use_case, "Unknown device",    23);
            strncpy(k->grade,    "GRADE: ? (unknown)", 23);
        }
    }

    strncpy(k->label, "Key #?", 19);
    k->valid = true;
    app->has_current = true;

    view_dispatcher_send_custom_event(app->view_dispatcher, IbcEventKeyRead);
}

// ─────────────────────────────────────────────────────────────────────────────
// Emulate callback — called each time the key is successfully read by a reader
// ─────────────────────────────────────────────────────────────────────────────

void ibc_emulate_callback(void* ctx, bool emulate_status) {
    IbcApp* app = ctx;
    if(emulate_status) {
        app->emulate_count++;
        notification_message(app->notifications, &sequence_single_vibro);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// App lifecycle
// ─────────────────────────────────────────────────────────────────────────────

static IbcApp* ibc_alloc(void) {
    IbcApp* app = malloc(sizeof(IbcApp));
    memset(app, 0, sizeof(IbcApp));

    app->gui           = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->scene_manager = scene_manager_alloc(&ibc_scene_handlers, app);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(app->view_dispatcher);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    app->read_view = view_alloc();
    view_allocate_model(app->read_view, ViewModelTypeLocking, 256);
    view_dispatcher_add_view(app->view_dispatcher, IbcViewRead, app->read_view);

    app->emulate_view = view_alloc();
    view_allocate_model(app->emulate_view, ViewModelTypeLocking, 256);
    view_dispatcher_add_view(app->view_dispatcher, IbcViewEmulate, app->emulate_view);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui,
                                  ViewDispatcherTypeFullscreen);

    // Allocate key buffer for worker (DS1990A = 8 bytes)
    app->key = ibutton_key_alloc(IBC_KEY_DATA_MAX);
    app->worker = ibutton_worker_alloc(app->key);
    ibutton_worker_start_thread(app->worker);

    ibc_vault_load(app);
    return app;
}

static void ibc_free(IbcApp* app) {
    ibutton_worker_stop(app->worker);
    ibutton_worker_stop_thread(app->worker);
    ibutton_worker_free(app->worker);
    ibutton_key_free(app->key);

    view_dispatcher_remove_view(app->view_dispatcher, IbcViewRead);
    view_dispatcher_remove_view(app->view_dispatcher, IbcViewEmulate);
    view_free(app->read_view);
    view_free(app->emulate_view);
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    free(app);
}

int32_t ibutton_cloner_app(void* p) {
    UNUSED(p);
    IbcApp* app = ibc_alloc();
    scene_manager_next_scene(app->scene_manager, IbcSceneRead);
    view_dispatcher_run(app->view_dispatcher);
    ibc_free(app);
    return 0;
}
