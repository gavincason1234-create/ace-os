// BLE Scanner — passive enumeration of nearby BLE devices
// Company: SignalStack  |  Authorized demonstrations only.
// Passive receive only — no BLE packets are transmitted.
// Demonstrates that BLE advertisements expose device identity, manufacturer,
// and signal strength to any listener within ~100m.
// Fixed (public) MACs are permanently trackable across sessions.

#include "ble_scanner.h"
#include <string.h>

// ─────────────────────────────────────────────────────────────────────────────
// AD (Advertisement Data) parser — extracts name, company ID, flags
// ─────────────────────────────────────────────────────────────────────────────

// AD type codes
#define AD_FLAGS            0x01
#define AD_INCOMPLETE_UUID  0x02
#define AD_COMPLETE_UUID    0x03
#define AD_SHORT_NAME       0x08
#define AD_COMPLETE_NAME    0x09
#define AD_TX_POWER         0x0A
#define AD_MANUFACTURER     0xFF
#define AD_APPEARANCE       0x19

// Appearance values → category
static BsDevCategory bs_appearance_to_category(uint16_t appearance) {
    uint8_t major = (uint8_t)(appearance >> 6);
    switch(major) {
    case 1:  return BsDevPhone;       // 0x40-0x7F: Phone
    case 2:  return BsDevLaptop;      // 0x80-0xBF: Computer
    case 3:  return BsDevSmartwatch;  // 0xC0-0xFF: Watch
    case 4:  return BsDevSmartwatch;  // 0x100+: Clock
    case 5:  return BsDevSensor;      // HID/input
    case 15: return BsDevHeadset;     // Audio/sink
    case 16: return BsDevHeadset;     // Headset
    case 17: return BsDevHeadset;     // Headphones
    case 18: return BsDevHeadset;     // Loudspeaker
    case 19: return BsDevHeadset;     // Headset
    case 24: return BsDevTV;          // Display
    default: return BsDevUnknown;
    }
}

typedef struct {
    char     name[BS_NAME_LEN];
    uint16_t company_id;
    bool     has_company;
    BsDevCategory category;
    bool     connectable;
} AdParsed;

