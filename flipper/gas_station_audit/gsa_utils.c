// Gas Station Audit — utilities

#include "gas_station_audit.h"
#include <datetime/datetime.h>

void gsa_uid_to_str(const uint8_t* uid, uint8_t len, char* out) {
    int pos = 0;
    for(uint8_t i = 0; i < len && pos < 31; i++)
        pos += snprintf(out + pos, 32 - pos, "%s%02X", i ? ":" : "", uid[i]);
    out[pos] = '\0';
}

void gsa_get_timestamp(char* out) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    snprintf(out, 24, "%04u-%02u-%02u %02u:%02u:%02u",
        (unsigned)(dt.year % 10000), (unsigned)(dt.month % 100),
        (unsigned)(dt.day % 100), (unsigned)(dt.hour % 100),
        (unsigned)(dt.minute % 100), (unsigned)(dt.second % 100));
}

bool gsa_bt_name_is_suspicious(const char* name) {
    if(!name || !name[0]) return false;
    for(int i = 0; i < GSA_SKIMMER_NAME_COUNT; i++) {
        // Prefix match — "HC-05" matches "HC-05:ABCD" too
        size_t klen = strlen(GSA_SKIMMER_NAMES[i]);
        if(strncasecmp(name, GSA_SKIMMER_NAMES[i], klen) == 0) return true;
    }
    return false;
}

bool gsa_bt_oui_is_suspicious(const uint8_t* mac) {
    for(int i = 0; i < GSA_SKIMMER_OUI_COUNT; i++) {
        if(memcmp(mac, GSA_SKIMMER_OUIS[i], 3) == 0) return true;
    }
    return false;
}

char gsa_nfc_security_grade(GsaNfcProto proto, bool has_crypto) {
    // ISO 15693 (HF vicinity) used by many fleet/loyalty cards — fixed UID, no auth = F
    if(proto == GsaNfcProtoISO15693) return has_crypto ? 'C' : 'F';
    // ISO 14443-A without crypto (EM/MIFARE UID only) = D
    if(proto == GsaNfcProtoISO14443A) return has_crypto ? 'B' : 'D';
    return 'F';
}

void gsa_build_report(GasStationAuditApp* app) {
    GsaReport* r = &app->report;

    r->bt_total_count    = app->bt_count;
    r->bt_flagged_count  = app->bt_flagged;
    r->nfc_total_count   = app->nfc_count;
    r->rf_signals_count  = app->rf_event_count;

    r->nfc_cloneable_count = 0;
    for(uint8_t i = 0; i < app->nfc_count; i++)
        if(app->nfc_results[i].cloneable) r->nfc_cloneable_count++;

    // Score: start at 100, deduct per finding
    int score = 100;
    score -= r->bt_flagged_count  * 30;  // each suspicious BT device = -30
    score -= r->nfc_cloneable_count * 20; // each cloneable card = -20
    if(r->rf_signals_count > 0) score -= 10; // any unencrypted RF = -10
    if(score < 0) score = 0;

    r->pump_score = (uint8_t)score;
    r->pump_grade = score >= 80 ? 'A' :
                    score >= 60 ? 'B' :
                    score >= 40 ? 'C' :
                    score >= 20 ? 'D' : 'F';

    gsa_get_timestamp(r->timestamp);

    // Build summary string
    int spos = 0;
    spos += snprintf(r->summary + spos, sizeof(r->summary) - spos,
        "GRADE: %c  SCORE: %u/100\n"
        "BT DEVICES: %u total, %u SUSPICIOUS\n"
        "NFC/RFID: %u scanned, %u CLONEABLE\n"
        "RF SIGNALS: %u detected\n",
        r->pump_grade, r->pump_score,
        r->bt_total_count, r->bt_flagged_count,
        r->nfc_total_count, r->nfc_cloneable_count,
        r->rf_signals_count);

    if(r->bt_flagged_count > 0)
        snprintf(r->summary + spos, sizeof(r->summary) - spos,
            "! SKIMMER RISK DETECTED !");

    app->report_ready = true;
}
