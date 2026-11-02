#pragma once
#include "gas_station_audit.h"
#include "gsa_crypto.h"

// Stealth paths — nothing obviously named
#define GSA_STEALTH_DIR   EXT_PATH(".system/calibration")
#define GSA_STEALTH_FILE  EXT_PATH(".system/calibration/CALIBRATION_DATA.bin")
// Header magic to identify our files if we need to parse them back
#define GSA_FILE_MAGIC    0x5353474100ULL  // "SSGA\0"

// Write encrypted record to stealth file (appends)
// Returns true on success
bool gsa_write_encrypted_record(Storage* storage, const char* plaintext);

// Read and decrypt all records from stealth file into out_buf (null-terminated)
// Returns number of bytes written to out_buf
size_t gsa_read_encrypted_records(Storage* storage, char* out_buf, size_t buf_size);

// Shred and delete the stealth file (overwrite with zeros then delete)
void gsa_wipe_file(Storage* storage);

// Returns true if a stealth file exists with our magic header
bool gsa_file_exists(Storage* storage);
