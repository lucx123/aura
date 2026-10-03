#include "aura_network_tools.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "esp_wifi_types.h"

typedef struct {
    bool active;
    bool has_scan;
    uint32_t generation;
    esp_err_t error;
    aura_wifi_scan_t scan;
    aura_network_report_t report;
} network_session_t;

static network_session_t session;

static unsigned auth_group(uint8_t mode)
{
    switch (mode) {
    case WIFI_AUTH_OPEN: return 0;
    case WIFI_AUTH_WEP:
    case WIFI_AUTH_WPA_PSK:
    case WIFI_AUTH_WPA_WPA2_PSK:
    case WIFI_AUTH_WPA_ENTERPRISE: return 1;
    case WIFI_AUTH_WPA2_PSK:
    case WIFI_AUTH_WPA2_ENTERPRISE: return 2;
    case WIFI_AUTH_WPA3_PSK:
    case WIFI_AUTH_WPA3_ENT_192:
    case WIFI_AUTH_WPA3_EXT_PSK:
    case WIFI_AUTH_WPA3_EXT_PSK_MIXED_MODE:
    case WIFI_AUTH_WPA3_ENTERPRISE: return 3;
    case WIFI_AUTH_WPA2_WPA3_PSK:
    case WIFI_AUTH_WPA2_WPA3_ENTERPRISE: return 4;
    case WIFI_AUTH_OWE: return 5;
    default: return 6;
    }
}

static const char *auth_name(uint8_t mode)
{
    switch (mode) {
    case WIFI_AUTH_OPEN: return "Abierta";
    case WIFI_AUTH_WEP: return "WEP";
    case WIFI_AUTH_WPA_PSK: return "WPA";
    case WIFI_AUTH_WPA2_PSK: return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-EAP";
    case WIFI_AUTH_WPA3_PSK:
    case WIFI_AUTH_WPA3_EXT_PSK:
    case WIFI_AUTH_WPA3_EXT_PSK_MIXED_MODE: return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/WPA3";
    case WIFI_AUTH_WPA3_ENT_192: return "WPA3-EAP-192";
    case WIFI_AUTH_WPA3_ENTERPRISE: return "WPA3-EAP";
    case WIFI_AUTH_WPA2_WPA3_ENTERPRISE: return "WPA2/3-EAP";
    case WIFI_AUTH_WPA_ENTERPRISE: return "WPA-EAP";
    case WIFI_AUTH_OWE: return "OWE";
    case WIFI_AUTH_WAPI_PSK: return "WAPI";
    case WIFI_AUTH_DPP: return "DPP";
    default: return "Desconocida";
    }
}

void aura_network_tools_analyze(const aura_wifi_scan_t *scan, aura_network_report_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!scan || scan->scanning || scan->error != ESP_OK) return;
    out->sample_count = scan->count < AURA_WIFI_SCAN_MAX ? scan->count : AURA_WIFI_SCAN_MAX;
    for (uint16_t i = 0; i < out->sample_count; ++i) {
        const aura_wifi_network_t *ap = &scan->networks[i];
        switch (auth_group(ap->authmode)) {
        case 0: ++out->open_count; break;
        case 1: ++out->legacy_count; break;
        case 2: ++out->wpa2_count; break;
        case 3: ++out->wpa3_count; break;
        case 4: ++out->transition_count; break;
        case 5: ++out->owe_count; break;
        default: ++out->other_count; break;
        }
        if (ap->channel < 1 || ap->channel > 13) {
            ++out->unmodelled_channels;
            continue;
        }
        ++out->channel_count[ap->channel];
        /* Deliberately an ordinal heuristic, not dBm power or channel airtime.
         * Clamp RSSI and weight nearby primary channels for a nominal 20 MHz
         * plan. No bandwidth/secondary-channel or non-Wi-Fi noise is available. */
        int rssi = ap->rssi;
        if (rssi < -95) rssi = -95;
        if (rssi > -35) rssi = -35;
        unsigned strength = (unsigned)(rssi + 96);
        for (unsigned channel = 1; channel <= 13; ++channel) {
            unsigned distance = channel > ap->channel ? channel - ap->channel : ap->channel - channel;
            if (distance < 5) out->channel_score[channel] += strength * (5 - distance);
        }
    }
    if (out->sample_count == out->unmodelled_channels) return;
    const uint8_t candidates[] = {1, 6, 11};
    uint32_t lowest = UINT32_MAX;
    for (unsigned i = 0; i < sizeof(candidates); ++i) {
        uint8_t channel = candidates[i];
        uint32_t score = out->channel_score[channel];
        if (score < lowest) {
            lowest = score;
            out->best_channel = channel;
            out->best_channel_mask = (uint16_t)(1U << channel);
        } else if (score == lowest) out->best_channel_mask |= (uint16_t)(1U << channel);
    }
}