static void bs_parse_ad(const uint8_t* data, uint8_t data_len, AdParsed* out) {
    memset(out, 0, sizeof(AdParsed));
    out->category = BsDevUnknown;

    uint8_t i = 0;
    while(i < data_len) {
        uint8_t len  = data[i];
        if(len == 0 || i + len >= data_len) break;
        uint8_t type = data[i + 1];
        const uint8_t* payload = data + i + 2;
        uint8_t plen = len - 1;

        switch(type) {
        case AD_COMPLETE_NAME:
        case AD_SHORT_NAME:
            if(plen > 0) {
                uint8_t copy = plen < BS_NAME_LEN - 1 ? plen : BS_NAME_LEN - 1;
                memcpy(out->name, payload, copy);
                out->name[copy] = '\0';
                // Sanitize non-printable
                for(uint8_t k = 0; k < copy; k++)
                    if(out->name[k] < 0x20) out->name[k] = '?';
            }
            break;
        case AD_MANUFACTURER:
            if(plen >= 2) {
                out->company_id  = (uint16_t)(payload[0] | (payload[1] << 8));
                out->has_company = true;
                // Apple iBeacon / Nearby / AirTag: company = 0x004C
                if(out->company_id == 0x004C) out->category = BsDevBeacon;
            }
            break;
        case AD_APPEARANCE:
            if(plen >= 2) {
                uint16_t app = (uint16_t)(payload[0] | (payload[1] << 8));
                BsDevCategory c = bs_appearance_to_category(app);
                if(c != BsDevUnknown) out->category = c;
            }
            break;
        case AD_FLAGS:
            if(plen >= 1) {
                // Bit 1 set = general discoverable, bit 2 = limited
                out->connectable = (payload[0] & 0x02) || (payload[0] & 0x01);
            }
            break;
        default:
            break;
        }
        i += len + 1;
    }

    // Category heuristic from company ID if appearance didn't resolve
    if(out->category == BsDevUnknown && out->has_company) {
        switch(out->company_id) {
        case 0x004C: out->category = BsDevBeacon;    break;  // Apple
        case 0x0006: out->category = BsDevLaptop;    break;  // Microsoft
        case 0x00E0: out->category = BsDevPhone;     break;  // Google
        case 0x0112: out->category = BsDevSmartwatch;break;  // Fitbit
        case 0x009E: out->category = BsDevSmartwatch;break;  // Garmin
        case 0x01D9: out->category = BsDevHeadset;   break;  // Bose
        case 0x0089: out->category = BsDevHeadset;   break;  // Jabra
        case 0x0A8C: out->category = BsDevBeacon;    break;  // Tile
        case 0x0171: out->category = BsDevSensor;    break;  // Amazon
        case 0x0499: out->category = BsDevSensor;    break;  // Ruuvi
        default:      break;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Device table management
// ─────────────────────────────────────────────────────────────────────────────

// Find existing entry or claim a new slot
static BsDevice* bs_find_or_add(BsApp* app, const uint8_t addr[6]) {
    // Search existing
    for(uint8_t i = 0; i < app->device_count; i++) {
        if(memcmp(app->devices[i].addr, addr, 6) == 0)
            return &app->devices[i];
    }
    // Add new slot
    if(app->device_count >= BS_DEVICE_MAX) {
        // Evict weakest signal entry
        uint8_t weakest = 0;
        for(uint8_t i = 1; i < BS_DEVICE_MAX; i++) {
            if(app->devices[i].rssi < app->devices[weakest].rssi)
                weakest = i;
        }
        memset(&app->devices[weakest], 0, sizeof(BsDevice));
        memcpy(app->devices[weakest].addr, addr, 6);
        return &app->devices[weakest];
    }
    BsDevice* d = &app->devices[app->device_count++];
    memset(d, 0, sizeof(BsDevice));
    memcpy(d->addr, addr, 6);
    return d;
}

// Sort device list by RSSI descending (closest first)
static void bs_sort_devices(BsApp* app) {
    for(uint8_t i = 0; i < app->device_count - 1; i++) {
        for(uint8_t j = i + 1; j < app->device_count; j++) {
            if(app->devices[j].rssi > app->devices[i].rssi) {
                BsDevice tmp = app->devices[i];
                app->devices[i] = app->devices[j];
                app->devices[j] = tmp;
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// GAP scan callback — fires for every received BLE advertisement
// Called from BT stack thread; must be fast and thread-safe
// ─────────────────────────────────────────────────────────────────────────────

static void bs_gap_callback(GapEvent event, void* ctx) {
    BsApp* app = ctx;
    if(event.type != GapEventTypeAdvData) return;

    const GapExtraBeaconData* adv = &event.data.adv;

    BsDevice* dev = bs_find_or_add(app, adv->addr);
    if(!dev) return;

    // Update RSSI
    dev->rssi = (int8_t)adv->rssi;
    if(dev->rssi > dev->rssi_peak) dev->rssi_peak = dev->rssi;
    dev->last_seen_tick = furi_get_tick();
    dev->seen_count++;
    dev->valid = true;

    // MAC type
    dev->mac_type = (adv->addr_type == 0) ? BsMacPublic : BsMacRandom;

    // Parse advertisement payload
    AdParsed parsed;
    bs_parse_ad(adv->data, adv->data_len, &parsed);

    if(parsed.name[0] && !dev->name[0])
        strncpy(dev->name, parsed.name, BS_NAME_LEN - 1);

    if(parsed.has_company) {
        const char* mfr = bs_company_name(parsed.company_id);
        if(mfr && !dev->mfr[0])
            strncpy(dev->mfr, mfr, BS_MFR_LEN - 1);
    }

    if(parsed.category != BsDevUnknown && dev->category == BsDevUnknown)
        dev->category = parsed.category;

    dev->connectable = parsed.connectable;

    // Update global stats
    app->total_seen++;
    if(dev->mac_type == BsMacPublic) app->public_count++;
    else                              app->random_count++;
    if(dev->category < BsDevCount) app->category_counts[dev->category]++;

    view_dispatcher_send_custom_event(app->view_dispatcher, BsEventDeviceFound);
}

// ─────────────────────────────────────────────────────────────────────────────
// App alloc / free / entry
// ─────────────────────────────────────────────────────────────────────────────

static BsApp* bs_alloc(void) {
    BsApp* app = malloc(sizeof(BsApp));
    memset(app, 0, sizeof(BsApp));

    app->gui           = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->scene_manager = scene_manager_alloc(&bs_scene_handlers, app);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(app->view_dispatcher);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    app->scan_view = view_alloc();
    view_allocate_model(app->scan_view, ViewModelTypeLocking, 512);
    view_dispatcher_add_view(app->view_dispatcher, BsViewScan, app->scan_view);

    app->detail_view = view_alloc();
    view_allocate_model(app->detail_view, ViewModelTypeLocking, 256);
    view_dispatcher_add_view(app->view_dispatcher, BsViewDetail, app->detail_view);

    app->stats_view = view_alloc();
    view_allocate_model(app->stats_view, ViewModelTypeLocking, 256);
    view_dispatcher_add_view(app->view_dispatcher, BsViewStats, app->stats_view);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui,
                                  ViewDispatcherTypeFullscreen);
    return app;
}

static void bs_free(BsApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, BsViewScan);
    view_dispatcher_remove_view(app->view_dispatcher, BsViewDetail);
    view_dispatcher_remove_view(app->view_dispatcher, BsViewStats);

    view_free(app->scan_view);
    view_free(app->detail_view);
    view_free(app->stats_view);
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    free(app);
}

int32_t ble_scanner_app(void* p) {
    UNUSED(p);
    BsApp* app = bs_alloc();
    scene_manager_next_scene(app->scene_manager, BsSceneScan);
    view_dispatcher_run(app->view_dispatcher);
    bs_free(app);
    return 0;
}
