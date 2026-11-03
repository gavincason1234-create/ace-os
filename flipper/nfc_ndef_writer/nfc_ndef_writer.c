// NFC NDEF Tag Writer — app entry, NFC poller, page-by-page write
//
// Write flow:
//   1. nfc_poller_alloc(nfc, NfcProtocolMfUltralight)
//   2. nfc_poller_start(poller, nw_poller_callback, app)
//   3. Callback fires MfUltralightPollerEventTypeRequestWriteData →
//      call mf_ultralight_poller_write_page() for each 4-byte page
//   4. On success / error fire custom event → write scene updates state
//
// NDEF layout on NTAG213 (page 4 onward):
//   page 4  = CC (Capability Container, 4 bytes) — only write if needed
//   page 5+ = NDEF TLV message (03 LL ... FE)

#include "nfc_ndef_writer.h"

// ─────────────────────────────────────────────────────────────────────────────
// NFC poller callback
// ─────────────────────────────────────────────────────────────────────────────

static NfcCommand nw_poller_callback(NfcGenericEvent event, void* ctx) {
    NwApp* app = ctx;

    if(event.protocol != NfcProtocolMfUltralight) return NfcCommandContinue;

    MfUltralightPollerEvent* mfev = event.event_data;

    if(mfev->type == MfUltralightPollerEventTypeDetected) {
        // Tag found — notify write scene to update state
        view_dispatcher_send_custom_event(app->view_dispatcher, NwEventTagDetected);
        return NfcCommandContinue;
    }

    if(mfev->type == MfUltralightPollerEventTypeRequestWriteData ||
       mfev->type == MfUltralightPollerEventTypeReady) {
        // Write NDEF pages starting at page 5 (page 4 is CC, left as-is)
        // CC byte[0]=0xE1 (NDEF magic), [1]=0x10 (v1.0), [2]=varies, [3]=0x00
        // Most pre-formatted NTAG213s already have CC written.
        // We write CC here to ensure the tag is formatted.
        MfUltralightPage cc_page = {.data = {0xE1, 0x10, 0x12, 0x00}};
        MfUltralightError err = mf_ultralight_poller_write_page(
            event.instance, NW_FIRST_PAGE, &cc_page);
        if(err != MfUltralightErrorNone) {
            view_dispatcher_send_custom_event(app->view_dispatcher, NwEventWriteError);
            return NfcCommandStop;
        }

        // Write NDEF data pages (page 5+), 4 bytes per page
        uint8_t page_num = NW_FIRST_PAGE + 1;
        uint16_t offset  = 0;
        uint32_t written = 0;

        while(offset < app->ndef_len) {
            MfUltralightPage pg = {.data = {0, 0, 0, 0}};
            uint16_t remaining = app->ndef_len - offset;
            uint16_t chunk     = (remaining > 4) ? 4 : remaining;
            memcpy(pg.data, app->ndef_buf + offset, chunk);

            err = mf_ultralight_poller_write_page(event.instance, page_num, &pg);
            if(err == MfUltralightErrorProtectedPage) {
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, NwEventWriteProtected);
                return NfcCommandStop;
            }
            if(err != MfUltralightErrorNone) {
                view_dispatcher_send_custom_event(app->view_dispatcher, NwEventWriteError);
                return NfcCommandStop;
            }

            offset += chunk;
            page_num++;
            written++;
        }

        app->write_count = written;
        view_dispatcher_send_custom_event(app->view_dispatcher, NwEventWriteDone);
        return NfcCommandStop;
    }

    if(mfev->type == MfUltralightPollerEventTypeAuthRequest) {
        view_dispatcher_send_custom_event(app->view_dispatcher, NwEventWriteProtected);
        return NfcCommandStop;
    }

    if(mfev->type == MfUltralightPollerEventTypeReadFailed) {
        view_dispatcher_send_custom_event(app->view_dispatcher, NwEventWriteError);
        return NfcCommandStop;
    }

    return NfcCommandContinue;
}

// ─────────────────────────────────────────────────────────────────────────────
// Poller lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void nw_poller_start(NwApp* app) {
    if(app->poller) return;
    app->poller = nfc_poller_alloc(app->nfc, NfcProtocolMfUltralight);
    nfc_poller_start(app->poller, nw_poller_callback, app);
}

void nw_poller_stop(NwApp* app) {
    if(!app->poller) return;
    nfc_poller_stop(app->poller);
    nfc_poller_free(app->poller);
    app->poller = NULL;
}

// ─────────────────────────────────────────────────────────────────────────────
// ViewDispatcher navigation callbacks
// ─────────────────────────────────────────────────────────────────────────────

static bool nw_nav_callback(void* ctx) {
    NwApp* app = ctx;
    scene_manager_handle_back_event(app->scene_manager);
    return true;
}

static bool nw_custom_event_callback(void* ctx, uint32_t event) {
    NwApp* app = ctx;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

// ─────────────────────────────────────────────────────────────────────────────
// App alloc / free
// ─────────────────────────────────────────────────────────────────────────────

static NwApp* nw_app_alloc(void) {
    NwApp* app = malloc(sizeof(NwApp));
    memset(app, 0, sizeof(NwApp));

    app->gui           = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->nfc           = nfc_alloc();

    app->scene_manager = scene_manager_alloc(&nw_scene_handlers, app);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, nw_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, nw_nav_callback);
    view_dispatcher_attach_to_gui(
        app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Allocate views with fixed 256-byte model
    app->menu_view  = view_alloc();
    app->write_view = view_alloc();

    view_allocate_model(app->menu_view,  ViewModelTypeLocking, 256);
    view_allocate_model(app->write_view, ViewModelTypeLocking, 256);

    view_dispatcher_add_view(app->view_dispatcher, NwViewMenu,  app->menu_view);
    view_dispatcher_add_view(app->view_dispatcher, NwViewWrite, app->write_view);

    app->payload_type = NwPayloadUrl;
    app->write_state  = NwWriteStateWaiting;

    return app;
}

static void nw_app_free(NwApp* app) {
    nw_poller_stop(app);
    nfc_free(app->nfc);

    view_dispatcher_remove_view(app->view_dispatcher, NwViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher, NwViewWrite);
    view_free(app->menu_view);
    view_free(app->write_view);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    free(app);
}

// ─────────────────────────────────────────────────────────────────────────────
// Entry point
// ─────────────────────────────────────────────────────────────────────────────

int32_t nfc_ndef_writer_app(void* p) {
    UNUSED(p);
    NwApp* app = nw_app_alloc();

    scene_manager_next_scene(app->scene_manager, NwSceneMenu);
    view_dispatcher_run(app->view_dispatcher);

    nw_app_free(app);
    return 0;
}
