#pragma once
#include <stdint.h>
#include <stddef.h>

// Simple stream cipher (RC4/PRGA variant) — fast, no external deps, good enough
// for forensic resistance on an embedded device. Key is derived from device UID.

#define GSA_KEY_LEN 16

typedef struct {
    uint8_t S[256];
    uint8_t i;
    uint8_t j;
} GsaCipherCtx;

static inline void gsa_cipher_init(GsaCipherCtx* ctx, const uint8_t* key, uint8_t klen) {
    for(int n = 0; n < 256; n++) ctx->S[n] = (uint8_t)n;
    uint8_t j = 0;
    for(int n = 0; n < 256; n++) {
        j = j + ctx->S[n] + key[n % klen];
        uint8_t t = ctx->S[n]; ctx->S[n] = ctx->S[j]; ctx->S[j] = t;
    }
    ctx->i = 0;
    ctx->j = 0;
}

static inline void gsa_cipher_crypt(GsaCipherCtx* ctx, const uint8_t* in, uint8_t* out, size_t len) {
    for(size_t n = 0; n < len; n++) {
        ctx->i++;
        ctx->j += ctx->S[ctx->i];
        uint8_t t = ctx->S[ctx->i]; ctx->S[ctx->i] = ctx->S[ctx->j]; ctx->S[ctx->j] = t;
        uint8_t k = ctx->S[(ctx->S[ctx->i] + ctx->S[ctx->j]) & 0xFF];
        out[n] = in[n] ^ k;
    }
}

// Derive key from Flipper device UID (unique per device, never hardcoded)
static inline void gsa_derive_key(uint8_t* key_out) {
    // Flipper UID lives in OTP flash — 8 bytes at a known address
    // We mix it with a static salt so the key is device-bound
    const uint8_t* uid = furi_hal_version_uid();  // returns ptr to 8-byte device UID
    const uint8_t salt[8] = {0xA3, 0x7F, 0x2C, 0x91, 0xE5, 0x4D, 0xB8, 0x06};
    for(int i = 0; i < 8; i++) {
        key_out[i]     = uid[i] ^ salt[i];
        key_out[i + 8] = uid[i] ^ salt[7 - i] ^ 0xFF;
    }
}
