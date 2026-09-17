#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include "esp_err.h"

typedef struct {
    bool available;
    bool connected;
    bool usb;
    bool charging;
    int percent;
} aura_battery_t;

void aura_services_init(void);
bool aura_clock_local(struct tm *out);
esp_err_t aura_clock_set(time_t utc, int offset_seconds);
esp_err_t aura_clock_adjust(int seconds);
bool aura_clock_valid(void);
bool aura_rtc_available(void);
int aura_clock_offset(void);
void aura_battery_read(aura_battery_t *out);
int aura_brightness(void);
int aura_theme(void);
bool aura_clock_24h(void);
void aura_save_brightness(int value);
void aura_save_theme(int value);
void aura_save_clock_24h(bool enabled);
uint8_t aura_power_button_events(void);
esp_err_t aura_power_off(void);
