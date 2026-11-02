// SignalStack utilities — UID formatting, file I/O, protocol info

#include "signalstack_nfc.h"
#include <storage/storage.h>
#include <toolbox/stream/stream.h>
#include <toolbox/stream/file_stream.h>
#include <datetime/datetime.h>

// ── UID → colon-separated hex string ─────────────────────────────────────────
void signalstack_uid_to_str(const uint8_t* uid, uint8_t len, char* out) {
    int pos = 0;
    for(uint8_t i = 0; i < len && pos < 31; i++) {
        pos += snprintf(out + pos, 32 - pos, "%s%02X", (i > 0) ? ":" : "", uid[i]);
    }
    out[pos] = '\0';
}

// ── Timestamp ─────────────────────────────────────────────────────────────────
void signalstack_get_timestamp(char* out, uint32_t* unix_out) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    snprintf(out, 24, "%04u-%02u-%02u %02u:%02u:%02u",
        (unsigned)(dt.year % 10000),
        (unsigned)(dt.month % 100),
        (unsigned)(dt.day % 100),
        (unsigned)(dt.hour % 100),
        (unsigned)(dt.minute % 100),
        (unsigned)(dt.second % 100));
    *unix_out = (uint32_t)dt.hour * 3600 +
                (uint32_t)dt.minute * 60 +
                (uint32_t)dt.second;
}

// ── Protocol name ─────────────────────────────────────────────────────────────
const char* signalstack_proto_name(TagProtocol p) {
    switch(p) {
    case TagProtoISO14443A:    return "ISO14443-A";
    case TagProtoISO14443B:    return "ISO14443-B";
    case TagProtoISO14443_4A:  return "ISO14443-4A";
    case TagProtoISO15693:     return "ISO15693";
    case TagProtoFeliCa:       return "FeliCa";
    case TagProtoMifareClassic:return "MIFARE Classic";
    case TagProtoMifareUltralight: return "MF Ultralight";
    case TagProtoDESFire:      return "MIFARE DESFire";
    default:                   return "Unknown";
    }
}

const char* signalstack_proto_description(TagProtocol p) {
    switch(p) {
    case TagProtoISO14443A:
        return "HF 13.56 MHz ISO 14443-A.\nAccess badges, bank cards,\ntransit tickets.";
    case TagProtoISO15693:
        return "HF 13.56 MHz Vicinity.\nLonger range ~1m. Used in\nretail inventory, library,\nasset tracking.";
    case TagProtoMifareClassic:
        return "HF 13.56 MHz MIFARE.\nOffice/hotel/transit cards.\n1K=16 sectors, 4K=32 sectors.";
    case TagProtoMifareUltralight:
        return "HF 13.56 MHz MF Ultralight.\nLow cost tickets, loyalty.";
    case TagProtoDESFire:
        return "HF 13.56 MHz DESFire.\nAES/DES high security.\nCorporate access, gov ID.";
    case TagProtoFeliCa:
        return "HF 13.56 MHz NFC-F.\nJapanese transit (Suica),\nOctopus, Sony FeliCa.";
    default:
        return "13.56 MHz HF RFID tag.";
    }
}

// ── Save to CSV log ───────────────────────────────────────────────────────────
void signalstack_save_to_log(SignalStackApp* app, const SignalStackScanResult* r) {
    Stream* stream = file_stream_alloc(app->storage);
    bool opened = file_stream_open(
        stream, SIGNALSTACK_LOG_FILE, FSAM_WRITE, FSOM_OPEN_APPEND);

    if(opened) {
        if(stream_size(stream) == 0) {
            stream_write_cstring(stream,
                "timestamp,protocol,uid,uid_len,extra\r\n");
        }
        FuriString* line = furi_string_alloc();
        furi_string_printf(line, "%s,%s,%s,%u,\"%s\"\r\n",
            r->timestamp, r->proto_name, r->uid_str, r->uid_len, r->extra);
        stream_write_cstring(stream, furi_string_get_cstr(line));
        furi_string_free(line);
    }

    file_stream_close(stream);
    stream_free(stream);
}

// ── Save raw .nfc file ────────────────────────────────────────────────────────
void signalstack_save_nfc_file(SignalStackApp* app, const SignalStackScanResult* r) {
    char path[128];
    snprintf(path, sizeof(path), EXT_PATH("SignalStack/nfc/%s.nfc"), r->uid_str);
    for(char* c = path; *c; c++) {
        if(*c == ':') *c = '-';
    }

    Stream* stream = file_stream_alloc(app->storage);
    bool opened = file_stream_open(stream, path, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(opened) {
        FuriString* s = furi_string_alloc();
        furi_string_printf(s,
            "Filetype: Flipper NFC device\r\n"
            "Version: 4\r\n"
            "# SignalStack NFC Scanner\r\n"
            "# Timestamp: %s\r\n"
            "Device type: %s\r\n"
            "UID: %s\r\n"
            "# Extra: %s\r\n",
            r->timestamp, r->proto_name, r->uid_str, r->extra);
        stream_write_cstring(stream, furi_string_get_cstr(s));
        furi_string_free(s);
    }
    file_stream_close(stream);
    stream_free(stream);
}

// ── Load history from CSV log ─────────────────────────────────────────────────
void signalstack_load_history_from_log(SignalStackApp* app) {
    app->history_count = 0;
    app->total_scans   = 0;

    Stream* stream = file_stream_alloc(app->storage);
    bool opened = file_stream_open(
        stream, SIGNALSTACK_LOG_FILE, FSAM_READ, FSOM_OPEN_EXISTING);

    if(!opened) {
        stream_free(stream);
        return;
    }

    FuriString* line = furi_string_alloc();
    bool first = true;

    while(stream_read_line(stream, line)) {
        if(first) { first = false; continue; }

        const char* s = furi_string_get_cstr(line);
        SignalStackScanResult r = {0};
        char uid_len_str[8] = {0};

        int parsed = sscanf(s, "%19[^,],%31[^,],%31[^,],%7[^,],\"%255[^\"]\"",
            r.timestamp, r.proto_name, r.uid_str, uid_len_str, r.extra);
        if(parsed >= 3) {
            r.uid_len = (uint8_t)atoi(uid_len_str);
            for(TagProtocol p = 0; p < TagProtoCount; p++) {
                if(strcmp(signalstack_proto_name(p), r.proto_name) == 0) {
                    r.protocol = p;
                    break;
                }
            }
            r.saved = true;
            app->total_scans++;
            if(app->history_count < SIGNALSTACK_MAX_HISTORY) {
                app->history[app->history_count++] = r;
            }
        }
    }

    furi_string_free(line);
    file_stream_close(stream);
    stream_free(stream);
}
