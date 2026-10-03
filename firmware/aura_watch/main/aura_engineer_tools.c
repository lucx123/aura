#include "aura_engineer_tools.h"
#include "aura_network_tools.h"

#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static bool bounded_input(const char *input)
{
    if (!input || !input[0]) return false;
    size_t length = 0;
    while (length <= AURA_ENGINEER_INPUT_MAX && input[length]) ++length;
    return length <= AURA_ENGINEER_INPUT_MAX;
}

static bool plain_decimal(const char **cursor, double *out)
{
    const char *p = *cursor;
    bool negative = *p == '-';
    if (negative) ++p;
    if (*p < '0' || *p > '9') return false;
    bool leading_zero = *p == '0';
    double value = 0;
    unsigned digits = 0;
    while (*p >= '0' && *p <= '9') {
        if (++digits > 8 || (leading_zero && digits > 1)) return false;
        value = value * 10 + (*p++ - '0');
    }
    if (*p == '.') {
        ++p;
        if (*p < '0' || *p > '9') return false;
        unsigned fractions = 0;
        double factor = 0.1;
        while (*p >= '0' && *p <= '9') {
            if (++fractions > 6) return false;
            value += (*p++ - '0') * factor;
            factor *= 0.1;
        }
    }
    *out = negative ? -value : value;
    *cursor = p;
    return isfinite(*out);
}

bool aura_engineer_tools_rf_calculate(const char *input, aura_rf_result_t *out)
{
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    if (!bounded_input(input)) return false;
    const char *p = input;
    aura_rf_result_t result = {0};
    if (!plain_decimal(&p, &result.frequency_mhz) || *p++ != ',' ||
        !plain_decimal(&p, &result.distance_m) || *p++ != ',' ||
        !plain_decimal(&p, &result.power_dbm) || *p) return false;
    if (result.frequency_mhz < 0.001 || result.frequency_mhz > 100000.0 ||
        result.distance_m < 0.001 || result.distance_m > 10000000.0 ||
        result.power_dbm < -200.0 || result.power_dbm > 100.0) return false;
    const double speed_of_light = 299792458.0;
    const double pi = 3.14159265358979323846;
    result.wavelength_m = speed_of_light / (result.frequency_mhz * 1000000.0);
    /* ITU-R P.525-5 section 2.3, equation (5): 20 log10(4 pi d / lambda).
     * Positive, finite bounded inputs keep log10/pow outside invalid domains. */
    result.fspl_db = 20.0 * log10(4.0 * pi * result.distance_m / result.wavelength_m);
    result.power_mw = pow(10.0, result.power_dbm / 10.0);
    result.short_distance = result.distance_m < result.wavelength_m;
    if (!isfinite(result.wavelength_m) || !isfinite(result.fspl_db) ||
        !isfinite(result.power_mw) || result.power_mw <= 0) return false;
    *out = result;
    return true;
}

static bool host_count(const char **cursor, uint32_t *out)
{
    const char *p = *cursor;
    if (*p < '1' || *p > '9') return false;
    uint64_t value = 0;
    unsigned digits = 0;
    while (*p >= '0' && *p <= '9') {
        if (++digits > 10) return false;
        value = value * 10 + (unsigned)(*p++ - '0');
        if (value > UINT64_C(4294967294)) return false;
    }
    *out = (uint32_t)value;
    *cursor = p;
    return true;
}

static bool vlsm_error(aura_vlsm_result_t *out, aura_vlsm_error_t error)
{
    memset(out, 0, sizeof(*out));
    out->error = error;
    return false;
}

bool aura_engineer_tools_vlsm_calculate(const char *input, aura_vlsm_result_t *out)
{
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    if (!bounded_input(input)) return vlsm_error(out, AURA_VLSM_INVALID_INPUT);
    const char *comma = strchr(input, ',');
    if (!comma || comma - input < 9 || comma - input > 18)
        return vlsm_error(out, AURA_VLSM_INVALID_INPUT);
    char base[19];
    size_t base_length = (size_t)(comma - input);
    memcpy(base, input, base_length);
    base[base_length] = 0;
    aura_cidr_result_t cidr;
    if (!aura_network_tools_cidr_parse(base, &cidr) || cidr.address != cidr.network || cidr.prefix > 30)
        return vlsm_error(out, AURA_VLSM_INVALID_BASE);
    aura_vlsm_result_t result = {.base_network = cidr.network, .base_prefix = cidr.prefix};
    const char *p = comma + 1;
    while (true) {
        if (result.count >= AURA_VLSM_MAX_REQUESTS)
            return vlsm_error(out, AURA_VLSM_INVALID_INPUT);
        aura_vlsm_assignment_t *assignment = &result.assignments[result.count];
        if (!host_count(&p, &assignment->requested_hosts))
            return vlsm_error(out, AURA_VLSM_INVALID_INPUT);
        assignment->request_index = ++result.count;
        if (!*p) break;
        if (*p++ != ',') return vlsm_error(out, AURA_VLSM_INVALID_INPUT);
    }
    /* Stable descending allocation. Larger power-of-two blocks come first,
     * preserving original request identity and avoiding alignment gaps. */
    for (unsigned i = 1; i < result.count; ++i) {
        aura_vlsm_assignment_t value = result.assignments[i];
        unsigned j = i;
        while (j && result.assignments[j - 1].requested_hosts < value.requested_hosts) {
            result.assignments[j] = result.assignments[j - 1];
            --j;
        }
        result.assignments[j] = value;
    }
    uint64_t cursor = cidr.network;
    uint64_t end_exclusive = (uint64_t)cidr.last_address + 1;
    for (unsigned i = 0; i < result.count; ++i) {
        aura_vlsm_assignment_t *assignment = &result.assignments[i];
        uint64_t block = 4;
        uint8_t prefix = 30;
        while (block - 2 < assignment->requested_hosts) {
            block <<= 1;
            --prefix;
        }
        uint64_t aligned = (cursor + block - 1) & ~(block - 1);
        if (aligned >= end_exclusive || block > end_exclusive - aligned)
            return vlsm_error(out, AURA_VLSM_NO_SPACE);
        assignment->network = (uint32_t)aligned;
        assignment->broadcast = (uint32_t)(aligned + block - 1);
        assignment->first_host = assignment->network + 1;
        assignment->last_host = assignment->broadcast - 1;
        assignment->capacity_hosts = (uint32_t)(block - 2);
        assignment->prefix = prefix;
        cursor = aligned + block;
    }
    result.remaining_addresses = end_exclusive - cursor;
    *out = result;
    return true;
}

