// Encrypted stealth storage — no readable footprint on SD card

#include "gsa_storage.h"
#include <storage/storage.h>
#include <toolbox/stream/stream.h>
#include <toolbox/stream/file_stream.h>
#include <string.h>
#include <stdlib.h>

bool gsa_write_encrypted_record(Storage* storage, const char* plaintext) {
    storage_simply_mkdir(storage, GSA_STEALTH_DIR);

    uint8_t key[GSA_KEY_LEN];
    gsa_derive_key(key);

    GsaCipherCtx ctx;
    gsa_cipher_init(&ctx, key, GSA_KEY_LEN);

    size_t plen = strlen(plaintext);
    uint8_t* enc = malloc(plen + 2);
    if(!enc) return false;

    // Prepend a length byte so we can parse records back
    enc[0] = (uint8_t)(plen & 0xFF);
    enc[1] = (uint8_t)((plen >> 8) & 0xFF);
    gsa_cipher_crypt(&ctx, (const uint8_t*)plaintext, enc + 2, plen);

    Stream* stream = file_stream_alloc(storage);
    bool ok = file_stream_open(stream, GSA_STEALTH_FILE, FSAM_WRITE, FSOM_OPEN_APPEND);
    if(ok) {
        // Write magic on first open (check size)
        if(stream_size(stream) == 0) {
            uint64_t magic = GSA_FILE_MAGIC;
            stream_write(stream, (uint8_t*)&magic, 5);
        }
        stream_write(stream, enc, plen + 2);
    }
    file_stream_close(stream);
    stream_free(stream);
    free(enc);
    return ok;
}

size_t gsa_read_encrypted_records(Storage* storage, char* out_buf, size_t buf_size) {
    if(!gsa_file_exists(storage)) return 0;

    uint8_t key[GSA_KEY_LEN];
    gsa_derive_key(key);

    Stream* stream = file_stream_alloc(storage);
    bool ok = file_stream_open(stream, GSA_STEALTH_FILE, FSAM_READ, FSOM_OPEN_EXISTING);
    if(!ok) { stream_free(stream); return 0; }

    // Skip magic header
    stream_seek(stream, 5, StreamOffsetFromStart);

    size_t written = 0;
    while(written < buf_size - 1) {
        uint8_t len_bytes[2];
        if(stream_read(stream, len_bytes, 2) != 2) break;
        uint16_t plen = len_bytes[0] | ((uint16_t)len_bytes[1] << 8);
        if(plen == 0 || plen > 512) break;

        uint8_t* enc = malloc(plen);
        if(!enc) break;
        if(stream_read(stream, enc, plen) != plen) { free(enc); break; }

        // Fresh cipher ctx per record so seek works correctly
        GsaCipherCtx ctx;
        gsa_cipher_init(&ctx, key, GSA_KEY_LEN);
        size_t space = buf_size - written - 1;
        size_t take = plen < space ? plen : space;
        gsa_cipher_crypt(&ctx, enc, (uint8_t*)(out_buf + written), take);
        written += take;
        free(enc);
    }
    out_buf[written] = '\0';

    file_stream_close(stream);
    stream_free(stream);
    return written;
}

void gsa_wipe_file(Storage* storage) {
    if(!gsa_file_exists(storage)) return;

    // Overwrite with zeros before deleting (basic shred)
    Stream* stream = file_stream_alloc(storage);
    if(file_stream_open(stream, GSA_STEALTH_FILE, FSAM_WRITE, FSOM_OPEN_EXISTING)) {
        size_t fsize = stream_size(stream);
        stream_seek(stream, 0, StreamOffsetFromStart);
        uint8_t zero[64] = {0};
        size_t remaining = fsize;
        while(remaining > 0) {
            size_t chunk = remaining < sizeof(zero) ? remaining : sizeof(zero);
            stream_write(stream, zero, chunk);
            remaining -= chunk;
        }
    }
    file_stream_close(stream);
    stream_free(stream);

    // Delete the file
    storage_simply_remove(storage, GSA_STEALTH_FILE);
    // Remove the dir too if empty
    storage_simply_remove(storage, GSA_STEALTH_DIR);
}

bool gsa_file_exists(Storage* storage) {
    if(!storage_file_exists(storage, GSA_STEALTH_FILE)) return false;

    // Verify magic
    Stream* stream = file_stream_alloc(storage);
    bool valid = false;
    if(file_stream_open(stream, GSA_STEALTH_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint8_t magic[5];
        if(stream_read(stream, magic, 5) == 5) {
            uint64_t ref = GSA_FILE_MAGIC;
            valid = (memcmp(magic, &ref, 5) == 0);
        }
    }
    file_stream_close(stream);
    stream_free(stream);
    return valid;
}
