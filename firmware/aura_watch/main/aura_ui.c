#include "aura_ui.h"
#include "aura_services.h"
#include "aura_wifi.h"
#include "aura_eclipse.h"
#include "aura_network_tools.h"
#include "aura_engineer_tools.h"
#include "aura_motion.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdatomic.h>
#include "esp_pm.h"
#include <string.h>
#include "esp_err.h"
#include "bsp/display.h"
#include "bsp/esp-bsp.h"
#include "driver/usb_serial_jtag.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "lvgl.h"

#define INK 0x141719
#define PAPER 0xf0f7f4
#define MUTED 0x82938e
#define UI_WIDTH 410
#define PAGE_TOP 62
#define PAGE_HEIGHT 420
#define PAGE_COUNT 17
#define ECLIPSE_GREEN 0x60ff9d
#define PI 3.14159265358979323846f

static const uint32_t colors[] = {0x8af2dd, 0xb6a2ff, 0xffd495};
static const char *weekdays[] = {"Dom", "Lun", "Mar", "Mie", "Jue", "Vie", "Sab"};
static const char *months[] = {"ene", "feb", "mar", "abr", "may", "jun", "jul", "ago", "sep", "oct", "nov", "dic"};
static lv_obj_t *pages[PAGE_COUNT], *battery_label, *local_label, *network_label, *wifi_icon_label;
static int network_header_connected = -1;
static lv_obj_t *eyes[2], *pupils[2], *shine[2], *mouth, *face, *mood_label, *dizzy_stars[4];
static lv_obj_t *home_time, *home_date, *clock_time, *clock_date;
static lv_obj_t *hour_hand, *minute_hand, *second_hand, *clock_dot;
static lv_point_precise_t hour_points[2], minute_points[2], second_points[2];
static lv_obj_t *timer_text, *timer_status, *timer_action, *timer_fill;
static lv_obj_t *stopwatch_text, *stopwatch_status, *stopwatch_action, *stopwatch_lap;
static lv_obj_t *brightness_label, *brightness_slider, *theme_buttons[3], *format_value, *theme_value, *idle_value;
static lv_obj_t *battery_percent, *battery_details, *flashlight_name;
static lv_obj_t *wifi_state_label, *wifi_ssid_label, *wifi_action_label, *wifi_sync_label;
static lv_obj_t *notice, *power_overlay, *power_countdown, *power_hint;
static int current_page, mood, selected_seconds = 300, remaining_seconds = 300;
static int timer_state; // 0 ready, 1 running, 2 paused, 3 finished
static int stopwatch_state, stopwatch_laps; // 0 ready, 1 running, 2 paused
static int64_t stopwatch_started, stopwatch_accumulated;
static int64_t deadline, mood_until, next_gaze, next_blink, blink_until, motion_until;
static int64_t last_activity, notice_until;
static int gaze_x, gaze_y, target_x, target_y;
static atomic_bool sleeping;
static lv_timer_t *animation_timer, *clock_timer, *stopwatch_timer, *battery_timer;
static esp_pm_lock_handle_t cpu_lock;
static bool power_hold_active, power_warning_visible, power_was_sleeping;
static int64_t power_hold_started;
static int64_t sleep_tap_started, touch_wake_guard_until;
static int sleep_tap_x, sleep_tap_y;
static int lock_taps, lock_tap_x, lock_tap_y;
static int64_t lock_tap_started;
static lv_obj_t *version_target;
static bool diagnostic_touch;
static lv_point_t diagnostic_point;
static lv_point_t tap_press_point;
static bool tap_cancelled;
static bool touching;
static bool holding_face;
static int version_taps;
static int64_t version_tap_until;
static int reaction;
static bool eclipse_active;
static lv_obj_t *spectrum_text, *system_text, *evidence_text, *evidence_action;
static lv_obj_t *channels_text, *security_text, *cidr_text, *cidr_input_label, *cidr_keyboard;
static bool cidr_editing;
static char cidr_input_text[32] = "192.168.1.10/24";
static char cidr_saved_input[32];
typedef struct {
    lv_obj_t *text, *input_label;
    char input[AURA_ENGINEER_INPUT_MAX + 1];
} engineer_ui_t;
static engineer_ui_t engineer_tools[2] = {
    { .input = "2400,100,-20" },
    { .input = "192.168.1.0/24,50,20,10" },
};
static lv_obj_t *engineer_keyboard;
static int engineer_editing = -1;
static char engineer_saved_input[AURA_ENGINEER_INPUT_MAX + 1];
static bool flashlight_active;
static int64_t flashlight_until;
static QueueHandle_t evidence_results;
static bool evidence_busy;
static atomic_bool evidence_cancelled;
static bool scan_requested;
static uint32_t scan_requested_generation;
static int rendered_scan_page = -1;
typedef struct {
    esp_err_t error;
    esp_err_t cleanup_error;
    bool cancelled;
    bool was_mounted;
    bool internal;
    aura_eclipse_storage_t storage;
    aura_evidence_status_t evidence;
} evidence_result_t;
static void apply_theme(void);
static void tick(lv_timer_t *timer);
static void stopwatch_tick(lv_timer_t *timer);
static void battery_tick(lv_timer_t *timer);
static void wifi_tick(lv_timer_t *timer);
static void cidr_editor_close(bool restore);
static void engineer_editor_close(bool restore);
static bool wake_only(void);
static void flashlight_stop(void)
{
    if (!flashlight_active) return;
    flashlight_active = false;
    flashlight_until = 0;
    if (!sleeping) bsp_display_brightness_set(aura_brightness());
}
static void configure_cpu(bool sleep_mode)
{
    /* The scheduler briefly requests max frequency even after the UI releases
     * its lock. Cap that maximum while the AMOLED is off as well. */
    esp_pm_config_t profile = {
        .max_freq_mhz = sleep_mode ? 80 : 240,
        .min_freq_mhz = 80,
        .light_sleep_enable = false,
    };
    ESP_ERROR_CHECK(esp_pm_configure(&profile));
}

static void update_timer_rates(void)
{
    if (!animation_timer) return;
    lv_timer_set_period(animation_timer, sleeping && !power_hold_active ? 250 : 33);
    lv_timer_set_period(clock_timer, sleeping ? 1000 : 250);
    lv_timer_set_period(stopwatch_timer, !sleeping && current_page == 5 ? 50 : 1000);
    lv_timer_set_period(battery_timer, sleeping ? 30000 : 5000);
}

/* Avoid reallocating label buffers and invalidating unchanged text. */
static void set_text(lv_obj_t *label, const char *text)
{
    if (!text || strcmp(lv_label_get_text(label), text)) lv_label_set_text(label, text);
}
static int64_t now_us(void) { return esp_timer_get_time(); }
static lv_color_t accent(void) { return lv_color_hex(eclipse_active ? ECLIPSE_GREEN : colors[aura_theme()]); }
static void eclipse_appearance(bool active)
{
    eclipse_active = active;
    mood = 0;
    mood_until = 0;
    set_text(mood_label, active ? "Eclipse activo" : "Estoy contigo");
    for (int i = 0; i < 2; ++i)
        lv_obj_set_style_radius(eyes[i], active ? 8 : 36, 0);
    apply_theme();
}

static lv_obj_t *box(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color, int radius)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_EVENT_BUBBLE);
    return obj;
}

static lv_obj_t *label(lv_obj_t *parent, const char *text, const lv_font_t *font, uint32_t color, int x, int y)
{
    lv_obj_t *obj = lv_label_create(parent);
    set_text(obj, text);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    lv_obj_set_pos(obj, x, y);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_EVENT_BUBBLE);
    return obj;
}

static lv_obj_t *center_label(lv_obj_t *parent, const char *text, const lv_font_t *font, uint32_t color, int y)
{
    lv_obj_t *obj = label(parent, text, font, color, 0, y);
    lv_obj_set_width(obj, UI_WIDTH);
    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, 0);
    return obj;
}

static lv_obj_t *button(lv_obj_t *parent, const char *text, int x, int y, int w, int h,
                        lv_event_cb_t cb, intptr_t value)
{
    lv_obj_t *obj = box(parent, x, y, w, h, INK, 16);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x293739), LV_STATE_PRESSED);
    lv_obj_t *txt = label(obj, text, &lv_font_montserrat_18, PAPER, 0, 0);
    lv_obj_center(txt);
    lv_obj_add_event_cb(obj, cb, LV_EVENT_CLICKED, (void *)value);
    return obj;
}

