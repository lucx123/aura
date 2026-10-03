#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "aura_wifi.h"

/* Pure diagnostics: no sockets, radio, timers, allocation or background tasks. */
typedef struct {
    uint16_t sample_count;
    uint16_t channel_count[14]; /* Indices 1..13; channel 14 is not modelled. */
    uint32_t channel_score[14];
    uint16_t unmodelled_channels;
    uint16_t open_count;
    uint16_t legacy_count;
    uint16_t wpa2_count;
    uint16_t wpa3_count;
    uint16_t transition_count;
    uint16_t owe_count;
    uint16_t other_count;
    uint8_t best_channel; /* Lowest score among 1, 6 and 11; 0 without samples. */
    uint16_t best_channel_mask; /* Bit N represents a tied candidate channel N. */
} aura_network_report_t;

typedef struct {
    uint32_t address;
    uint32_t network;
    uint32_t netmask;
    uint32_t last_address;
    uint32_t first_host;
    uint32_t last_host;
    uint64_t host_count;
    uint8_t prefix;
    bool has_broadcast;
} aura_cidr_result_t;

void aura_network_tools_analyze(const aura_wifi_scan_t *scan, aura_network_report_t *out);
bool aura_network_tools_cidr_parse(const char *cidr, aura_cidr_result_t *out);

/* Session functions are serialized by the caller's display lock, like the UI.
 * Start records the current scan generation. Only later completed snapshots are
 * accepted; stop erases the session's AP names and results. The Wi-Fi worker owns
 * actual scan cancellation/radio shutdown and must stop when Eclipse closes. */
void aura_network_tools_session_start(uint32_t baseline_scan_generation);
void aura_network_tools_session_stop(void);
bool aura_network_tools_session_active(void);
bool aura_network_tools_update_scan(const aura_wifi_scan_t *scan);
bool aura_network_tools_format_spectrum(char *out, size_t capacity);
bool aura_network_tools_format_channels(char *out, size_t capacity);
bool aura_network_tools_format_security(char *out, size_t capacity);
bool aura_network_tools_calculate_cidr(const char *cidr, char *out, size_t capacity);
