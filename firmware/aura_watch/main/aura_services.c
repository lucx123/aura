#include "aura_services.h"

#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include "bsp/esp-bsp.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "aura_services";
static i2c_master_dev_handle_t rtc, pmic;
static SemaphoreHandle_t state_lock;
static nvs_handle_t prefs;
static bool prefs_ready, clock_ready;
static bool clock_24h = true;
static int brightness = 65, theme = 0, utc_offset = 0;

#define AXP2101_COMMON_CONFIG 0x10
#define AXP2101_PWROFF_EN 0x22
#define AXP2101_IRQ_LEVEL 0x27
#define AXP2101_INTEN2 0x41
#define AXP2101_INTSTS2 0x49

static esp_err_t read_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *data, size_t count)
{
    if (!dev) return ESP_ERR_NOT_FOUND;
    return i2c_master_transmit_receive(dev, &reg, 1, data, count, 80);
}

static esp_err_t write_reg(i2c_master_dev_handle_t dev, uint8_t reg, const uint8_t *data, size_t count)
{
    uint8_t buf[9];
    if (!dev || count > 8) return ESP_ERR_INVALID_ARG;
    buf[0] = reg;
    memcpy(buf + 1, data, count);
    return i2c_master_transmit(dev, buf, count + 1, 80);
}

static i2c_master_dev_handle_t attach(uint8_t address)
{
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (i2c_master_probe(bus, address, 80) != ESP_OK) return NULL;
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = 100000,
    };
    i2c_master_dev_handle_t handle = NULL;
    if (i2c_master_bus_add_device(bus, &cfg, &handle) != ESP_OK) return NULL;
    return handle;
}

static int unbcd(uint8_t v)
{
    if ((v & 15) > 9 || (v >> 4) > 9) return -1;
    return (v >> 4) * 10 + (v & 15);
}

static uint8_t bcd(int v) { return ((v / 10) << 4) | (v % 10); }

static bool read_rtc(time_t *epoch)
{
    uint8_t r[7], control;
    if (read_reg(rtc, 0x00, &control, 1) != ESP_OK || (control & 0x22)) return false;
    if (read_reg(rtc, 0x04, r, 7) != ESP_OK || (r[0] & 0x80)) return false;
    struct tm t = {
        .tm_sec = unbcd(r[0] & 0x7f), .tm_min = unbcd(r[1] & 0x7f),
        .tm_hour = unbcd(r[2] & 0x3f), .tm_mday = unbcd(r[3] & 0x3f),
        .tm_mon = unbcd(r[5] & 0x1f) - 1, .tm_year = unbcd(r[6]) + 100,
        .tm_isdst = 0,
    };
    if (t.tm_sec < 0 || t.tm_sec > 59 || t.tm_min < 0 || t.tm_min > 59 ||
        t.tm_hour < 0 || t.tm_hour > 23 || t.tm_mday < 1 || t.tm_mday > 31 ||
        t.tm_mon < 0 || t.tm_mon > 11 || t.tm_year < 124 || t.tm_year > 199) return false;
    struct tm original = t;
    *epoch = mktime(&t); // The process timezone is UTC; display offset is applied separately.
    return *epoch > 0 && t.tm_mday == original.tm_mday && t.tm_mon == original.tm_mon;
}

void aura_services_init(void)
{
    state_lock = xSemaphoreCreateMutex();
    configASSERT(state_lock);
    setenv("TZ", "UTC0", 1);
    tzset();
    esp_err_t err = nvs_flash_init_partition("aura_cfg");
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase_partition("aura_cfg"));
        err = nvs_flash_init_partition("aura_cfg");
    }
    prefs_ready = err == ESP_OK && nvs_open_from_partition("aura_cfg", "prefs", NVS_READWRITE, &prefs) == ESP_OK;
    int32_t value;
    uint8_t rtc_is_utc = 0, saved_clock_24h = 1;
    if (prefs_ready) {
        if (nvs_get_i32(prefs, "brightness", &value) == ESP_OK && value >= 15 && value <= 100) brightness = value;
        if (nvs_get_i32(prefs, "theme", &value) == ESP_OK && value >= 0 && value < 3) theme = value;
        if (nvs_get_i32(prefs, "offset", &value) == ESP_OK && value >= -50400 && value <= 50400) utc_offset = value;
        nvs_get_u8(prefs, "rtc_utc", &rtc_is_utc);
        if (nvs_get_u8(prefs, "clock_24h", &saved_clock_24h) == ESP_OK) clock_24h = saved_clock_24h != 0;
    }
    rtc = attach(0x51);
    pmic = attach(0x34);
    if (pmic) {
        uint8_t value;
        // Report a hold after 1 s; hardware power-off at 8 s; 2 s to power on.
        if (read_reg(pmic, AXP2101_IRQ_LEVEL, &value, 1) == ESP_OK) {
            value = (value & 0xc0) | (0 << 4) | (2 << 2) | 3;
            write_reg(pmic, AXP2101_IRQ_LEVEL, &value, 1);
        }
        // Long PWR hold powers off rather than restarting the PMIC.
        if (read_reg(pmic, AXP2101_PWROFF_EN, &value, 1) == ESP_OK) {
            value = (value | 0x02) & ~0x01;
            write_reg(pmic, AXP2101_PWROFF_EN, &value, 1);
        }
        // Positive edge, negative edge, long press and short press events.
        if (read_reg(pmic, AXP2101_INTEN2, &value, 1) == ESP_OK) {
            value |= 0x0f;
            write_reg(pmic, AXP2101_INTEN2, &value, 1);
        }
        value = 0x0f;
        write_reg(pmic, AXP2101_INTSTS2, &value, 1);
    }
    time_t epoch;
    if (rtc_is_utc == 1 && read_rtc(&epoch)) {
        struct timeval tv = {.tv_sec = epoch};
        clock_ready = settimeofday(&tv, NULL) == 0;
    }
    ESP_LOGI(TAG, "RTC=%s PMIC=%s clock=%s brightness=%d theme=%d",
             rtc ? "found" : "missing", pmic ? "found" : "missing",
             clock_ready ? "restored" : "needs_sync", brightness, theme);
}

