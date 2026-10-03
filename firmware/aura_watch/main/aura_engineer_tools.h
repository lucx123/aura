#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AURA_ENGINEER_INPUT_MAX 80
#define AURA_VLSM_MAX_REQUESTS 4

typedef struct {
    double frequency_mhz;
    double distance_m;
    double power_dbm;
    double wavelength_m;
    double fspl_db;
    double power_mw;
    bool short_distance;
} aura_rf_result_t;

typedef enum {
    AURA_VLSM_OK = 0,
    AURA_VLSM_INVALID_INPUT,
    AURA_VLSM_INVALID_BASE,
    AURA_VLSM_NO_SPACE,
} aura_vlsm_error_t;

typedef struct {
    uint32_t requested_hosts;
    uint32_t network;
    uint32_t broadcast;
    uint32_t first_host;
    uint32_t last_host;
    uint32_t capacity_hosts;
    uint8_t prefix;
    uint8_t request_index; /* Original request position, 1..4. */
} aura_vlsm_assignment_t;

typedef struct {
    aura_vlsm_error_t error;
    uint32_t base_network;
    uint8_t base_prefix;
    uint8_t count;
    uint64_t remaining_addresses;
    aura_vlsm_assignment_t assignments[AURA_VLSM_MAX_REQUESTS];
} aura_vlsm_result_t;

/* Stateless, bounded offline helpers; no allocations, I/O, timers or workers.
 * UI/USB callers must enforce an active, awake Eclipse session.
 * RF: MHz,metres,dBm, e.g. "2400,100,-20". Plain decimal only, up to six
 * fractional digits; f 0.001..100000, d 0.001..10000000, P -200..100.
 * VLSM: canonical base CIDR followed by 1..4 positive host counts. Classical
 * subnet allocation reserves network/broadcast and uses prefixes 0..30.
 * Output functions return false on invalid input or truncation; any nonempty
 * output buffer remains NUL-terminated. RF 800/VLSM 1600 bytes are ample. */
bool aura_engineer_tools_rf_calculate(const char *input, aura_rf_result_t *out);
bool aura_engineer_tools_vlsm_calculate(const char *input, aura_vlsm_result_t *out);
bool aura_engineer_tools_format_rf(const char *input, char *out, size_t capacity);
bool aura_engineer_tools_format_vlsm(const char *input, char *out, size_t capacity);