static bool decimal(const char **cursor, unsigned maximum, unsigned *value)
{
    const char *p = *cursor;
    if (*p < '0' || *p > '9') return false;
    bool zero = *p == '0';
    unsigned parsed = 0, digits = 0;
    while (*p >= '0' && *p <= '9') {
        if (++digits > 3 || (zero && digits > 1)) return false;
        parsed = parsed * 10 + (unsigned)(*p - '0');
        if (parsed > maximum) return false;
        ++p;
    }
    *cursor = p;
    *value = parsed;
    return true;
}

bool aura_network_tools_cidr_parse(const char *cidr, aura_cidr_result_t *out)
{
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    if (!cidr) return false;
    /* Longest canonical form: 255.255.255.255/32. Bound malformed input. */
    size_t length = 0;
    while (length <= 18 && cidr[length]) ++length;
    if (length < 9 || length > 18) return false;
    const char *p = cidr;
    uint32_t address = 0;
    for (unsigned i = 0; i < 4; ++i) {
        unsigned octet;
        if (!decimal(&p, 255, &octet)) return false;
        address = (address << 8) | octet;
        if (*p++ != (i == 3 ? '/' : '.')) return false;
    }
    unsigned prefix;
    if (!decimal(&p, 32, &prefix) || *p) return false;
    uint32_t mask = prefix ? UINT32_MAX << (32 - prefix) : 0;
    uint32_t network = address & mask;
    uint32_t last = network | ~mask;
    out->address = address;
    out->prefix = (uint8_t)prefix;
    out->netmask = mask;
    out->network = network;
    out->last_address = last;
    out->has_broadcast = prefix < 31;
    out->first_host = prefix < 31 ? network + 1 : network;
    out->last_host = prefix < 31 ? last - 1 : last;
    out->host_count = (UINT64_C(1) << (32 - prefix)) - (prefix < 31 ? 2 : 0);
    return true;
}

void aura_network_tools_session_start(uint32_t baseline_scan_generation)
{
    memset(&session, 0, sizeof(session));
    session.active = true;
    session.generation = baseline_scan_generation;
}

void aura_network_tools_session_stop(void)
{
    memset(&session, 0, sizeof(session));
}

bool aura_network_tools_session_active(void) { return session.active; }

bool aura_network_tools_update_scan(const aura_wifi_scan_t *scan)
{
    if (!session.active || !scan || scan->scanning) return false;
    uint32_t delta = scan->generation - session.generation;
    if (!delta || delta >= (UINT32_C(1) << 31)) return false;
    session.generation = scan->generation;
    session.error = scan->error;
    session.has_scan = scan->error == ESP_OK;
    memset(&session.scan, 0, sizeof(session.scan));
    memset(&session.report, 0, sizeof(session.report));
    if (session.has_scan) {
        session.scan = *scan;
        if (session.scan.count > AURA_WIFI_SCAN_MAX) session.scan.count = AURA_WIFI_SCAN_MAX;
        aura_network_tools_analyze(&session.scan, &session.report);
    }
    return true;
}

typedef struct { char *out; size_t capacity, used; bool ok; } writer_t;

static void append(writer_t *writer, const char *format, ...)
{
    if (!writer->ok) return;
    va_list args;
    va_start(args, format);
    int count = vsnprintf(writer->out + writer->used, writer->capacity - writer->used, format, args);
    va_end(args);
    if (count < 0 || (size_t)count >= writer->capacity - writer->used) {
        writer->out[writer->capacity - 1] = 0;
        writer->ok = false;
    } else writer->used += (size_t)count;
}

static bool begin(writer_t *writer, char *out, size_t capacity, bool need_scan)
{
    *writer = (writer_t){.out = out, .capacity = capacity, .ok = out && capacity};
    if (!writer->ok) return false;
    out[0] = 0;
    if (!session.active) {
        append(writer, "Inicia Eclipse para usar esta app.");
        return false;
    }
    if (need_scan && !session.has_scan) {
        if (session.error != ESP_OK) append(writer, "Escaneo fallido (0x%x).\nToca Escanear para reintentar.", (unsigned)session.error);
        else append(writer, "Toca Escanear.\nSolo se activa Wi-Fi durante la busqueda.");
        return false;
    }
    return true;
}

bool aura_network_tools_format_channels(char *out, size_t capacity)
{
    writer_t writer;
    if (!begin(&writer, out, capacity, true)) return false;
    const aura_network_report_t *report = &session.report;
    append(&writer, "%u AP observados\nPlan 20 MHz: canales 1 / 6 / 11\n\n", report->sample_count);
    const uint8_t candidates[] = {1, 6, 11};
    for (unsigned i = 0; i < sizeof(candidates); ++i) {
        unsigned channel = candidates[i];
        append(&writer, "CH %u: indice %" PRIu32 " | %u AP\n", channel, report->channel_score[channel], report->channel_count[channel]);
    }
    if (report->best_channel) {
        append(&writer, "\nMenor indice: ");
        bool first = true;
        for (unsigned i = 0; i < sizeof(candidates); ++i) {
            unsigned channel = candidates[i];
            if (!(report->best_channel_mask & (1U << channel))) continue;
            append(&writer, "%s%u", first ? "" : " / ", channel);
            first = false;
        }
    } else append(&writer, "\nSin AP validos para comparar.");
    append(&writer, "\n\nIndice relativo por RSSI y cercania de canales; menor es mejor.\nNo mide trafico, ruido RF ni ancho real de cada AP.\nMuestra limitada a %u AP.", AURA_WIFI_SCAN_MAX);
    if (report->unmodelled_channels) append(&writer, "\n%u AP fuera de canales 1-13.", report->unmodelled_channels);
    return writer.ok;
}

