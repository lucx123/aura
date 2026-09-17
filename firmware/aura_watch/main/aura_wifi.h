#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    AURA_WIFI_OFF = 0,
    AURA_WIFI_CONNECTING,
    AURA_WIFI_ONLINE,
    AURA_WIFI_SYNCED,
    AURA_WIFI_SETUP,
    AURA_WIFI_ERROR,
} aura_wifi_state_t;

typedef struct {
    aura_wifi_state_t state;
    bool configured;
    int8_t rssi;
    uint16_t disconnect_reason;
    int64_t last_sync;
    char ssid[33];
} aura_wifi_status_t;

void aura_wifi_init(void);
void aura_wifi_get_status(aura_wifi_status_t *out);
void aura_wifi_request_sync(void);
void aura_wifi_start_setup(void);
void aura_wifi_forget(void);
