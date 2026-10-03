#pragma once
#include <stdbool.h>
#include <stddef.h>

/* Form-encoded SSID (32 bytes) and WPA passphrase/PSK (64 bytes). */
#define AURA_PORTAL_BODY_MAX 293
bool aura_portal_parse(const char *body, size_t length, char ssid[33], char pass[65]);