static void clean_ssid(char *out, const char *ssid)
{
    size_t i = 0;
    while (i < 32 && ssid[i]) {
        unsigned char byte = (unsigned char)ssid[i];
        out[i] = byte < 0x20 || byte == 0x7f ? '?' : (char)byte;
        ++i;
    }
    out[i] = 0;
    if (!i) memcpy(out, "(oculta)", 9);
}

bool aura_network_tools_format_spectrum(char *out, size_t capacity)
{
    writer_t writer;
    if (!begin(&writer, out, capacity, true)) return false;
    append(&writer, "%u AP observados\n\n", session.scan.count);
    for (uint16_t i = 0; i < session.scan.count; ++i) {
        const aura_wifi_network_t *ap = &session.scan.networks[i];
        char name[33];
        clean_ssid(name, ap->ssid);
        append(&writer, "%s\nCH %u | %d dBm | %s\nBSSID %02X:%02X:%02X:%02X:%02X:%02X\n\n",
               name, ap->channel, ap->rssi, auth_name(ap->authmode),
               (unsigned)ap->bssid[0], (unsigned)ap->bssid[1], (unsigned)ap->bssid[2],
               (unsigned)ap->bssid[3], (unsigned)ap->bssid[4], (unsigned)ap->bssid[5]);
    }
    return writer.ok;
}

bool aura_network_tools_format_security(char *out, size_t capacity)
{
    writer_t writer;
    if (!begin(&writer, out, capacity, true)) return false;
    const aura_network_report_t *report = &session.report;
    append(&writer, "%u AP | seguridad anunciada\n\nAbiertas: %u\nWEP / WPA legado: %u\nWPA2: %u | WPA3: %u\nWPA2/3 mixto: %u | OWE: %u\nOtros: %u\n\n", report->sample_count,
           report->open_count, report->legacy_count, report->wpa2_count, report->wpa3_count,
           report->transition_count, report->owe_count, report->other_count);
    append(&writer, "Revisa WEP y WPA legado.\nUna red abierta no cifra el enlace.\nEl anuncio no prueba seguridad ni PMF.\n\n");
    for (uint16_t i = 0; i < session.scan.count; ++i) {
        const aura_wifi_network_t *ap = &session.scan.networks[i];
        char name[33];
        clean_ssid(name, ap->ssid);
        append(&writer, "%s\n%s | CH %u | %d dBm\n\n", name, auth_name(ap->authmode), ap->channel, ap->rssi);
    }
    return writer.ok;
}

static void ip_text(uint32_t value, char text[16])
{
    snprintf(text, 16, "%u.%u.%u.%u", (unsigned)(value >> 24), (unsigned)((value >> 16) & 255),
             (unsigned)((value >> 8) & 255), (unsigned)(value & 255));
}

bool aura_network_tools_calculate_cidr(const char *cidr, char *out, size_t capacity)
{
    writer_t writer;
    if (!begin(&writer, out, capacity, false)) return false;
    aura_cidr_result_t result;
    if (!aura_network_tools_cidr_parse(cidr, &result)) {
        append(&writer, "IPv4/CIDR invalido.\nEjemplo: 192.168.1.10/24\nOctetos 0-255; prefijo 0-32.");
        return false;
    }
    char network[16], mask[16], last[16], first_host[16], last_host[16];
    ip_text(result.network, network);
    ip_text(result.netmask, mask);
    ip_text(result.last_address, last);
    ip_text(result.first_host, first_host);
    ip_text(result.last_host, last_host);
    append(&writer, "Red: %s/%u\nMascara: %s\n%s: %s\n\nHosts: %" PRIu64 "\nPrimero: %s\nUltimo: %s\n\n", network, result.prefix, mask,
           result.has_broadcast ? "Broadcast" : "Ultima direccion", last, result.host_count, first_host, last_host);
    if (result.prefix == 31) append(&writer, "/31: dos hosts en enlace punto a punto (RFC 3021). Sin broadcast de subred.");
    else if (result.prefix == 32) append(&writer, "/32: ruta de un solo host. Sin broadcast de subred.");
    else append(&writer, "Calculo offline. El rango es matematico; comprueba las reservas de tu red.");
    return writer.ok;
}
