#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef enum {
    AURA_WIFI_OFF = 0,
    AURA_WIFI_CONNECTING,
    AURA_WIFI_ONLINE,
    AURA_WIFI_SYNCED,
    AURA_WIFI_SETUP,
    AURA_WIFI_ERROR,
    AURA_WIFI_SCANNING,
} aura_wifi_state_t;

typedef struct {
    aura_wifi_state_t state;
    bool configured;
    bool radio_on;
    int8_t rssi;
    uint16_t disconnect_reason;
    int64_t last_sync;
    char ssid[33];
} aura_wifi_status_t;

#define AURA_WIFI_SCAN_MAX 24

typedef struct {
    char ssid[33];
    int8_t rssi;
    uint8_t channel;
    uint8_t authmode;
    uint8_t bssid[6];
} aura_wifi_network_t;

typedef struct {
    bool scanning;
    uint32_t generation;
    uint16_t count;
    esp_err_t error;
    aura_wifi_network_t networks[AURA_WIFI_SCAN_MAX];
} aura_wifi_scan_t;

void aura_wifi_init(void);
void aura_wifi_get_status(aura_wifi_status_t *out);
void aura_wifi_request_sync(void);
void aura_wifi_start_setup(void);
void aura_wifi_forget(void);
void aura_wifi_request_scan(void);
// Cancel active/queued tools scans; normal clock sync and saved credentials remain.
void aura_wifi_cancel_scan(void);
void aura_wifi_get_scan(aura_wifi_scan_t *out);