bool aura_clock_local(struct tm *out)
{
    xSemaphoreTake(state_lock, portMAX_DELAY);
    bool valid = clock_ready;
    time_t local = time(NULL) + utc_offset;
    if (valid) gmtime_r(&local, out);
    xSemaphoreGive(state_lock);
    return valid;
}

esp_err_t aura_clock_set(time_t utc, int offset_seconds)
{
    if (utc < 1704067200LL || utc >= 4102444800LL || offset_seconds < -50400 ||
        offset_seconds > 50400 || offset_seconds % 60) return ESP_ERR_INVALID_ARG;
    struct tm t;
    gmtime_r(&utc, &t);
    uint8_t r[] = {bcd(t.tm_sec), bcd(t.tm_min), bcd(t.tm_hour), bcd(t.tm_mday),
                   (uint8_t)t.tm_wday, bcd(t.tm_mon + 1), bcd(t.tm_year - 100)};
    xSemaphoreTake(state_lock, portMAX_DELAY);
    uint8_t ctrl = 0;
    esp_err_t err = read_reg(rtc, 0, &ctrl, 1);
    if (err == ESP_OK) {
        ctrl &= ~(0x22); // 24-hour format, oscillator running. Preserve other control bits.
        err = write_reg(rtc, 0, &ctrl, 1);
        if (err == ESP_OK) err = write_reg(rtc, 4, r, sizeof(r));
    }
    struct timeval tv = {.tv_sec = utc};
    if (settimeofday(&tv, NULL) == 0) clock_ready = true;
    utc_offset = offset_seconds;
    if (prefs_ready) {
        nvs_set_i32(prefs, "offset", utc_offset);
        nvs_set_u8(prefs, "rtc_utc", err == ESP_OK ? 1 : 0);
        nvs_commit(prefs);
    }
    xSemaphoreGive(state_lock);
    ESP_LOGI(TAG, "Time synced; RTC=%s offset=%d", esp_err_to_name(err), offset_seconds);
    return err;
}

esp_err_t aura_clock_adjust(int seconds)
{
    if (!aura_clock_valid()) return ESP_ERR_INVALID_STATE;
    return aura_clock_set(time(NULL) + seconds, aura_clock_offset());
}

bool aura_clock_valid(void)
{
    xSemaphoreTake(state_lock, portMAX_DELAY);
    bool valid = clock_ready;
    xSemaphoreGive(state_lock);
    return valid;
}

bool aura_rtc_available(void) { return rtc != NULL; }
int aura_clock_offset(void)
{
    xSemaphoreTake(state_lock, portMAX_DELAY);
    int value = utc_offset;
    xSemaphoreGive(state_lock);
    return value;
}

void aura_battery_read(aura_battery_t *out)
{
    *out = (aura_battery_t){.percent = -1};
    uint8_t status[2], percent;
    if (read_reg(pmic, 0, status, 2) != ESP_OK) return;
    out->available = true;
    out->connected = (status[0] & (1 << 3)) != 0;
    out->usb = (status[0] & (1 << 5)) && !(status[1] & (1 << 3));
    out->charging = (status[1] >> 5) == 1;
    if (out->connected && read_reg(pmic, 0xa4, &percent, 1) == ESP_OK && percent <= 100) out->percent = percent;
}

int aura_brightness(void) { return brightness; }
int aura_theme(void) { return theme; }
bool aura_clock_24h(void) { return clock_24h; }

void aura_save_brightness(int value)
{
    if (value < 15 || value > 100) return;
    xSemaphoreTake(state_lock, portMAX_DELAY);
    brightness = value;
    if (prefs_ready) { nvs_set_i32(prefs, "brightness", value); nvs_commit(prefs); }
    xSemaphoreGive(state_lock);
}

void aura_save_theme(int value)
{
    if (value < 0 || value >= 3) return;
    xSemaphoreTake(state_lock, portMAX_DELAY);
    theme = value;
    if (prefs_ready) { nvs_set_i32(prefs, "theme", value); nvs_commit(prefs); }
    xSemaphoreGive(state_lock);
}

void aura_save_clock_24h(bool enabled)
{
    xSemaphoreTake(state_lock, portMAX_DELAY);
    clock_24h = enabled;
    if (prefs_ready) { nvs_set_u8(prefs, "clock_24h", enabled ? 1 : 0); nvs_commit(prefs); }
    xSemaphoreGive(state_lock);
}

uint8_t aura_power_button_events(void)
{
    uint8_t events = 0;
    if (!pmic || read_reg(pmic, AXP2101_INTSTS2, &events, 1) != ESP_OK) return 0;
    events &= 0x0f;
    if (events) write_reg(pmic, AXP2101_INTSTS2, &events, 1); // RW1C
    return events;
}

esp_err_t aura_power_off(void)
{
    uint8_t value;
    esp_err_t err = read_reg(pmic, AXP2101_COMMON_CONFIG, &value, 1);
    if (err != ESP_OK) return err;
    value |= 0x01; // Software power-off; VRTC remains supplied.
    return write_reg(pmic, AXP2101_COMMON_CONFIG, &value, 1);
}
