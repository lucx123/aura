#include "aura_portal.h"
#include <string.h>

static int hex(unsigned char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static bool decode(const char *input, size_t length, char *out, size_t capacity)
{
    size_t used = 0;
    for (size_t i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)input[i];
        if (c == '%') {
            if (i + 2 >= length) return false;
            int high = hex(input[i + 1]), low = hex(input[i + 2]);
            if (high < 0 || low < 0) return false;
            c = (unsigned char)((high << 4) | low);
            i += 2;
        } else if (c == '+') c = ' ';
        if (!c || used + 1 >= capacity) return false;
        out[used++] = (char)c;
    }
    out[used] = 0;
    return true;
}

bool aura_portal_parse(const char *body, size_t length, char ssid[33], char pass[65])
{
    if (!body || !ssid || !pass) return false;
    ssid[0] = pass[0] = 0;
    if (!length || length > AURA_PORTAL_BODY_MAX) return false;
    bool have_ssid = false, have_pass = false;
    size_t pos = 0;
    while (pos < length) {
        size_t end = pos;
        while (end < length && body[end] != '&') ++end;
        if (end - pos < 2 || body[pos + 1] != '=') goto invalid;
        if (body[pos] == 's' && !have_ssid) {
            if (!decode(body + pos + 2, end - pos - 2, ssid, 33)) goto invalid;
            have_ssid = true;
        } else if (body[pos] == 'p' && !have_pass) {
            if (!decode(body + pos + 2, end - pos - 2, pass, 65)) goto invalid;
            have_pass = true;
        } else goto invalid;
        if (end < length && end + 1 == length) goto invalid;
        pos = end + 1;
    }
    size_t pass_length = strlen(pass);
    if (!have_ssid || !have_pass || !ssid[0]) goto invalid;
    if (pass_length && pass_length < 8) goto invalid;
    if (pass_length == 64) {
        for (size_t i = 0; i < pass_length; ++i) if (hex(pass[i]) < 0) goto invalid;
    }
    return true;
invalid:
    memset(ssid, 0, 33);
    memset(pass, 0, 65);
    return false;
}