static lv_obj_t *numeric_keyboard(lv_obj_t *parent, const char *const *keys,
                                  int y, int height, lv_event_cb_t cb)
{
    lv_obj_t *keyboard = lv_buttonmatrix_create(parent);
    lv_obj_remove_style_all(keyboard);
    lv_obj_set_pos(keyboard, 28, y);
    lv_obj_set_size(keyboard, 354, height);
    lv_obj_set_style_pad_all(keyboard, 4, 0);
    lv_obj_set_style_pad_row(keyboard, 6, 0);
    lv_obj_set_style_pad_column(keyboard, 8, 0);
    lv_obj_set_style_bg_color(keyboard, lv_color_hex(INK), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(keyboard, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_radius(keyboard, 14, LV_PART_ITEMS);
    lv_obj_set_style_text_font(keyboard, &lv_font_montserrat_18, LV_PART_ITEMS);
    lv_obj_set_style_text_color(keyboard, lv_color_hex(PAPER), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(keyboard, lv_color_hex(0x344149), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_remove_flag(keyboard, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_EVENT_BUBBLE | LV_OBJ_FLAG_HIDDEN);
    lv_buttonmatrix_set_map(keyboard, keys);
    lv_obj_add_event_cb(keyboard, cb, LV_EVENT_VALUE_CHANGED, NULL);
    return keyboard;
}

static void show_notice(const char *text)
{
    set_text(notice, text);
    lv_obj_remove_flag(notice, LV_OBJ_FLAG_HIDDEN);
    notice_until = now_us() + 2200000;
}

static void eclipse_pause_tools(void)
{
    if (!eclipse_active) return;
    atomic_store(&evidence_cancelled, true);
    // Keep the last completed scan when leaving just before tools_tick runs.
    aura_wifi_scan_t scan;
    aura_wifi_get_scan(&scan);
    if (!scan.scanning && scan.error == ESP_OK) aura_network_tools_update_scan(&scan);
    aura_wifi_cancel_scan();
    scan_requested = false;
    rendered_scan_page = -1;
    cidr_editor_close(true);
    engineer_editor_close(true);
}

static void eclipse_end(void)
{
    if (!eclipse_active) return;
    eclipse_pause_tools();
    aura_network_tools_session_stop();
    eclipse_appearance(false);
}

void aura_ui_eclipse_close(void)
{
    if (!eclipse_active) return;
    eclipse_end();
    aura_ui_page(0);
}

static void eclipse_home(lv_event_t *e)
{
    (void)e;
    if (!wake_only()) aura_ui_page(0);
}

static void eclipse_close(lv_event_t *e)
{
    (void)e;
    aura_ui_eclipse_close();
}

static void tool_open(lv_event_t *e)
{
    if (eclipse_active) aura_ui_page((int)(intptr_t)lv_event_get_user_data(e));
}

void aura_ui_spectrum_start(void)
{
    if (!eclipse_active || sleeping || (current_page != 7 && current_page != 10 && current_page != 11)) return;
    aura_wifi_status_t wifi;
    aura_wifi_get_status(&wifi);
    if (wifi.state == AURA_WIFI_SETUP) {
        show_notice("Cierra primero el portal Wi-Fi");
        return;
    }
    aura_wifi_scan_t scan;
    aura_wifi_get_scan(&scan);
    scan_requested_generation = scan.generation;
    scan_requested = true;
    aura_wifi_request_scan();
    lv_obj_t *result = current_page == 10 ? channels_text :
                       current_page == 11 ? security_text : spectrum_text;
    set_text(result, "Buscando redes 2.4 GHz...\nLa radio se apaga al terminar.");
    last_activity = now_us();
}

static void spectrum_start(lv_event_t *e)
{
    (void)e;
    aura_ui_spectrum_start();
}

static void evidence_update_layout(void)
{
    if (!evidence_action || sleeping || current_page != 9) return;
    lv_obj_update_layout(pages[9]);
    int y = lv_obj_get_y(evidence_text) + lv_obj_get_height(evidence_text) + 24;
    if (y < 325) y = 325;
    if (lv_obj_get_y(evidence_action) != y) lv_obj_set_y(evidence_action, y);
}

static void evidence_set_text(const char *text)
{
    set_text(evidence_text, text);
    evidence_update_layout();
}

static void evidence_worker(void *unused)
{
    (void)unused;
    evidence_result_t result = { .error = ESP_OK };
    if (atomic_load(&evidence_cancelled)) result.cancelled = true;
    else {
        result.error = aura_eclipse_storage_mount();
        if (atomic_load(&evidence_cancelled)) result.cancelled = true;
        else if (result.error == ESP_OK)
            result.error = aura_eclipse_log("system_check", "local_manual_check");
        if (!result.cancelled && result.error != ESP_OK) {
            result.internal = true;
            result.error = aura_eclipse_log_internal("system_check", "local_manual_check");
            aura_eclipse_evidence_status(&result.evidence);
        }
    }
    aura_eclipse_storage_status(&result.storage);
    result.was_mounted = result.storage.mounted;
    // Finish the current FAT operation, then always release host/DMA here.
    result.cleanup_error = aura_eclipse_storage_unmount();
    if (result.error == ESP_OK && result.cleanup_error != ESP_OK)
        result.error = result.cleanup_error;
    aura_eclipse_storage_status(&result.storage);
    xQueueSend(evidence_results, &result, portMAX_DELAY);
    vTaskDelete(NULL);
}

void aura_ui_evidence_start(void)
{
    if (!eclipse_active || sleeping || current_page != 9 || evidence_busy) return;
    atomic_store(&evidence_cancelled, false);
    evidence_busy = true;
    evidence_set_text("Revisando microSD...\nLa pantalla sigue disponible.");
    if (xTaskCreate(evidence_worker, "evidence", 6144, NULL, 2, NULL) != pdPASS) {
        evidence_busy = false;
        evidence_set_text("Sin memoria para iniciar. Intenta otra vez.");
    }
    last_activity = now_us();
}

static void evidence_start(lv_event_t *e)
{
    (void)e;
    aura_ui_evidence_start();
}

bool aura_ui_evidence_busy(void) { return evidence_busy; }

static void cidr_editor_close(bool restore)
{
    if (!cidr_editing) return;
    cidr_editing = false;
    if (restore) {
        strlcpy(cidr_input_text, cidr_saved_input, sizeof(cidr_input_text));
        set_text(cidr_input_label, cidr_input_text);
    }
    lv_obj_add_flag(cidr_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(cidr_text, LV_OBJ_FLAG_HIDDEN);
}

void aura_ui_network_tool_input(const char *cidr)
{
    if (!eclipse_active || sleeping || !cidr) return;
    if (strlen(cidr) >= sizeof(cidr_input_text)) {
        set_text(cidr_text, "La entrada es demasiado larga.\nUsa IPv4/prefijo, por ejemplo\n192.168.1.10/24.");
        return;
    }
    char input[sizeof(cidr_input_text)], result[512];
    strlcpy(input, cidr, sizeof(input));
    cidr_editor_close(false);
    strlcpy(cidr_input_text, input, sizeof(cidr_input_text));
    set_text(cidr_input_label, cidr_input_text[0] ? cidr_input_text : "IPv4/prefijo");
    aura_network_tools_calculate_cidr(cidr_input_text, result, sizeof(result));
    set_text(cidr_text, result);
    last_activity = now_us();
}

static void cidr_edit(lv_event_t *e)
{
    (void)e;
    if (!eclipse_active || sleeping || current_page != 12 || cidr_editing) return;
    strlcpy(cidr_saved_input, cidr_input_text, sizeof(cidr_saved_input));
    cidr_editing = true;
    lv_obj_add_flag(cidr_text, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(cidr_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_scroll_to_y(pages[12], 0, LV_ANIM_OFF);
    last_activity = now_us();
}

static void cidr_key(lv_event_t *e)
{
    (void)e;
    if (!eclipse_active || sleeping || current_page != 12 || !cidr_editing) return;
    uint32_t selected = lv_buttonmatrix_get_selected_button(cidr_keyboard);
    const char *key = lv_buttonmatrix_get_button_text(cidr_keyboard, selected);
    if (!key) return;
    if (!strcmp(key, "Aplicar")) {
        aura_ui_network_tool_input(cidr_input_text);
    } else if (!strcmp(key, LV_SYMBOL_CLOSE)) {
        cidr_editor_close(true);
    } else {
        size_t length = strlen(cidr_input_text);
        if (!strcmp(key, LV_SYMBOL_BACKSPACE)) {
            if (length) cidr_input_text[length - 1] = 0;
        } else if (!strcmp(key, "Borrar")) {
            cidr_input_text[0] = 0;
        } else if (length < 18 && key[0] && !key[1]) {
            cidr_input_text[length] = key[0];
            cidr_input_text[length + 1] = 0;
        }
        set_text(cidr_input_label, cidr_input_text[0] ? cidr_input_text : "IPv4/prefijo");
    }
    last_activity = now_us();
}

static void engineer_input_label(int index)
{
    const char *placeholder = index ? "Red/prefijo,hosts" : "MHz,metros,dBm";
    set_text(engineer_tools[index].input_label,
             engineer_tools[index].input[0] ? engineer_tools[index].input : placeholder);
}

static void engineer_editor_close(bool restore)
{
    if (engineer_editing < 0) return;
    int index = engineer_editing;
    engineer_editing = -1;
    if (restore) {
        strlcpy(engineer_tools[index].input, engineer_saved_input,
                sizeof(engineer_tools[index].input));
        engineer_input_label(index);
    }
    lv_obj_add_flag(engineer_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(engineer_tools[index].text, LV_OBJ_FLAG_HIDDEN);
}

void aura_ui_engineer_input(int page, const char *text)
{
    if (!eclipse_active || sleeping || !text || (page != 15 && page != 16) || current_page != page) return;
    int index = page - 15;
    if (strlen(text) >= sizeof(engineer_tools[index].input)) {
        engineer_editor_close(true);
        set_text(engineer_tools[index].text, "La entrada admite hasta 80 caracteres.\nUsa numeros separados por comas.");
        return;
    }
    char input[sizeof(engineer_tools[index].input)], result[1600];
    strlcpy(input, text, sizeof(input));
    engineer_editor_close(false);
    strlcpy(engineer_tools[index].input, input, sizeof(engineer_tools[index].input));
    engineer_input_label(index);
    if (index) aura_engineer_tools_format_vlsm(input, result, sizeof(result));
    else aura_engineer_tools_format_rf(input, result, sizeof(result));
    set_text(engineer_tools[index].text, result);
    last_activity = now_us();
}

static void engineer_edit(lv_event_t *e)
{
    int page = (int)(intptr_t)lv_event_get_user_data(e);
    if (!eclipse_active || sleeping || (page != 15 && page != 16) || current_page != page || engineer_editing >= 0) return;
    int index = page - 15;
    strlcpy(engineer_saved_input, engineer_tools[index].input, sizeof(engineer_saved_input));
    engineer_editing = index;
    lv_obj_add_flag(engineer_tools[index].text, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_parent(engineer_keyboard, pages[page]);
    lv_obj_set_pos(engineer_keyboard, 28, 184);
    lv_obj_remove_flag(engineer_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_scroll_to_y(pages[page], 0, LV_ANIM_OFF);
    last_activity = now_us();
}

static void engineer_key(lv_event_t *e)
{
    (void)e;
    if (!eclipse_active || sleeping || engineer_editing < 0 || current_page != engineer_editing + 15) return;
    uint32_t selected = lv_buttonmatrix_get_selected_button(engineer_keyboard);
    const char *key = lv_buttonmatrix_get_button_text(engineer_keyboard, selected);
    if (!key) return;
    int index = engineer_editing;
    if (!strcmp(key, "Aplicar")) {
        aura_ui_engineer_input(index + 15, engineer_tools[index].input);
    } else if (!strcmp(key, "Cancelar")) {
        engineer_editor_close(true);
    } else {
        char *input = engineer_tools[index].input;
        size_t length = strlen(input);
        if (!strcmp(key, LV_SYMBOL_BACKSPACE)) {
            if (length) input[length - 1] = 0;
        } else if (!strcmp(key, "Borrar")) {
            input[0] = 0;
        } else if (length + 1 < sizeof(engineer_tools[index].input) && key[0] && !key[1]) {
            input[length] = key[0];
            input[length + 1] = 0;
        }
        engineer_input_label(index);
    }
    last_activity = now_us();
}

static void engineer_editor_action_async(void *data)
{
    int action = (int)(intptr_t)data;
    int page = 15 + action / 32;
    int key = action % 32 - 1;
    if (!eclipse_active || sleeping || current_page != page || (page != 15 && page != 16)) return;
    if (key == -1) {
        lv_obj_t *input = lv_obj_get_parent(engineer_tools[page - 15].input_label);
        lv_obj_send_event(input, LV_EVENT_CLICKED, NULL);
    } else if (key >= 0 && key <= 17 && engineer_editing == page - 15) {
        lv_buttonmatrix_set_selected_button(engineer_keyboard, (uint32_t)key);
        lv_obj_send_event(engineer_keyboard, LV_EVENT_VALUE_CHANGED, NULL);
    }
}

void aura_ui_engineer_editor_action(int key)
{
    if (!eclipse_active || sleeping || (current_page != 15 && current_page != 16) || key < -1 || key > 17) return;
    // Queue only the originating page and key; LVGL resolves objects at dispatch.
    int action = (current_page - 15) * 32 + key + 1;
    lv_async_call(engineer_editor_action_async, (void *)(intptr_t)action);
}

static void tools_tick(lv_timer_t *timer)
{
    (void)timer;
    evidence_result_t result;
    if (xQueueReceive(evidence_results, &result, 0) == pdTRUE) {
        evidence_busy = false;
        char message[480], capacity[48];
        if (result.storage.capacity_bytes)
            snprintf(capacity, sizeof(capacity), "Capacidad: %.1f GB",
                     (double)result.storage.capacity_bytes / 1000000000.0);
        else strlcpy(capacity, "Capacidad sin confirmar", sizeof(capacity));
        const char *format = aura_storage_format_name(result.storage.detected_format);
        if (result.cleanup_error != ESP_OK)
            snprintf(message, sizeof(message), "Formato detectado: %s\n%s\nCierre fallo: %s\nOperacion: %s\n\nNo se confirmo la liberacion\nde la tarjeta.",
                     format, capacity, esp_err_to_name(result.cleanup_error), esp_err_to_name(result.error));
        else if (result.cancelled) strlcpy(message, "Comprobacion cancelada.\nNo se iniciaron nuevos pasos.", sizeof(message));
        else if (result.internal && result.error == ESP_OK && result.evidence.verified)
            snprintf(message, sizeof(message), "Registro interno verificado.\nGuardados: %u de %u\nSe conservan los ultimos %u.\n\nMicroSD: %s\n%s\nMontaje/registro SD: %s\n\nExporta por USB en Eclipse.\nTarjeta liberada al terminar.",
                     result.evidence.count, AURA_EVIDENCE_CAPACITY, AURA_EVIDENCE_CAPACITY,
                     format, capacity, esp_err_to_name(result.storage.last_error));
        else if (result.internal)
            snprintf(message, sizeof(message), "MicroSD: %s\n%s\nOperacion SD: %s\n\nRegistro interno fallo: %s\nTarjeta liberada al terminar.",
                     format, capacity, esp_err_to_name(result.storage.last_error), esp_err_to_name(result.error));
        else if (result.error == ESP_OK && result.storage.last_record_verified)
            snprintf(message, sizeof(message), "Formato detectado: %s\n%s\n\nRegistro guardado y verificado.\n/AURA/eclipse/events.jsonl\n\nTarjeta liberada al terminar.",
                     format, capacity);
        else if (result.error == ESP_OK)
            snprintf(message, sizeof(message), "Formato detectado: %s\n%s\n\nRegistro sin verificar.\nTarjeta liberada al terminar.", format, capacity);
        else if (result.was_mounted)
            snprintf(message, sizeof(message), "Formato detectado: %s\n%s\n\nFallo de preparacion/registro:\n%s\n\nTarjeta liberada al terminar.\nAura no formatea ni borra.",
                     format, capacity, esp_err_to_name(result.error));
        else if (result.storage.probe_error == ESP_OK)
            snprintf(message, sizeof(message), "Formato detectado: %s\n%s\nMontaje fallo: %s\n\nDetectar el formato no confirma\nque se pueda montar.\nAura no formatea ni borra.",
                     format, capacity, esp_err_to_name(result.error));
        else
            snprintf(message, sizeof(message), "Formato sin confirmar\n%s\nMontaje fallo: %s\nLectura fallo: %s\n\nNo se pudo identificar el formato.\nAura no formatea ni borra.",
                     capacity, esp_err_to_name(result.error), esp_err_to_name(result.storage.probe_error));
        evidence_set_text(message);
    }
    if (!eclipse_active || sleeping) return;
    if (current_page == 7 || current_page == 10 || current_page == 11) {
        aura_wifi_scan_t scan;
        aura_wifi_get_scan(&scan);
        if (scan.scanning) return;
        if (scan_requested && scan.generation == scan_requested_generation) return;
        scan_requested = false;
        // Cancelling a new scan does not erase the last completed sample.
        bool changed = scan.error != ESP_ERR_INVALID_STATE && aura_network_tools_update_scan(&scan);
        if (!changed && rendered_scan_page == current_page) return;
        rendered_scan_page = current_page;
        char text[2400];
        if (current_page == 10) aura_network_tools_format_channels(text, sizeof(text));
        else if (current_page == 11) aura_network_tools_format_security(text, sizeof(text));
        else aura_network_tools_format_spectrum(text, sizeof(text));
        set_text(current_page == 10 ? channels_text : current_page == 11 ? security_text : spectrum_text, text);
    } else if (current_page == 8) {
        aura_wifi_status_t wifi; aura_wifi_get_status(&wifi);
        aura_motion_status_t motion; aura_motion_get_status(&motion);
        aura_battery_t battery; aura_battery_read(&battery);
        char text[420];
        snprintf(text, sizeof(text), "Bateria: %d%% | USB: %s\n\nRTC: %s | IMU: %s\nMovimiento: %.2f\nGiro: %.1f\n\nWi-Fi: %s\nEstado: %d | Senal: %d dBm\n\nMemoria libre: %lu KB",
            battery.percent, battery.usb ? "si" : "no", aura_rtc_available() ? "OK" : "no", motion.available ? "OK" : "no",
            motion.movement, motion.rotation, wifi.configured ? wifi.ssid : "sin configurar", wifi.state, wifi.rssi,
            (unsigned long)(xPortGetFreeHeapSize() / 1024));
        set_text(system_text, text);
    }
}

static void eclipse_enter(void)
{
    version_taps = 0;
    version_tap_until = 0;
    if (eclipse_active) { aura_ui_page(6); return; }
    aura_wifi_scan_t scan;
    aura_wifi_get_scan(&scan);
    aura_network_tools_session_start(scan.generation);
    scan_requested = false;
    rendered_scan_page = -1;
    set_text(spectrum_text, "Toca Escanear.\nWi-Fi 2.4 GHz, bajo demanda.");
    set_text(channels_text, "Toca Escanear para comparar\nlos canales 1, 6 y 11.");
    set_text(security_text, "Toca Escanear para revisar\nla seguridad anunciada por Wi-Fi.");
    eclipse_appearance(true);
    aura_ui_page(6);
}

void aura_ui_version_tap(void)
{
    if (sleeping || current_page != 3) return;
    int64_t now = now_us();
    if (now < touch_wake_guard_until) return;
    if (!version_taps || now >= version_tap_until) {
        version_taps = 0;
        version_tap_until = now + 5000000;
    }
    last_activity = now;
    version_taps++;
    if (version_taps >= 7) eclipse_enter();
}

static void version_click(lv_event_t *e)
{
    (void)e;
    aura_ui_version_tap();
}

void aura_ui_menu_scroll(int pixels)
{
    if (sleeping || (current_page != 3 && current_page != 6) || !pixels) return;
    lv_obj_t *menu = pages[current_page];
    lv_obj_update_layout(menu);
    int32_t current = lv_obj_get_scroll_y(menu);
    int32_t maximum = current + lv_obj_get_scroll_bottom(menu);
    int64_t target = (int64_t)current + pixels;
    if (target < 0) target = 0;
    if (target > maximum) target = maximum;
    last_activity = now_us();
    lv_obj_scroll_by(menu, 0, current - (int32_t)target, LV_ANIM_ON);
}

int aura_ui_menu_scroll_y(void)
{
    return lv_obj_get_scroll_y(pages[current_page == 6 ? 6 : 3]);
}

void aura_ui_power_short(void)
{
    if (power_hold_active) return;
    if (sleeping) {
        aura_ui_sleep(0);
        aura_ui_page(0);
        return;
    }
    aura_ui_page(eclipse_active ? (current_page == 6 ? 0 : 6) : (current_page == 3 ? 0 : 3));
}

void aura_ui_power_long(void)
{
    if (power_hold_active) return;
    power_hold_active = true;
    power_warning_visible = false;
    power_was_sleeping = sleeping;
    power_hold_started = now_us();
    update_timer_rates();
}

void aura_ui_power_release(void)
{
    if (!power_hold_active) return;
    power_hold_active = false;
    power_warning_visible = false;
    lv_obj_add_flag(power_overlay, LV_OBJ_FLAG_HIDDEN);
    if (power_was_sleeping) {
        aura_ui_sleep(0);
        aura_ui_page(0);
    } else aura_ui_sleep(1);
}

void aura_ui_page(int page)
{
    if (page < 0 || page >= PAGE_COUNT) return;
    if (((page >= 6 && page <= 12) || page >= 15) && !eclipse_active) return;
    // Sleep keeps home selected; wake explicitly before changing visible pages.
    if (sleeping && page != 0) return;
    // RF/VLSM are offline: pause radio/storage jobs without ending the mode.
    if ((page <= 6 || page >= 13) && eclipse_active) eclipse_pause_tools();
    if (page != 12) cidr_editor_close(true);
    if (engineer_editing >= 0 && page != engineer_editing + 15) engineer_editor_close(true);
    if (page != 14) flashlight_stop();
    if (page != 3) { version_taps = 0; version_tap_until = 0; }
    lock_taps = 0;
    lock_tap_started = 0;
    tap_cancelled = true;
    current_page = page;
    touching = false;
    for (int i = 0; i < PAGE_COUNT; ++i) {
        if (i == page) lv_obj_remove_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
    }
    last_activity = now_us();
    update_timer_rates();
    if (clock_timer && !sleeping) { tick(NULL); stopwatch_tick(NULL); }
    if (current_page == 12) aura_ui_network_tool_input(cidr_input_text);
    if (current_page == 15 || current_page == 16)
        aura_ui_engineer_input(current_page, engineer_tools[current_page - 15].input);
    if (current_page == 9) evidence_update_layout();
    if (current_page == 13 && battery_timer) battery_tick(NULL);
    if (current_page == 14 && !flashlight_active) {
        flashlight_active = true;
        flashlight_until = now_us() + 30000000;
        bsp_display_brightness_set(90);
    }
}

void aura_ui_boot_long(void)
{
    if (!sleeping && current_page == 3) eclipse_enter();
}

void aura_ui_sleep(int sleep)
{
    bool next = sleep != 0;
    if (next == sleeping) { if (!next) last_activity = now_us(); return; }
    sleeping = next;
    touching = false;
    lock_taps = 0;
    lock_tap_started = 0;
    tap_cancelled = true;
    if (sleeping) {
        version_taps = 0;
        version_tap_until = 0;
        eclipse_pause_tools();
        sleep_tap_started = 0;
        aura_ui_page(0);
        bsp_display_brightness_set(0);
        ESP_ERROR_CHECK_WITHOUT_ABORT(aura_display_set_sleep(true));
        ESP_ERROR_CHECK(esp_pm_lock_release(cpu_lock));
        configure_cpu(true);
    } else {
        configure_cpu(false);
        ESP_ERROR_CHECK(esp_pm_lock_acquire(cpu_lock));
        ESP_ERROR_CHECK_WITHOUT_ABORT(aura_display_set_sleep(false));
        tick(NULL);
        battery_tick(NULL);
        wifi_tick(NULL);
        bsp_display_brightness_set(aura_brightness());
    }
    last_activity = now_us();
    update_timer_rates();
}

bool aura_ui_is_sleeping(void)
{
    return sleeping;
}

int aura_ui_current_page(void) { return current_page; }
bool aura_ui_eclipse_active(void) { return eclipse_active; }

void aura_ui_motion(float x, float y)
{
    if (sleeping || current_page != 0 || touching || mood == 5) return;
    int horizontal = (int)(y * 1.7f);
    int vertical = (int)(x * 1.4f);
    if (horizontal < -18) horizontal = -18;
    if (horizontal > 18) horizontal = 18;
    if (vertical < -14) vertical = -14;
    if (vertical > 14) vertical = 14;
    target_x = horizontal;
    target_y = vertical;
    motion_until = now_us() + 180000;
}

void aura_ui_dizzy(void)
{
    if (sleeping || current_page != 0) return;
    aura_ui_page(0);
    mood = 5;
    mood_until = now_us() + 4800000;
    motion_until = mood_until;
    set_text(mood_label, "Estoy viendo estrellitas...");
}

static bool wake_only(void)
{
    int64_t now = now_us();
    if (sleeping || now < touch_wake_guard_until) return true;
    last_activity = now;
    return false;
}

static bool touch_point(lv_point_t *point)
{
    if (diagnostic_touch) { *point = diagnostic_point; return true; }
    lv_indev_t *input = lv_indev_active();
    if (!input || lv_indev_get_type(input) != LV_INDEV_TYPE_POINTER) return false;
    lv_indev_get_point(input, point);
    return true;
}

static void root_event(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) {
        tap_cancelled = !touch_point(&tap_press_point) || power_hold_active || now_us() < touch_wake_guard_until;
        return;
    }
    if (code == LV_EVENT_PRESSING || code == LV_EVENT_RELEASED) {
        lv_point_t point;
        if (touch_point(&point)) {
            int dx = point.x - tap_press_point.x, dy = point.y - tap_press_point.y;
            if (dx * dx + dy * dy > 256) tap_cancelled = true;
        }
        if (code == LV_EVENT_PRESSING) return;
    }
    if (code == LV_EVENT_SCROLL_BEGIN || code == LV_EVENT_LONG_PRESSED || code == LV_EVENT_PRESS_LOST) {
        tap_cancelled = true;
        lock_taps = 0;
        lock_tap_started = 0;
        sleep_tap_started = 0;
        return;
    }
    int64_t now = now_us();
    if (code == LV_EVENT_SHORT_CLICKED) {
        lv_obj_t *target = lv_event_get_target_obj(e);
        while (target && !lv_obj_has_flag(target, LV_OBJ_FLAG_CLICKABLE)) target = lv_obj_get_parent(target);
        // Only backgrounds and Aura's face; controls keep their normal clicks.
        bool background = target == lv_screen_active() || target == face;
        for (int i = 0; i < PAGE_COUNT && !background; ++i) background = target == pages[i];
        if (!background || tap_cancelled || sleeping || power_hold_active || now < touch_wake_guard_until) {
            lock_taps = 0;
            lock_tap_started = 0;
            return;
        }
        lv_point_t point;
        if (!touch_point(&point)) return;
        int dx = point.x - lock_tap_x, dy = point.y - lock_tap_y;
        if (!lock_taps || now - lock_tap_started > 750000 || dx * dx + dy * dy > 1600) {
            lock_taps = 0;
            lock_tap_started = now;
            lock_tap_x = point.x;
            lock_tap_y = point.y;
        }
        if (++lock_taps == 3) {
            touch_wake_guard_until = now + 400000;
            aura_ui_sleep(1);
        }
        return;
    }
    if (code != LV_EVENT_RELEASED || now < touch_wake_guard_until) return;
    if (tap_cancelled) {
        lock_taps = 0;
        lock_tap_started = 0;
        sleep_tap_started = 0;
        return;
    }
    if (sleeping) {
        lv_point_t point;
        if (!touch_point(&point)) return;
        int dx = point.x - sleep_tap_x;
        int dy = point.y - sleep_tap_y;
        bool fast = sleep_tap_started && now - sleep_tap_started <= 500000;
        bool nearby = dx * dx + dy * dy <= 1600;
        if (fast && nearby) {
            sleep_tap_started = 0;
            touch_wake_guard_until = now + 400000;
            aura_ui_sleep(0);
            aura_ui_page(0);
        } else {
            sleep_tap_started = now;
            sleep_tap_x = point.x;
            sleep_tap_y = point.y;
        }
    } else last_activity = now;
}

void aura_ui_diagnostic_tap(int x, int y, int target_kind)
{
    if (x < 0 || x >= UI_WIDTH || y < 0 || y >= 502 || target_kind < 0 || target_kind > 3) return;
    lv_obj_t *target = target_kind == 1 ? brightness_slider :
                       target_kind == 2 ? version_target :
                       target_kind == 3 ? face : pages[current_page];
    diagnostic_point = (lv_point_t){.x = x, .y = y};
    diagnostic_touch = true;
    lv_obj_send_event(target, LV_EVENT_PRESSED, NULL);
    lv_obj_send_event(target, LV_EVENT_RELEASED, NULL);
    lv_obj_send_event(target, LV_EVENT_SHORT_CLICKED, NULL);
    lv_obj_send_event(target, LV_EVENT_CLICKED, NULL);
    diagnostic_touch = false;
}

void aura_ui_diagnostic_drag(int x, int y, int end_x, int end_y)
{
    lv_obj_t *target = current_page == 0 ? face : pages[current_page];
    diagnostic_touch = true;
    diagnostic_point = (lv_point_t){.x = x, .y = y};
    lv_obj_send_event(target, LV_EVENT_PRESSED, NULL);
    diagnostic_point = (lv_point_t){.x = end_x, .y = end_y};
    lv_obj_send_event(target, LV_EVENT_PRESSING, NULL);
    lv_obj_send_event(target, LV_EVENT_RELEASED, NULL);
    // A short drag on a non-scrolling object can still emit SHORT_CLICKED.
    lv_obj_send_event(target, LV_EVENT_SHORT_CLICKED, NULL);
    diagnostic_touch = false;
}

static void face_event(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSING) {
        if (holding_face) return;
        if (wake_only()) return;
        lv_indev_t *input = lv_indev_active();
        if (!input) return;
        lv_point_t p;
        lv_indev_get_point(input, &p);
        target_x = (p.x - 205) / 9;
        target_y = (p.y - 180) / 13;
        if (target_x < -18) target_x = -18;
        if (target_x > 18) target_x = 18;
        if (target_y < -16) target_y = -16;
        if (target_y > 16) target_y = 16;
        touching = true;
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        holding_face = false;
        touching = false;
        next_gaze = now_us() + 1800000;
    } else if (code == LV_EVENT_SHORT_CLICKED) {
        if (wake_only()) return;
        reaction = (reaction % 3) + 1;
        mood = reaction;
        mood_until = now_us() + 2200000;
        const char *messages[] = {"", "Eso me gusto", "Te estoy mirando", "Un guino para ti"};
        set_text(mood_label, messages[mood]);
    } else if (code == LV_EVENT_LONG_PRESSED) {
        if (wake_only()) return;
        holding_face = true;
        mood = 4;
        mood_until = now_us() + 2600000;
        set_text(mood_label, "Mmm... sigue");
    }
}

static void animate(lv_timer_t *t)
{
    (void)t;
    int64_t now = now_us();
    if (power_hold_active) {
        int elapsed = (int)((now - power_hold_started) / 1000000);
        // The PMIC reports a long press after about one second. Keep the
        // screen-lock gesture silent; only warn from five seconds total.
        if (elapsed >= 4 && !power_warning_visible) {
            power_warning_visible = true;
            if (sleeping) aura_ui_sleep(0);
            set_text(power_hint, power_was_sleeping ?
                              "Suelta para encender la pantalla" :
                              "Suelta para apagar la pantalla");
            lv_obj_remove_flag(power_overlay, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(power_overlay);
        }
        if (power_warning_visible) {
            int remaining = 7 - elapsed;
            char count[12];
            snprintf(count, sizeof(count), "%d", remaining > 0 ? remaining : 0);
            set_text(power_countdown, count);
            if (remaining <= 0) {
                power_hold_active = false;
                power_warning_visible = false;
                set_text(power_countdown, "...");
                aura_power_off();
            }
        }
    }
    if (notice_until && now > notice_until) {
        lv_obj_add_flag(notice, LV_OBJ_FLAG_HIDDEN);
        notice_until = 0;
    }
    if (flashlight_active && now >= flashlight_until) aura_ui_page(3);
    if (!sleeping && now - last_activity > (int64_t)aura_idle_timeout() * 1000000) aura_ui_sleep(1);
    if (sleeping || current_page != 0) return;
    if (!sleeping && !touching && mood != 5 && now > motion_until && now > next_gaze) {
        target_x = (int)(esp_random() % 29) - 14;
        target_y = (int)(esp_random() % 17) - 8;
        next_gaze = now + 1800000 + esp_random() % 2800000;
    }
    if (mood == 5) {
        float phase = (float)(now % 620000) / 620000.0f * 2.0f * PI;
        target_x = (int)(sinf(phase) * 18.0f);
        target_y = (int)(cosf(phase) * 12.0f);
    }
    gaze_x += (target_x - gaze_x) / 3;
    gaze_y += (target_y - gaze_y) / 3;
    if (!sleeping && now > next_blink) {
        blink_until = now + 150000;
        next_blink = now + 2700000 + esp_random() % 3500000;
    }
    if (mood && now > mood_until) {
        mood = 0;
        target_x = target_y = 0;
        next_gaze = now + 900000;
        set_text(mood_label, sleeping ? "Descansando" : eclipse_active ? "Eclipse activo" : "Estoy contigo");
    }
    for (int i = 0; i < 4; ++i) {
        if (mood == 5) {
            float phase = (float)(now % 850000) / 850000.0f * 2.0f * PI + i * PI / 2.0f;
            lv_obj_set_pos(dizzy_stars[i], 200 + (int)(cosf(phase) * 137.0f),
                           45 + (int)(sinf(phase) * 31.0f));
            lv_obj_remove_flag(dizzy_stars[i], LV_OBJ_FLAG_HIDDEN);
        } else lv_obj_add_flag(dizzy_stars[i], LV_OBJ_FLAG_HIDDEN);
    }
    for (int i = 0; i < 2; ++i) {
        int height = eclipse_active ? 46 : 112, width = eclipse_active ? 94 : 86;
        if (sleeping) height = 9;
        else if (now < blink_until && mood != 5) height = 10;
        else if (mood == 1) { height = 29; width = 94; }
        else if (mood == 2 && i == 0) { height = 133; width = 93; }
        else if (mood == 3 && i == 1) height = 12;
        else if (mood == 4) { height = 14; width = 92; }
        else if (mood == 5) { height = 88; width = 88; }
        lv_obj_set_size(eyes[i], width, height);
        lv_obj_set_pos(eyes[i], (i == 0 ? 98 : 226) + gaze_x / 3, 109 - height / 2 + gaze_y / 3);
        lv_obj_set_style_bg_color(eyes[i], accent(), 0);
        if (height < 40) {
            lv_obj_add_flag(pupils[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(shine[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_remove_flag(pupils[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(shine[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(pupils[i], width / 2 - 17 + gaze_x, height / 2 - 29 + gaze_y);
            lv_obj_set_pos(shine[i], width / 2 - 8 + gaze_x, height / 2 - 21 + gaze_y);
        }
    }
    int mouth_width = 20, mouth_height = 4;
    if (mood == 1) { mouth_width = 38; mouth_height = 8; }
    else if (mood == 2) { mouth_width = 14; mouth_height = 14; }
    else if (mood == 3) { mouth_width = 28; mouth_height = 5; }
    else if (mood == 4 || sleeping) { mouth_width = 16; mouth_height = 3; }
    else if (mood == 5) { mouth_width = 34; mouth_height = 6; }
    lv_obj_set_size(mouth, mouth_width, mouth_height);
    lv_obj_set_pos(mouth, 205 - mouth_width / 2, (mood == 5 ? 211 : 202) - mouth_height / 2);
    lv_obj_set_style_bg_color(mouth, accent(), 0);
}

static void set_hand(lv_obj_t *hand, lv_point_precise_t pts[2], float angle, int length)
{
    pts[0] = (lv_point_precise_t){205, 151};
    pts[1] = (lv_point_precise_t){205 + sinf(angle) * length, 151 - cosf(angle) * length};
    lv_line_set_points(hand, pts, 2);
}

void aura_ui_timer_start(int seconds)
{
    if (seconds < 1 || seconds > 5999) return;
    selected_seconds = remaining_seconds = seconds;
    deadline = now_us() + (int64_t)seconds * 1000000;
    timer_state = 1;
    aura_ui_sleep(0);
    aura_ui_page(2);
}

static void timer_click(lv_event_t *e)
{
    if (wake_only()) return;
    int action = (intptr_t)lv_event_get_user_data(e);
    if (action > 0) {
        selected_seconds = remaining_seconds = action;
        timer_state = 0;
    } else if (action == -1) {
        timer_state = 0;
        remaining_seconds = selected_seconds;
    } else if (timer_state == 1) {
        int64_t left = deadline - now_us();
        remaining_seconds = left > 0 ? (int)((left + 999999) / 1000000) : 0;
        timer_state = 2;
    } else if (timer_state == 3) {
        remaining_seconds = selected_seconds;
        timer_state = 0;
    } else {
        if (remaining_seconds <= 0) remaining_seconds = selected_seconds;
        deadline = now_us() + (int64_t)remaining_seconds * 1000000;
        timer_state = 1;
    }
}

static int64_t stopwatch_elapsed(void)
{
    return stopwatch_accumulated + (stopwatch_state == 1 ? now_us() - stopwatch_started : 0);
}

void aura_ui_stopwatch_action(int action)
{
    int64_t elapsed = stopwatch_elapsed();
    if (action == -1) {
        stopwatch_state = 0;
        stopwatch_laps = 0;
        stopwatch_accumulated = 0;
        set_text(stopwatch_lap, "Sin vueltas todavia");
    } else if (action == 1) {
        if (stopwatch_state != 1) return;
        stopwatch_laps++;
        char lap[64];
        int64_t centiseconds = elapsed / 10000;
        snprintf(lap, sizeof(lap), "Vuelta %d  -  %02lld:%02lld.%02lld", stopwatch_laps,
                 centiseconds / 6000, (centiseconds / 100) % 60, centiseconds % 100);
        set_text(stopwatch_lap, lap);
        show_notice("Aura guardo tu vuelta");
    } else if (stopwatch_state == 1) {
        stopwatch_accumulated = elapsed;
        stopwatch_state = 2;
    } else {
        stopwatch_started = now_us();
        stopwatch_state = 1;
    }
    last_activity = now_us();
}

static void stopwatch_click(lv_event_t *e)
{
    if (wake_only()) return;
    aura_ui_stopwatch_action((intptr_t)lv_event_get_user_data(e));
}

static void stopwatch_tick(lv_timer_t *timer)
{
    (void)timer;
    if (!stopwatch_text || sleeping || current_page != 5) return;
    int64_t centiseconds = stopwatch_elapsed() / 10000;
    char value[24];
    snprintf(value, sizeof(value), "%02lld:%02lld.%02lld", centiseconds / 6000,
             (centiseconds / 100) % 60, centiseconds % 100);
    set_text(stopwatch_text, value);
    const char *states[] = {"Listo cuando tu quieras", "Midiendo tu momento", "Cronometro en pausa"};
    const char *actions[] = {"Iniciar", "Pausar", "Continuar"};
    set_text(stopwatch_status, states[stopwatch_state]);
    set_text(stopwatch_action, actions[stopwatch_state]);
}

static void tick(lv_timer_t *t)
{
    (void)t;
    if (timer_state == 1) {
        int64_t left = deadline - now_us();
        remaining_seconds = left > 0 ? (int)((left + 999999) / 1000000) : 0;
        if (!remaining_seconds) {
            timer_state = 3;
            aura_ui_sleep(0);
            aura_ui_page(2);
        }
    }
    if (sleeping) return;
    struct tm local;
    char hhmm[16] = "--:--", date[64] = "Sin hora: sincroniza por USB";
    if (aura_clock_local(&local)) {
        if (aura_clock_24h()) {
            snprintf(hhmm, sizeof(hhmm), "%02d:%02d", local.tm_hour, local.tm_min);
        } else {
            int hour = local.tm_hour % 12;
            if (!hour) hour = 12;
            snprintf(hhmm, sizeof(hhmm), "%d:%02d %s", hour, local.tm_min, local.tm_hour < 12 ? "AM" : "PM");
        }
        snprintf(date, sizeof(date), "%s %d %s  %d", weekdays[local.tm_wday], local.tm_mday,
                 months[local.tm_mon], local.tm_year + 1900);
        if (current_page == 1) {
        set_hand(hour_hand, hour_points, (local.tm_hour % 12 + local.tm_min / 60.f) * PI / 6, 69);
        set_hand(minute_hand, minute_points, (local.tm_min + local.tm_sec / 60.f) * PI / 30, 99);
        set_hand(second_hand, second_points, local.tm_sec * PI / 30, 106);
        }
    }
    if (current_page == 0) { set_text(home_time, hhmm); set_text(home_date, date); }
    if (current_page == 1) { set_text(clock_time, hhmm); set_text(clock_date, date); }
    if (current_page == 2) {
    char count[16];
    snprintf(count, sizeof(count), "%02d:%02d", remaining_seconds / 60, remaining_seconds % 60);
    set_text(timer_text, count);
    const char *states[] = {"Elige tu pausa", "Tu momento esta en marcha", "En pausa", "Tiempo cumplido"};
    const char *actions[] = {"Empezar", "Pausar", "Continuar", "Listo"};
    set_text(timer_status, states[timer_state]);
    set_text(timer_action, actions[timer_state]);
    lv_obj_set_width(timer_fill, selected_seconds ? 314 * remaining_seconds / selected_seconds : 0);
    lv_obj_set_style_text_color(timer_text,
        timer_state == 3 && ((now_us() / 1000000) % 2) ? lv_color_hex(PAPER) : accent(), 0);
    }
    aura_wifi_status_t wifi;
    aura_wifi_get_status(&wifi);
    const char *presence = wifi.state == AURA_WIFI_CONNECTING ? "Conectando" :
                           wifi.state == AURA_WIFI_ONLINE ? "En linea" :
                           wifi.state == AURA_WIFI_SETUP ? "En portal" :
                           wifi.state == AURA_WIFI_SYNCED ? "Hora lista" :
                           sleeping ? "Descanso" : timer_state == 1 ? "En pausa" : "Tranquila";
    set_text(local_label, presence);
}

static void battery_tick(lv_timer_t *timer)
{
    (void)timer;
    if (sleeping) return;
    aura_battery_t battery;
    aura_battery_read(&battery);
    char text[40];
    if (!battery.available) snprintf(text, sizeof(text), "Bateria --");
    else if (battery.percent >= 0) snprintf(text, sizeof(text), "%s%d%%", battery.charging ? LV_SYMBOL_CHARGE " " : "", battery.percent);
    else snprintf(text, sizeof(text), "%s", battery.usb ? "USB" : "Bateria --");
    set_text(battery_label, text);
    lv_obj_set_style_text_color(battery_label,
        battery.percent >= 0 && battery.percent < 15 ? lv_color_hex(0xff9b85) : lv_color_hex(PAPER), 0);
    if (current_page == 13) {
        if (battery.available && battery.percent >= 0)
            snprintf(text, sizeof(text), "%d%%", battery.percent);
        else strlcpy(text, "--", sizeof(text));
        set_text(battery_percent, text);
        char details[160];
        if (!battery.available) strlcpy(details, "Estado de bateria no disponible", sizeof(details));
        else snprintf(details, sizeof(details), "Bateria: %s\n\nUSB: %s\nCarga: %s",
            battery.connected ? "conectada" : "no detectada",
            battery.usb ? "conectado" : "desconectado",
            battery.charging ? "en progreso" : "sin cargar");
        set_text(battery_details, details);
    }
}

static void brightness_event(lv_event_t *e)
{
    last_activity = now_us();
    int value = lv_slider_get_value(brightness_slider);
    char text[32];
    snprintf(text, sizeof(text), "Brillo  %d%%", value);
    set_text(brightness_label, text);
    bsp_display_brightness_set(value);
    if (lv_event_get_code(e) == LV_EVENT_RELEASED) aura_save_brightness(value);
}

static void apply_theme(void)
{
    for (int i = 0; i < 3; ++i) {
        lv_obj_set_style_border_color(theme_buttons[i],
            lv_color_hex(i == aura_theme() ? PAPER : colors[i]), 0);
        lv_obj_set_style_border_width(theme_buttons[i], i == aura_theme() ? 2 : 0, 0);
    }
    lv_obj_set_style_line_color(minute_hand, accent(), 0);
    lv_obj_set_style_bg_color(clock_dot, accent(), 0);
    lv_obj_set_style_bg_color(timer_fill, accent(), 0);
    if (power_countdown) lv_obj_set_style_text_color(power_countdown, accent(), 0);
    if (wifi_icon_label) lv_obj_set_style_text_color(wifi_icon_label, accent(), 0);
    if (battery_percent) lv_obj_set_style_text_color(battery_percent, accent(), 0);
    lv_obj_set_style_bg_color(brightness_slider, accent(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(brightness_slider, accent(), LV_PART_KNOB);
}

static void format_click(lv_event_t *e)
{
    (void)e;
    if (wake_only()) return;
    aura_save_clock_24h(!aura_clock_24h());
    set_text(format_value, aura_clock_24h() ? "24 h" : "12 h");
    show_notice(aura_clock_24h() ? "Reloj en 24 horas" : "Reloj en 12 horas");
    tick(NULL);
}

static void sleep_click(lv_event_t *e) { (void)e; aura_ui_sleep(1); }

static void idle_cycle(lv_event_t *e)
{
    (void)e;
    if (wake_only()) return;
    const int choices[] = {15, 30, 45, 90};
    int next = 15;
    for (int i = 0; i < 4; ++i)
        if (aura_idle_timeout() == choices[i]) next = choices[(i + 1) % 4];
    aura_save_idle_timeout(next);
    char value[24];
    snprintf(value, sizeof(value), "%d segundos", aura_idle_timeout());
    set_text(idle_value, value);
}

static void battery_save(lv_event_t *e)
{
    (void)e;
    if (wake_only()) return;
    aura_save_brightness(35);
    aura_save_idle_timeout(15);
    bsp_display_brightness_set(aura_brightness());
    lv_slider_set_value(brightness_slider, aura_brightness(), LV_ANIM_OFF);
    char value[32];
    snprintf(value, sizeof(value), "Brillo  %d%%", aura_brightness());
    set_text(brightness_label, value);
    snprintf(value, sizeof(value), "%d segundos", aura_idle_timeout());
    set_text(idle_value, value);
    show_notice(aura_brightness() == 35 && aura_idle_timeout() == 15 ?
                "Ahorro aplicado" : "No pude guardar todos los ajustes");
}

static void flashlight_select(lv_event_t *e)
{
    if (wake_only() || current_page != 14 || !flashlight_active) return;
    int color = (int)(intptr_t)lv_event_get_user_data(e);
    static const uint32_t shades[] = {0xffffff, 0xffdca4, 0xff3025};
    static const char *names[] = {"Blanca", "Calida", "Roja"};
    if (color < 0 || color >= 3) return;
    lv_obj_set_style_bg_color(pages[14], lv_color_hex(shades[color]), 0);
    set_text(flashlight_name, names[color]);
}

/* Static vector icons keep the app grid light enough to redraw while scrolling. */
typedef enum { MENU_WIFI, MENU_TIMER, MENU_STOPWATCH, MENU_CLOCK, MENU_SYSTEM,
               MENU_BATTERY, MENU_FLASHLIGHT, MENU_CHANNELS, MENU_AUDIT, MENU_CIDR,
               MENU_EVIDENCE, MENU_RF, MENU_VLSM } menu_icon_t;
static const lv_point_precise_t menu_hands[][3] = {
    {{18, 8}, {18, 18}, {10, 24}},
    {{18, 7}, {18, 18}, {26, 12}},
    {{18, 7}, {18, 18}, {27, 22}},
};

static lv_draw_buf_t *menu_tile_images[14];
static unsigned menu_tile_image_count;
static lv_obj_t *menu_tile_objects[14];
static unsigned menu_tile_object_count;
static unsigned menu_cache_stack_free;

unsigned aura_ui_menu_cache_count(void) { return menu_tile_image_count; }
unsigned aura_ui_menu_cache_stack_free(void) { return menu_cache_stack_free; }

static void menu_tile_feedback(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    if (code != LV_EVENT_PRESSED && code != LV_EVENT_RELEASED && code != LV_EVENT_PRESS_LOST) return;
    lv_obj_t *image = lv_event_get_user_data(event);
    if (code == LV_EVENT_PRESSED) lv_obj_add_state(image, LV_STATE_PRESSED);
    else lv_obj_remove_state(image, LV_STATE_PRESSED);
}

static void menu_tile_cache(lv_obj_t *tile)
{
    if (menu_tile_image_count >= sizeof(menu_tile_images) / sizeof(menu_tile_images[0])) return;
    lv_obj_update_layout(tile);
    lv_draw_buf_t *snapshot = lv_snapshot_take(tile, LV_COLOR_FORMAT_RGB565);
    if (!snapshot) return;  // Keep the original objects if allocation fails.
    uint32_t children = lv_obj_get_child_count(tile);
    lv_obj_t *image = lv_image_create(tile);
    lv_obj_remove_style_all(image);
    lv_image_set_src(image, snapshot);
    lv_obj_set_pos(image, 0, 0);
    lv_obj_remove_flag(image, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(image, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_style_image_opa(image, LV_OPA_80, LV_STATE_PRESSED);
    for (uint32_t i = 0; i < children; ++i)
        lv_obj_add_flag(lv_obj_get_child(tile, i), LV_OBJ_FLAG_HIDDEN);
    // Pixels live for the same lifetime as the prebuilt menu. Tile events stay live.
    menu_tile_images[menu_tile_image_count++] = snapshot;
    lv_obj_set_style_bg_opa(tile, LV_OPA_TRANSP, 0);
    lv_obj_add_event_cb(tile, menu_tile_feedback, LV_EVENT_ALL, image);
}

static void menu_tiles_cache_async(void *unused)
{
    (void)unused;
    // Snapshot drawing needs the LVGL task's stack, after construction completes.
    for (unsigned i = 0; i < menu_tile_object_count; ++i)
        menu_tile_cache(menu_tile_objects[i]);
    menu_cache_stack_free = (unsigned)uxTaskGetStackHighWaterMark(NULL);
    ESP_LOGI("aura_ui", "Menu caches=%u, LVGL stack free=%u", menu_tile_image_count,
             menu_cache_stack_free);
}

static lv_obj_t *menu_tile(lv_obj_t *parent, const char *title, uint32_t icon_color,
                           menu_icon_t kind, int x, int y, lv_event_cb_t cb, intptr_t value)
{
    lv_obj_t *tile = box(parent, x, y, 171, 112, INK, 28);
    lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(tile, lv_color_hex(0x282e32), LV_STATE_PRESSED);
    lv_obj_t *orb = box(tile, 53, 12, 64, 64, icon_color, LV_RADIUS_CIRCLE);
    if (kind == MENU_CHANNELS) {
        box(orb, 15, 34, 8, 16, PAPER, 3);
        box(orb, 28, 23, 8, 27, PAPER, 3);
        box(orb, 41, 12, 8, 38, PAPER, 3);
    } else if (kind == MENU_WIFI || kind >= MENU_SYSTEM) {
        const char *glyph = kind == MENU_WIFI ? LV_SYMBOL_WIFI :
            kind == MENU_SYSTEM ? LV_SYMBOL_SETTINGS : kind == MENU_BATTERY ? LV_SYMBOL_BATTERY_FULL :
            kind == MENU_FLASHLIGHT ? LV_SYMBOL_CHARGE : kind == MENU_AUDIT ? LV_SYMBOL_OK :
            kind == MENU_CIDR ? "IP" : kind == MENU_RF ? "RF" :
            kind == MENU_VLSM ? LV_SYMBOL_LIST : LV_SYMBOL_SD_CARD;
        lv_obj_t *icon = label(orb, glyph,
            kind == MENU_CIDR || kind == MENU_RF ? &lv_font_montserrat_24 : &lv_font_montserrat_28, PAPER, 0, 0);
        lv_obj_center(icon);
    } else {
        int dial_y = kind == MENU_CLOCK ? 14 : 18;
        lv_obj_t *dial = box(orb, 14, dial_y, 36, 36, icon_color, LV_RADIUS_CIRCLE);
        lv_obj_set_style_border_color(dial, lv_color_hex(PAPER), 0);
        lv_obj_set_style_border_width(dial, 2, 0);
        lv_obj_t *hands = lv_line_create(dial);
        lv_obj_remove_style_all(hands);
        lv_obj_set_style_line_color(hands, lv_color_hex(PAPER), 0);
        lv_obj_set_style_line_width(hands, 2, 0);
        lv_obj_set_style_line_rounded(hands, true, 0);
        lv_line_set_points(hands, menu_hands[kind - MENU_TIMER], 3);
        lv_obj_remove_flag(hands, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(hands, LV_OBJ_FLAG_EVENT_BUBBLE);
        if (kind != MENU_CLOCK) box(orb, 27, 10, 10, 4, PAPER, 2);
        if (kind == MENU_STOPWATCH) box(orb, 47, 18, 6, 4, PAPER, 1);
    }
    lv_obj_t *text = label(tile, title, &lv_font_montserrat_18, PAPER, 0, 84);
    lv_obj_set_width(text, 171);
    lv_obj_set_style_text_align(text, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_event_cb(tile, cb, LV_EVENT_CLICKED, (void *)value);
    if (menu_tile_object_count < sizeof(menu_tile_objects) / sizeof(menu_tile_objects[0]))
        menu_tile_objects[menu_tile_object_count++] = tile;
    return tile;
}

static lv_obj_t *menu_control(lv_obj_t *parent, const char *title, const char *value,
                              int x, int y, lv_event_cb_t cb, lv_obj_t **value_label)
{
    lv_obj_t *control = box(parent, x, y, 171, 68, INK, 20);
    lv_obj_add_flag(control, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(control, lv_color_hex(0x282e32), LV_STATE_PRESSED);
    label(control, title, &lv_font_montserrat_14, MUTED, 16, 10);
    *value_label = label(control, value, &lv_font_montserrat_18, PAPER, 16, 34);
    lv_obj_add_event_cb(control, cb, LV_EVENT_CLICKED, NULL);
    return control;
}

static void menu_scroll_event(lv_event_t *e)
{
    if (sleeping || lv_event_get_target_obj(e) != pages[current_page]) return;
    last_activity = now_us();
    /* Dragging the page cannot count towards the hidden entry gesture. */
    version_taps = 0;
    version_tap_until = 0;
}

static void wifi_open(lv_event_t *e) { (void)e; if (!wake_only()) aura_ui_page(4); }
static void timer_open(lv_event_t *e) { (void)e; if (!wake_only()) aura_ui_page(2); }
static void clock_open(lv_event_t *e) { (void)e; if (!wake_only()) aura_ui_page(1); }
static void stopwatch_open(lv_event_t *e) { (void)e; if (!wake_only()) aura_ui_page(5); }
static void battery_open(lv_event_t *e) { (void)e; if (!wake_only()) aura_ui_page(13); }
static void flashlight_open(lv_event_t *e) { (void)e; if (!wake_only()) aura_ui_page(14); }

static void theme_cycle(lv_event_t *e)
{
    (void)e;
    if (wake_only()) return;
    int next = (aura_theme() + 1) % 3;
    aura_save_theme(next);
    const char *names[] = {"Menta", "Lila", "Sol"};
    set_text(theme_value, names[next]);
    apply_theme();
}

static void wifi_primary(lv_event_t *e)
{
    (void)e;
    if (wake_only()) return;
    aura_wifi_status_t wifi;
    aura_wifi_get_status(&wifi);
    if (wifi.configured) {
        aura_wifi_request_sync();
        show_notice("Aura esta buscando internet");
    } else {
        aura_wifi_start_setup();
        show_notice("Busca la red AURA-SETUP");
    }
}

static void wifi_setup(lv_event_t *e)
{
    (void)e;
    if (wake_only()) return;
    aura_wifi_start_setup();
    show_notice("Conecta tu telefono a AURA-SETUP");
}

static void wifi_forget_click(lv_event_t *e)
{
    (void)e;
    if (wake_only()) return;
    aura_wifi_forget();
    show_notice("Red olvidada");
}

static void wifi_back(lv_event_t *e) { (void)e; if (!wake_only()) aura_ui_page(3); }
static void menu_back(lv_event_t *e) { (void)e; if (!wake_only()) aura_ui_page(3); }

static void wifi_tick(lv_timer_t *timer)
{
    (void)timer;
    if (sleeping) return;
    aura_wifi_status_t wifi;
    aura_wifi_get_status(&wifi);
    bool connected = wifi.radio_on && wifi.state == AURA_WIFI_ONLINE;
    int header_connected = connected && wifi.ssid[0];
    // Setting an identical local style still invalidates it in LVGL 9.5.
    if (header_connected != network_header_connected) {
        network_header_connected = header_connected;
        if (header_connected) {
            lv_obj_remove_flag(wifi_icon_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(network_label, 73, 29);
            lv_obj_set_width(network_label, 80);
            lv_obj_set_style_text_font(network_label, &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_color(network_label, lv_color_hex(MUTED), 0);
        } else {
            lv_obj_add_flag(wifi_icon_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(network_label, 52, 24);
            lv_obj_set_width(network_label, 105);
            lv_obj_set_style_text_font(network_label, &lv_font_montserrat_20, 0);
            lv_obj_set_style_text_color(network_label, lv_color_hex(PAPER), 0);
        }
    }
    set_text(network_label, header_connected ? wifi.ssid : "Aura");
    const char *states[] = {"Wi-Fi en reposo", "Buscando tu red", "Conectada", "Hora sincronizada",
                            "Lista para configurar", "No pude conectar", "Escaneando redes"};
    set_text(wifi_state_label, states[wifi.state]);
    char network[64];
    if (wifi.state == AURA_WIFI_SETUP) snprintf(network, sizeof(network), "AURA-SETUP  -  abre 192.168.4.1");
    else snprintf(network, sizeof(network), "%s%s", wifi.configured ? wifi.ssid : "Sin red guardada",
                  wifi.state == AURA_WIFI_ONLINE && wifi.rssi ? "  · en linea" : "");
    set_text(wifi_ssid_label, network);
    set_text(wifi_action_label, wifi.configured ? "Sincronizar ahora" : "Configurar Wi-Fi");
    char last[64] = "La hora aun no se ha sincronizado";
    if (wifi.last_sync > 0) {
        time_t local_epoch = (time_t)wifi.last_sync + aura_clock_offset();
        struct tm local;
        gmtime_r(&local_epoch, &local);
        snprintf(last, sizeof(last), "Ultima vez: %02d:%02d - %d %s", local.tm_hour, local.tm_min,
                 local.tm_mday, months[local.tm_mon]);
    }
    set_text(wifi_sync_label, last);
}

static lv_obj_t *hand(lv_obj_t *parent, int width, uint32_t color)
{
    lv_obj_t *obj = lv_line_create(parent);
    lv_obj_set_style_line_width(obj, width, 0);
    lv_obj_set_style_line_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_line_rounded(obj, true, 0);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    return obj;
}

void aura_ui_init(void)
{
    ESP_ERROR_CHECK(esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "aura_ui", &cpu_lock));
    ESP_ERROR_CHECK(esp_pm_lock_acquire(cpu_lock));
    lv_obj_t *screen = lv_screen_active();
    for (lv_indev_t *input = lv_indev_get_next(NULL); input; input = lv_indev_get_next(input)) {
        if (lv_indev_get_type(input) != LV_INDEV_TYPE_POINTER) continue;
        lv_indev_set_scroll_limit(input, 8);
        lv_indev_set_scroll_throw(input, 8);
    }
    lv_obj_set_style_bg_color(screen, lv_color_hex(0), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(screen, root_event, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(screen, root_event, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(screen, root_event, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(screen, root_event, LV_EVENT_PRESS_LOST, NULL);
    lv_obj_add_event_cb(screen, root_event, LV_EVENT_SHORT_CLICKED, NULL);
    lv_obj_add_event_cb(screen, root_event, LV_EVENT_LONG_PRESSED, NULL);
    lv_obj_add_event_cb(screen, root_event, LV_EVENT_SCROLL_BEGIN, NULL);
    wifi_icon_label = label(screen, LV_SYMBOL_WIFI, &lv_font_montserrat_14, colors[aura_theme()], 52, 29);
    lv_obj_add_flag(wifi_icon_label, LV_OBJ_FLAG_HIDDEN);
    network_label = label(screen, "Aura", &lv_font_montserrat_20, PAPER, 52, 24);
    lv_label_set_long_mode(network_label, LV_LABEL_LONG_DOT);
    local_label = label(screen, "Tranquila", &lv_font_montserrat_14, MUTED, 165, 28);
    lv_obj_set_width(local_label, 90);
    lv_obj_set_style_text_align(local_label, LV_TEXT_ALIGN_CENTER, 0);
    battery_label = label(screen, "--", &lv_font_montserrat_16, PAPER, 300, 26);
    lv_obj_set_width(battery_label, 58);
    lv_obj_set_style_text_align(battery_label, LV_TEXT_ALIGN_RIGHT, 0);
    for (int i = 0; i < PAGE_COUNT; ++i) {
        pages[i] = box(screen, 0, PAGE_TOP, UI_WIDTH, PAGE_HEIGHT, 0, 0);
        lv_obj_add_flag(pages[i], LV_OBJ_FLAG_CLICKABLE);
    }

    face = box(pages[0], 0, 0, UI_WIDTH, 224, 0, 0);
    lv_obj_add_flag(face, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(face, face_event, LV_EVENT_ALL, NULL);
    for (int i = 0; i < 2; ++i) {
        eyes[i] = box(face, 98 + 128 * i, 53, 86, 112, colors[aura_theme()], 36);
        pupils[i] = box(eyes[i], 27, 29, 32, 54, 0x06110f, 16);
        shine[i] = box(eyes[i], 36, 36, 8, 14, PAPER, 5);
    }
    for (int i = 0; i < 4; ++i) {
        dizzy_stars[i] = label(face, "*", &lv_font_montserrat_24,
                               i % 2 ? 0xffd495 : colors[aura_theme()], 0, 0);
        lv_obj_add_flag(dizzy_stars[i], LV_OBJ_FLAG_HIDDEN);
    }
    mouth = box(face, 195, 200, 20, 4, colors[aura_theme()], LV_RADIUS_CIRCLE);
    home_time = center_label(pages[0], "--:--", &lv_font_montserrat_48, PAPER, 236);
    home_date = center_label(pages[0], "", &lv_font_montserrat_18, MUTED, 294);
    mood_label = center_label(pages[0], "Estoy contigo", &lv_font_montserrat_14, MUTED, 334);

    button(pages[1], LV_SYMBOL_LEFT, 28, 4, 54, 46, menu_back, 0);
    lv_obj_t *dial = box(pages[1], 78, 24, 254, 254, 0, LV_RADIUS_CIRCLE);
    lv_obj_set_style_border_width(dial, 1, 0);
    lv_obj_set_style_border_color(dial, lv_color_hex(INK), 0);
    for (int i = 0; i < 60; ++i) {
        float a = i * PI / 30;
        int size = i % 5 == 0 ? 5 : 2;
        box(pages[1], 205 + sinf(a) * 118 - size / 2, 151 - cosf(a) * 118 - size / 2,
            size, size, i % 5 == 0 ? PAPER : 0x374440, LV_RADIUS_CIRCLE);
    }
    hour_hand = hand(pages[1], 8, PAPER);
    minute_hand = hand(pages[1], 5, colors[aura_theme()]);
    second_hand = hand(pages[1], 2, 0xb6a2ff);
    clock_dot = box(pages[1], 199, 145, 12, 12, colors[aura_theme()], LV_RADIUS_CIRCLE);
    clock_time = center_label(pages[1], "--:--", &lv_font_montserrat_28, PAPER, 293);
    clock_date = center_label(pages[1], "", &lv_font_montserrat_18, MUTED, 338);

    button(pages[2], LV_SYMBOL_LEFT, 28, 4, 54, 46, menu_back, 0);
    center_label(pages[2], "Un momento para ti", &lv_font_montserrat_24, PAPER, 25);
    timer_text = center_label(pages[2], "05:00", &lv_font_montserrat_48, colors[aura_theme()], 91);
    timer_status = center_label(pages[2], "Elige tu pausa", &lv_font_montserrat_18, MUTED, 152);
    lv_obj_t *track = box(pages[2], 48, 191, 314, 4, INK, 2);
    timer_fill = box(track, 0, 0, 314, 4, colors[aura_theme()], 2);
    button(pages[2], "5 min", 28, 225, 110, 49, timer_click, 300);
    button(pages[2], "15 min", 150, 225, 110, 49, timer_click, 900);
    button(pages[2], "25 min", 272, 225, 110, 49, timer_click, 1500);
    lv_obj_t *start = button(pages[2], "Empezar", 28, 301, 226, 58, timer_click, 0);
    timer_action = lv_obj_get_child(start, 0);
    button(pages[2], LV_SYMBOL_REFRESH, 270, 301, 112, 58, timer_click, -1);

    lv_obj_add_flag(pages[3], LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_MOMENTUM |
                              LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_set_scroll_dir(pages[3], LV_DIR_VER);
    lv_obj_set_scrollbar_mode(pages[3], LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_bottom(pages[3], 20, 0);
    lv_obj_add_event_cb(pages[3], menu_scroll_event, LV_EVENT_SCROLL_BEGIN, NULL);
    lv_obj_add_event_cb(pages[3], menu_scroll_event, LV_EVENT_SCROLL, NULL);
    center_label(pages[3], "Tus apps", &lv_font_montserrat_24, PAPER, 6);
    menu_tile(pages[3], "Wi-Fi", 0x176354, MENU_WIFI, 28, 50, wifi_open, 0);
    menu_tile(pages[3], "Timer", 0x946027, MENU_TIMER, 211, 50, timer_open, 0);
    menu_tile(pages[3], "Crono", 0x345b8a, MENU_STOPWATCH, 28, 174, stopwatch_open, 0);
    menu_tile(pages[3], "Reloj", 0x665098, MENU_CLOCK, 211, 174, clock_open, 0);
    menu_tile(pages[3], "Bateria", 0x416945, MENU_BATTERY, 28, 298, battery_open, 0);
    menu_tile(pages[3], "Linterna", 0x7c6535, MENU_FLASHLIGHT, 211, 298, flashlight_open, 0);
    char text[32];
    snprintf(text, sizeof(text), "Brillo  %d%%", aura_brightness());
    lv_obj_t *brightness_card = box(pages[3], 28, 424, 354, 76, INK, 22);
    brightness_label = label(brightness_card, text, &lv_font_montserrat_18, PAPER, 20, 12);
    brightness_slider = lv_slider_create(brightness_card);
    lv_obj_set_pos(brightness_slider, 24, 55);
    lv_obj_set_size(brightness_slider, 306, 6);
    lv_slider_set_range(brightness_slider, 15, 100);
    lv_slider_set_value(brightness_slider, aura_brightness(), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(0x343b40), LV_PART_MAIN);
    lv_obj_set_style_radius(brightness_slider, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_pad_all(brightness_slider, 6, LV_PART_KNOB);
    lv_obj_set_style_shadow_width(brightness_slider, 0, LV_PART_KNOB);
    lv_obj_set_ext_click_area(brightness_slider, 14);
    lv_obj_add_flag(brightness_slider, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_event_cb(brightness_slider, brightness_event, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(brightness_slider, brightness_event, LV_EVENT_RELEASED, NULL);
    menu_control(pages[3], "Hora", aura_clock_24h() ? "24 h" : "12 h", 28, 514,
                 format_click, &format_value);
    lv_obj_t *appearance = menu_control(pages[3], "Color",
        aura_theme() == 0 ? "Menta" : aura_theme() == 1 ? "Lila" : "Sol",
        211, 514, theme_cycle, &theme_value);
    for (int i = 0; i < 3; ++i) {
        theme_buttons[i] = box(appearance, 106 + i * 17, 39, 12, 12, colors[i], LV_RADIUS_CIRCLE);
    }
    snprintf(text, sizeof(text), "%d segundos", aura_idle_timeout());
    lv_obj_t *idle_control = menu_control(pages[3], "Apagar pantalla tras", text,
                                          28, 596, idle_cycle, &idle_value);
    lv_obj_set_width(idle_control, 354);
    label(idle_control, LV_SYMBOL_REFRESH, &lv_font_montserrat_18, MUTED, 316, 25);
    button(pages[3], LV_SYMBOL_POWER "  Descansar", 28, 678, 354, 56, sleep_click, 0);
    lv_obj_t *version = box(pages[3], 28, 746, 354, 44, 0, 0);
    version_target = version;
    lv_obj_add_flag(version, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *version_text = label(version, "AURA Watch - Basic 1.4 dev.6",
                                  &lv_font_montserrat_14, MUTED, 0, 14);
    lv_obj_set_width(version_text, 354);
    lv_obj_set_style_text_align(version_text, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_event_cb(version, version_click, LV_EVENT_CLICKED, NULL);
    lv_obj_set_height(pages[3], PAGE_HEIGHT);

    button(pages[4], LV_SYMBOL_LEFT, 28, 4, 54, 46, wifi_back, 0);
    center_label(pages[4], "Conexion", &lv_font_montserrat_24, PAPER, 11);
    lv_obj_t *wifi_orb = box(pages[4], 157, 45, 96, 96, 0x173c38, LV_RADIUS_CIRCLE);
    lv_obj_t *wifi_mark = label(wifi_orb, "Wi", &lv_font_montserrat_28, 0x8af2dd, 0, 0);
    lv_obj_center(wifi_mark);
    wifi_state_label = center_label(pages[4], "Wi-Fi en reposo", &lv_font_montserrat_20, PAPER, 153);
    wifi_ssid_label = center_label(pages[4], "Sin red guardada", &lv_font_montserrat_14, MUTED, 185);
    wifi_sync_label = center_label(pages[4], "La hora aun no se ha sincronizado", &lv_font_montserrat_14, MUTED, 211);
    lv_obj_t *primary = button(pages[4], "Configurar Wi-Fi", 42, 242, 326, 54, wifi_primary, 0);
    wifi_action_label = lv_obj_get_child(primary, 0);
    button(pages[4], "Cambiar red", 42, 307, 156, 45, wifi_setup, 0);
    button(pages[4], "Olvidar", 212, 307, 156, 45, wifi_forget_click, 0);

    button(pages[5], LV_SYMBOL_LEFT, 28, 4, 54, 46, menu_back, 0);
    center_label(pages[5], "Cronometro Aura", &lv_font_montserrat_24, PAPER, 13);
    stopwatch_text = center_label(pages[5], "00:00.00", &lv_font_montserrat_48, colors[aura_theme()], 103);
    stopwatch_status = center_label(pages[5], "Listo cuando tu quieras", &lv_font_montserrat_18, MUTED, 169);
    stopwatch_lap = center_label(pages[5], "Sin vueltas todavia", &lv_font_montserrat_14, MUTED, 216);
    lv_obj_t *stopwatch_main = button(pages[5], "Iniciar", 28, 278, 226, 60, stopwatch_click, 0);
    stopwatch_action = lv_obj_get_child(stopwatch_main, 0);
    button(pages[5], "Vuelta", 270, 278, 112, 60, stopwatch_click, 1);
    button(pages[5], LV_SYMBOL_REFRESH "  Reiniciar", 92, 338, 226, 48, stopwatch_click, -1);

    /* Eclipse opens the prebuilt workspace immediately after its hidden gesture. */
    button(pages[6], LV_SYMBOL_LEFT, 28, 4, 54, 44, eclipse_home, 0);
    center_label(pages[6], "Aura Eclipse", &lv_font_montserrat_28, ECLIPSE_GREEN, 6);
    lv_obj_add_flag(pages[6], LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_MOMENTUM |
                              LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_set_scroll_dir(pages[6], LV_DIR_VER);
    lv_obj_set_scrollbar_mode(pages[6], LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_bottom(pages[6], 20, 0);
    lv_obj_add_event_cb(pages[6], menu_scroll_event, LV_EVENT_SCROLL_BEGIN, NULL);
    lv_obj_add_event_cb(pages[6], menu_scroll_event, LV_EVENT_SCROLL, NULL);
    menu_tile(pages[6], "Spectrum", 0x176354, MENU_WIFI, 28, 54, tool_open, 7);
    menu_tile(pages[6], "Canales", 0x345b8a, MENU_CHANNELS, 211, 54, tool_open, 10);
    menu_tile(pages[6], "Auditoria", 0x7d456c, MENU_AUDIT, 28, 178, tool_open, 11);
    menu_tile(pages[6], "Sistema", 0x665098, MENU_SYSTEM, 211, 178, tool_open, 8);
    menu_tile(pages[6], "IPv4 / CIDR", 0x356b71, MENU_CIDR, 28, 302, tool_open, 12);
    menu_tile(pages[6], "Evidence", 0x946027, MENU_EVIDENCE, 211, 302, tool_open, 9);
    menu_tile(pages[6], "RF / enlace", 0x365774, MENU_RF, 28, 426, tool_open, 15);
    menu_tile(pages[6], "VLSM", 0x695583, MENU_VLSM, 211, 426, tool_open, 16);
    lv_obj_t *exit = button(pages[6], "Salir de Eclipse", 28, 554, 354, 52, eclipse_close, 0);
    lv_obj_set_style_border_color(exit, lv_color_hex(ECLIPSE_GREEN), 0);
    lv_obj_set_style_border_width(exit, 1, 0);
    for (int i = 7; i < PAGE_COUNT; ++i) {
        if (i == 13 || i == 14) continue;
        lv_obj_add_flag(pages[i], LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_MOMENTUM);
        lv_obj_set_scroll_dir(pages[i], LV_DIR_VER);
        lv_obj_add_event_cb(pages[i], menu_scroll_event, LV_EVENT_SCROLL_BEGIN, NULL);
        lv_obj_add_event_cb(pages[i], menu_scroll_event, LV_EVENT_SCROLL, NULL);
        button(pages[i], LV_SYMBOL_LEFT, 28, 4, 54, 44, tool_open, 6);
    }
    label(pages[7], "Spectrum", &lv_font_montserrat_24, ECLIPSE_GREEN, 99, 14);
    button(pages[7], "Escanear", 252, 4, 126, 44, spectrum_start, 0);
    spectrum_text = label(pages[7], "Toca Escanear.\nWi-Fi 2.4 GHz, bajo demanda.", &lv_font_montserrat_16, PAPER, 36, 73);
    lv_obj_set_width(spectrum_text, 338);
    label(pages[8], "System Check", &lv_font_montserrat_24, ECLIPSE_GREEN, 102, 14);
    system_text = label(pages[8], "Leyendo...", &lv_font_montserrat_18, PAPER, 36, 78);
    lv_obj_set_width(system_text, 338);
    label(pages[9], "Evidence", &lv_font_montserrat_24, ECLIPSE_GREEN, 110, 14);
    evidence_text = label(pages[9], "Revisa la microSD y guarda\nun registro local de comprobacion.\n\nLa tarjeta no se formatea.", &lv_font_montserrat_18, PAPER, 36, 78);
    lv_obj_set_width(evidence_text, 338);
    evidence_action = button(pages[9], "Revisar y guardar registro", 38, 325, 334, 54, evidence_start, 0);
    lv_obj_set_style_pad_bottom(pages[9], 24, 0);

    label(pages[10], "Canales", &lv_font_montserrat_24, ECLIPSE_GREEN, 99, 14);
    button(pages[10], "Escanear", 252, 4, 126, 44, spectrum_start, 0);
    channels_text = label(pages[10], "Toca Escanear para comparar\nlos canales 1, 6 y 11.",
                          &lv_font_montserrat_16, PAPER, 36, 73);
    lv_obj_set_width(channels_text, 338);
    label(pages[11], "Auditoria", &lv_font_montserrat_24, ECLIPSE_GREEN, 99, 14);
    button(pages[11], "Escanear", 252, 4, 126, 44, spectrum_start, 0);
    security_text = label(pages[11], "Toca Escanear para revisar\nla seguridad anunciada por Wi-Fi.",
                          &lv_font_montserrat_16, PAPER, 36, 73);
    lv_obj_set_width(security_text, 338);

    label(pages[12], "IPv4 / CIDR", &lv_font_montserrat_24, ECLIPSE_GREEN, 107, 14);
    lv_obj_t *cidr_input = box(pages[12], 28, 68, 354, 58, INK, 18);
    lv_obj_add_flag(cidr_input, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(cidr_input, lv_color_hex(0x282e32), LV_STATE_PRESSED);
    cidr_input_label = label(cidr_input, cidr_input_text, &lv_font_montserrat_20, PAPER, 14, 17);
    lv_obj_set_width(cidr_input_label, 294);
    label(cidr_input, LV_SYMBOL_EDIT, &lv_font_montserrat_18, ECLIPSE_GREEN, 318, 19);
    lv_obj_add_event_cb(cidr_input, cidr_edit, LV_EVENT_CLICKED, NULL);
    label(pages[12], "Toca la direccion para editar IPv4/prefijo",
          &lv_font_montserrat_14, MUTED, 36, 137);
    cidr_text = label(pages[12], "", &lv_font_montserrat_16, PAPER, 36, 166);
    lv_obj_set_width(cidr_text, 338);
    static const char *const cidr_keys[] = {
        "1", "2", "3", LV_SYMBOL_BACKSPACE, "\n",
        "4", "5", "6", "Borrar", "\n",
        "7", "8", "9", "/", "\n",
        ".", "0", "Aplicar", LV_SYMBOL_CLOSE, ""
    };
    cidr_keyboard = numeric_keyboard(pages[12], cidr_keys, 166, 212, cidr_key);
    lv_obj_set_style_pad_row(cidr_keyboard, 8, 0);

    static const char *const engineer_keys[] = {
        "1", "2", "3", LV_SYMBOL_BACKSPACE, "\n",
        "4", "5", "6", "Borrar", "\n",
        "7", "8", "9", "-", "\n",
        ".", "0", "/", ",", "\n",
        "Aplicar", "Cancelar", ""
    };
    for (int i = 0; i < 2; ++i) {
        int page = i + 15;
        label(pages[page], i ? "VLSM" : "RF / enlace", &lv_font_montserrat_24,
              ECLIPSE_GREEN, 103, 14);
        lv_obj_t *input = box(pages[page], 28, 62, 354, 80, INK, 18);
        lv_obj_add_flag(input, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_color(input, lv_color_hex(0x282e32), LV_STATE_PRESSED);
        engineer_tools[i].input_label = label(input, engineer_tools[i].input,
                                             &lv_font_montserrat_16, PAPER, 14, 10);
        lv_obj_set_width(engineer_tools[i].input_label, 294);
        label(input, LV_SYMBOL_EDIT, &lv_font_montserrat_18, ECLIPSE_GREEN, 318, 30);
        lv_obj_add_event_cb(input, engineer_edit, LV_EVENT_CLICKED, (void *)(intptr_t)page);
        lv_obj_t *hint = label(pages[page], i ?
            "Red/prefijo, hosts separados por comas\nHasta 4 grupos de hosts" :
            "Frecuencia MHz, distancia m, potencia dBm\nModelo de espacio libre",
            &lv_font_montserrat_14, MUTED, 36, 152);
        lv_obj_set_width(hint, 338);
        engineer_tools[i].text = label(pages[page], "Toca la entrada para calcular.",
                                       &lv_font_montserrat_16, PAPER, 36, 194);
        lv_obj_set_width(engineer_tools[i].text, 338);
        lv_obj_set_style_text_line_space(engineer_tools[i].text, 3, 0);
        lv_obj_set_style_pad_bottom(pages[page], 24, 0);
        lv_obj_set_scrollbar_mode(pages[page], LV_SCROLLBAR_MODE_OFF);
    }
    engineer_keyboard = numeric_keyboard(pages[15], engineer_keys, 184, 224, engineer_key);

    button(pages[13], LV_SYMBOL_LEFT, 28, 4, 54, 46, menu_back, 0);
    center_label(pages[13], "Bateria", &lv_font_montserrat_24, PAPER, 14);
    battery_percent = center_label(pages[13], "--", &lv_font_montserrat_48, colors[aura_theme()], 91);
    battery_details = label(pages[13], "Leyendo estado...", &lv_font_montserrat_18, PAPER, 48, 170);
    lv_obj_set_width(battery_details, 314);
    button(pages[13], "Ahorrar bateria", 28, 324, 354, 54, battery_save, 0);
    center_label(pages[13], "Brillo 35% - descanso 15 s", &lv_font_montserrat_14, MUTED, 390);

    lv_obj_set_style_bg_color(pages[14], lv_color_hex(0xffffff), 0);
    button(pages[14], LV_SYMBOL_LEFT, 28, 4, 54, 46, menu_back, 0);
    center_label(pages[14], "Linterna", &lv_font_montserrat_28, INK, 79);
    flashlight_name = center_label(pages[14], "Blanca", &lv_font_montserrat_20, INK, 122);
    center_label(pages[14], "Se apaga tras 30 segundos", &lv_font_montserrat_16, INK, 164);
    button(pages[14], "Blanca", 28, 258, 110, 54, flashlight_select, 0);
    button(pages[14], "Calida", 150, 258, 110, 54, flashlight_select, 1);
    button(pages[14], "Roja", 272, 258, 110, 54, flashlight_select, 2);
    button(pages[14], "Volver", 28, 334, 354, 54, menu_back, 0);

    evidence_results = xQueueCreate(1, sizeof(evidence_result_t));
    configASSERT(evidence_results);
    lv_timer_create(tools_tick, 1000, NULL);

    notice = center_label(screen, "", &lv_font_montserrat_16, PAPER, 455);
    lv_obj_set_style_bg_color(notice, lv_color_hex(INK), 0);
    lv_obj_set_style_bg_opa(notice, LV_OPA_COVER, 0);
    lv_obj_add_flag(notice, LV_OBJ_FLAG_HIDDEN);
    power_overlay = box(screen, 0, 0, 410, 502, 0x000000, 0);
    lv_obj_add_flag(power_overlay, LV_OBJ_FLAG_CLICKABLE);
    center_label(power_overlay, "PWR mantenido", &lv_font_montserrat_24, PAPER, 120);
    center_label(power_overlay, "Si sigues, Aura se apagara en", &lv_font_montserrat_16, MUTED, 166);
    power_countdown = center_label(power_overlay, "5", &lv_font_montserrat_48, colors[aura_theme()], 205);
    power_hint = center_label(power_overlay, "Suelta para apagar la pantalla", &lv_font_montserrat_16, PAPER, 294);
    center_label(power_overlay, "El reloj y la hora quedaran guardados", &lv_font_montserrat_14, MUTED, 333);
    lv_obj_add_flag(power_overlay, LV_OBJ_FLAG_HIDDEN);
    last_activity = now_us();
    next_blink = last_activity + 2800000;
    next_gaze = last_activity + 1800000;
    apply_theme();
    aura_ui_page(0);
    bsp_display_brightness_set(aura_brightness());
    tick(NULL);
    battery_tick(NULL);
    animation_timer = lv_timer_create(animate, 33, NULL);
    clock_timer = lv_timer_create(tick, 250, NULL);
    battery_timer = lv_timer_create(battery_tick, 5000, NULL);
    lv_timer_create(wifi_tick, 1000, NULL);
    stopwatch_timer = lv_timer_create(stopwatch_tick, 1000, NULL);
    wifi_tick(NULL);
    lv_async_call(menu_tiles_cache_async, NULL);
}

static void refresh_labels(lv_obj_t *obj)
{
    if (lv_obj_check_type(obj, &lv_label_class)) set_text(obj, NULL);
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); ++i)
        refresh_labels(lv_obj_get_child(obj, i));
}

void aura_ui_dump_screen(void)
{
    if (sleeping) { printf("AURA_ERROR screen_asleep\n"); return; }
    refresh_labels(lv_screen_active());
    lv_obj_update_layout(lv_screen_active());
    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(NULL);
    lv_draw_buf_t *shot = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_RGB565);
    if (!shot) { printf("AURA_ERROR snapshot\n"); return; }
    char header[96];
    size_t bytes = shot->header.stride * shot->header.h;
    int length = snprintf(header, sizeof(header), "AURA_FRAME %u %u %u %u\n",
                          (unsigned)shot->header.w, (unsigned)shot->header.h,
                          (unsigned)shot->header.stride, (unsigned)bytes);
    fflush(stdout);
    usb_serial_jtag_write_bytes(header, length, pdMS_TO_TICKS(1000));
    for (size_t offset = 0; offset < bytes;) {
        size_t chunk = bytes - offset > 2048 ? 2048 : bytes - offset;
        int written = usb_serial_jtag_write_bytes(shot->data + offset, chunk, pdMS_TO_TICKS(1000));
        if (written <= 0) break;
        offset += written;
    }
    usb_serial_jtag_wait_tx_done(pdMS_TO_TICKS(1000));
    lv_draw_buf_destroy(shot);
}
