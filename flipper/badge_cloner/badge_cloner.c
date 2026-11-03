// Badge Cloner — 125kHz HID/EM4100 read, store, emulate
// Company: SignalStack  |  Authorized security audits only.
// Demonstrates that 125kHz proximity badges have zero encryption and can be
// cloned by anyone within a few centimeters.  All operations require physical
// proximity to the badge; no remote attack is performed.

#include "badge_cloner.h"
#include <string.h>

// ─────────────────────────────────────────────────────────────────────────────
// Stealth vault: RC4-scramble badge bytes before writing to SD card.
// Key = first 4 bytes of device UID XOR with a compile-time salt.
// This is evidence-grade obfuscation, not real encryption, but it means the
// file on-disk is not a plain-text badge dump.
// ─────────────────────────────────────────────────────────────────────────────

#define VAULT_SALT 0x53534143UL  // "SSAC" — SignalStack Audit Cloner

static void bc_rc4_crypt(uint8_t* buf, uint32_t len, const uint8_t* key, uint8_t klen) {
    uint8_t S[256];
    for(uint16_t i = 0; i < 256; i++) S[i] = (uint8_t)i;
    uint8_t j = 0;
    for(uint16_t i = 0; i < 256; i++) {
        j = (uint8_t)(j + S[i] + key[i % klen]);
        uint8_t t = S[i]; S[i] = S[j]; S[j] = t;
    }
    uint8_t a = 0, b = 0;
    for(uint32_t i = 0; i < len; i++) {
        a++;
        b = (uint8_t)(b + S[a]);
        uint8_t t = S[a]; S[a] = S[b]; S[b] = t;
        buf[i] ^= S[(uint8_t)(S[a] + S[b])];
    }
}

static void bc_get_key(uint8_t key[8]) {
    // Derive key from device UID salt — unique per device
    uint32_t uid = furi_hal_version_uid() ? *(uint32_t*)furi_hal_version_uid() : 0xDEADBEEF;
    uint32_t k0  = uid ^ VAULT_SALT;
    uint32_t k1  = (uid >> 16) ^ (VAULT_SALT >> 8) ^ 0xA5C3E1F7UL;
    memcpy(key,     &k0, 4);
    memcpy(key + 4, &k1, 4);
}

static void bc_vault_save(BcApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    // Ensure stealth directory exists
    storage_common_mkdir(storage, "/ext/.system/calibration");

    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, BC_VAULT_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        // Serialize vault to a scratch buffer then encrypt
        uint32_t sz = sizeof(uint8_t) + BC_SAVED_MAX * sizeof(BcBadge);
        uint8_t* buf = malloc(sz);
        if(buf) {
            buf[0] = app->saved_count;
            memcpy(buf + 1, app->saved, BC_SAVED_MAX * sizeof(BcBadge));
            uint8_t key[8];
            bc_get_key(key);
            bc_rc4_crypt(buf, sz, key, 8);
            storage_file_write(f, buf, sz);
            free(buf);
        }
        storage_file_close(f);
    }
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
}

static void bc_vault_load(BcApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, BC_VAULT_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint32_t sz = sizeof(uint8_t) + BC_SAVED_MAX * sizeof(BcBadge);
        uint8_t* buf = malloc(sz);
        if(buf) {
            uint32_t read = storage_file_read(f, buf, sz);
            if(read == sz) {
                uint8_t key[8];
                bc_get_key(key);
                bc_rc4_crypt(buf, sz, key, 8);
                app->saved_count = buf[0];
                if(app->saved_count > BC_SAVED_MAX) app->saved_count = BC_SAVED_MAX;
                memcpy(app->saved, buf + 1, BC_SAVED_MAX * sizeof(BcBadge));
            }
            free(buf);
        }
        storage_file_close(f);
    }
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
}

// ─────────────────────────────────────────────────────────────────────────────
// App alloc / free / entry
// ─────────────────────────────────────────────────────────────────────────────

static BcApp* bc_alloc(void) {
    BcApp* app = malloc(sizeof(BcApp));
    memset(app, 0, sizeof(BcApp));

    app->gui           = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->scene_manager = scene_manager_alloc(&bc_scene_handlers, app);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(app->view_dispatcher);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher, BcViewSubmenu,
                             submenu_get_view(app->submenu));

    app->read_view = view_alloc();
    view_allocate_model(app->read_view, ViewModelTypeLocking, 256);
    view_dispatcher_add_view(app->view_dispatcher, BcViewRead, app->read_view);

    app->vault_view = view_alloc();
    view_allocate_model(app->vault_view, ViewModelTypeLocking, 256);
    view_dispatcher_add_view(app->view_dispatcher, BcViewVault, app->vault_view);

    app->emulate_view = view_alloc();
    view_allocate_model(app->emulate_view, ViewModelTypeLocking, 256);
    view_dispatcher_add_view(app->view_dispatcher, BcViewEmulate, app->emulate_view);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui,
                                  ViewDispatcherTypeFullscreen);

    app->worker = lfrfid_worker_alloc(NULL);

    bc_vault_load(app);
    return app;
}

static void bc_free(BcApp* app) {
    lfrfid_worker_stop(app->worker);
    lfrfid_worker_free(app->worker);

    view_dispatcher_remove_view(app->view_dispatcher, BcViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, BcViewRead);
    view_dispatcher_remove_view(app->view_dispatcher, BcViewVault);
    view_dispatcher_remove_view(app->view_dispatcher, BcViewEmulate);

    submenu_free(app->submenu);
    view_free(app->read_view);
    view_free(app->vault_view);
    view_free(app->emulate_view);
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    free(app);
}

int32_t badge_cloner_app(void* p) {
    UNUSED(p);
    BcApp* app = bc_alloc();
    scene_manager_next_scene(app->scene_manager, BcSceneMenu);
    view_dispatcher_run(app->view_dispatcher);
    bc_free(app);
    return 0;
}

// Expose vault saver for scene use
void bc_save_current(BcApp* app) {
    if(!app->current.valid) return;
    if(app->saved_count >= BC_SAVED_MAX) {
        // Shift oldest out
        memmove(&app->saved[0], &app->saved[1],
                (BC_SAVED_MAX - 1) * sizeof(BcBadge));
        app->saved_count = BC_SAVED_MAX - 1;
    }
    app->saved[app->saved_count] = app->current;
    // Auto-label: "Badge #N"
    snprintf(app->saved[app->saved_count].label, BC_LABEL_LEN,
             "Badge #%u", (unsigned)(app->saved_count + 1));
    app->saved_count++;
    bc_vault_save(app);
}