typedef struct { char *out; size_t capacity, used; bool ok; } writer_t;

static writer_t writer_start(char *out, size_t capacity)
{
    if (out && capacity) out[0] = 0;
    return (writer_t){.out = out, .capacity = capacity, .ok = out && capacity};
}

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

bool aura_engineer_tools_format_rf(const char *input, char *out, size_t capacity)
{
    writer_t writer = writer_start(out, capacity);
    if (!writer.ok) return false;
    aura_rf_result_t result;
    if (!aura_engineer_tools_rf_calculate(input, &result)) {
        append(&writer, "Entrada RF invalida o fuera de rango.\nUsa MHz,metros,dBm\nEjemplo: 2400,100,-20\n\nf: 0.001-100000 MHz\nd: 0.001-10000000 m\nP: -200 a 100 dBm\nDecimales simples, sin espacios; hasta 6 cifras decimales.");
        return false;
    }
    append(&writer, "Frecuencia: %.6g MHz\nDistancia: %.6g m\nPotencia: %.6g dBm\n\nLambda: %.6g m\nFSPL: %.2f dB\nPotencia: %.6g mW\n\n", result.frequency_mhz,
           result.distance_m, result.power_dbm, result.wavelength_m, result.fspl_db, result.power_mw);
    append(&writer, "ITU-R P.525: espacio libre ideal, campo lejano y linea de vista.\nSin obstaculos ni ganancias de antena.");
    if (result.short_distance) append(&writer, "\n\nDistancia menor que lambda: verifica que el modelo de campo lejano aplique.");
    return writer.ok;
}

static void ip_text(uint32_t address, char text[16])
{
    snprintf(text, 16, "%u.%u.%u.%u", (unsigned)(address >> 24), (unsigned)((address >> 16) & 255),
             (unsigned)((address >> 8) & 255), (unsigned)(address & 255));
}

bool aura_engineer_tools_format_vlsm(const char *input, char *out, size_t capacity)
{
    writer_t writer = writer_start(out, capacity);
    if (!writer.ok) return false;
    aura_vlsm_result_t result;
    if (!aura_engineer_tools_vlsm_calculate(input, &result)) {
        if (result.error == AURA_VLSM_NO_SPACE)
            append(&writer, "Sin espacio en el bloque base para todas las solicitudes.\nAmplia el bloque o reduce los hosts.\nNo se asignaron subredes parciales.");
        else if (result.error == AURA_VLSM_INVALID_BASE)
            append(&writer, "Base invalida. Usa la direccion de red y prefijo 0-30.\nEjemplo: 192.168.1.0/24\nEsta calculadora reserva red y broadcast; no asigna /31 ni /32.");
        else append(&writer, "Entrada VLSM invalida.\nUsa red/prefijo,hosts1,...hosts4\nEjemplo: 192.168.1.0/24,50,20,10\nDe 1 a 4 solicitudes positivas, sin espacios ni ceros iniciales.");
        return false;
    }
    char base[16];
    ip_text(result.base_network, base);
    append(&writer, "Base: %s/%u\nOrden: mayor a menor\n\n", base, result.base_prefix);
    for (unsigned i = 0; i < result.count; ++i) {
        const aura_vlsm_assignment_t *assignment = &result.assignments[i];
        char network[16], broadcast[16], first[16], last[16];
        ip_text(assignment->network, network);
        ip_text(assignment->broadcast, broadcast);
        ip_text(assignment->first_host, first);
        ip_text(assignment->last_host, last);
        append(&writer, "Solicitud %u: %" PRIu32 " hosts\n%s/%u\nBroadcast: %s\nRango: %s - %s\nCapacidad: %" PRIu32 " hosts\n\n",
               assignment->request_index, assignment->requested_hosts, network, assignment->prefix,
               broadcast, first, last, assignment->capacity_hosts);
    }
    append(&writer, "Direcciones sin asignar: %" PRIu64 "\nReserva red y broadcast en cada subred.\nCalculo offline; verifica las reservas de tu red.", result.remaining_addresses);
    return writer.ok;
}
