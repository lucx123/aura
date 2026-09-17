#include "aura_ui.h"
#include "aura_services.h"
#include "aura_wifi.h"

#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include "esp_err.h"
#include "bsp/display.h"
#include "bsp/esp-bsp.h"
#include "driver/usb_serial_jtag.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "lvgl.h"

#define INK 0x101918
#define PAPER 0xf0f7f4
#define MUTED 0x82938e
#define UI_WIDTH 410
#define PAGE_TOP 62
#define PAGE_HEIGHT 420
#define PI 3.14159265358979323846f

static const uint32_t colors[] = {0x8af2dd, 0xb6a2ff, 0xffd495};
static const char *weekdays[] = {"Dom", "Lun", "Mar", "Mie", "Jue", "Vie", "Sab"};
static const char *months[] = {"ene", "feb", "mar", "abr", "may", "jun", "jul", "ago", "sep", "oct", "nov", "dic"};
static lv_obj_t *pages[5], *battery_label, *local_label, *network_label, *wifi_icon_label;
static lv_obj_t *eyes[2], *pupils[2], *shine[2], *mouth, *face, *mood_label, *dizzy_stars[4];
static lv_obj_t *home_time, *home_date, *clock_time, *clock_date;
static lv_obj_t *hour_hand, *minute_hand, *second_hand, *clock_dot;
static lv_point_precise_t hour_points[2], minute_points[2], second_points[2];
static lv_obj_t *timer_text, *timer_status, *timer_action, *timer_fill;
static lv_obj_t *brightness_label, *brightness_slider, *theme_buttons[3], *format_value, *theme_value;
static lv_obj_t *wifi_state_label, *wifi_ssid_label, *wifi_action_label, *wifi_sync_label;
static lv_obj_t *notice, *power_overlay, *power_countdown, *power_hint;
static int current_page, mood, selected_seconds = 300, remaining_seconds = 300;
static int timer_state; // 0 ready, 1 running, 2 paused, 3 finished
static int64_t deadline, mood_until, next_gaze, next_blink, blink_until, motion_until;
static int64_t last_activity, notice_until;
static int gaze_x, gaze_y, target_x, target_y;
static bool sleeping;
static bool power_hold_active, power_warning_visible, power_was_sleeping;
static int64_t power_hold_started;
static int64_t sleep_tap_started, touch_wake_guard_until;
static int sleep_tap_x, sleep_tap_y;
static bool touching;
static bool holding_face;
static int reaction;
static int64_t now_us(void) { return esp_timer_get_time(); }
static lv_color_t accent(void) { return lv_color_hex(colors[aura_theme()]); }

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
    lv_label_set_text(obj, text);
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

static void show_notice(const char *text)
{
    lv_label_set_text(notice, text);
    lv_obj_remove_flag(notice, LV_OBJ_FLAG_HIDDEN);
    notice_until = now_us() + 2200000;
}

void aura_ui_power_short(void)
{
    if (power_hold_active) return;
    if (sleeping) {
        aura_ui_sleep(0);
        aura_ui_page(0);
        return;
    }
    aura_ui_page(current_page == 3 ? 0 : 3);
}

void aura_ui_power_long(void)
{
    if (power_hold_active) return;
    power_hold_active = true;
    power_warning_visible = false;
    power_was_sleeping = sleeping;
    power_hold_started = now_us();
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
    if (page < 0 || page > 4) return;
    current_page = page;
    touching = false;
    for (int i = 0; i < 5; ++i) {
        if (i == page) lv_obj_remove_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
    }
    last_activity = now_us();
}

void aura_ui_sleep(int sleep)
{
    sleeping = sleep != 0;
    touching = false;
    if (sleeping) {
        sleep_tap_started = 0;
        aura_ui_page(0);
    }
    bsp_display_brightness_set(sleeping ? 0 : aura_brightness());
    last_activity = now_us();
    lv_label_set_text(mood_label, sleeping ? "Descansando" : "Estoy contigo");
}

bool aura_ui_is_sleeping(void)
{
    return sleeping;
}

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
    if (sleeping) return;
    aura_ui_page(0);
    mood = 5;
    mood_until = now_us() + 4800000;
    motion_until = mood_until;
    lv_label_set_text(mood_label, "Estoy viendo estrellitas...");
}

static bool wake_only(void)
{
    int64_t now = now_us();
    if (sleeping || now < touch_wake_guard_until) return true;
    last_activity = now;
    return false;
}

static void root_event(lv_event_t *e)
{
    (void)e;
    if (sleeping) {
        lv_indev_t *input = lv_indev_active();
        if (!input) return;
        lv_point_t point;
        lv_indev_get_point(input, &point);
        int64_t now = now_us();
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
    } else last_activity = now_us();
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
        lv_label_set_text(mood_label, messages[mood]);
    } else if (code == LV_EVENT_LONG_PRESSED) {
        if (wake_only()) return;
        holding_face = true;
        mood = 4;
        mood_until = now_us() + 2600000;
        lv_label_set_text(mood_label, "Mmm... sigue");
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
            lv_label_set_text(power_hint, power_was_sleeping ?
                              "Suelta para encender la pantalla" :
                              "Suelta para apagar la pantalla");
            lv_obj_remove_flag(power_overlay, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(power_overlay);
        }
        if (power_warning_visible) {
            int remaining = 7 - elapsed;
            char count[12];
            snprintf(count, sizeof(count), "%d", remaining > 0 ? remaining : 0);
            lv_label_set_text(power_countdown, count);
            if (remaining <= 0) {
                power_hold_active = false;
                power_warning_visible = false;
                lv_label_set_text(power_countdown, "...");
                aura_power_off();
            }
        }
    }
    if (notice_until && now > notice_until) {
        lv_obj_add_flag(notice, LV_OBJ_FLAG_HIDDEN);
        notice_until = 0;
    }
    if (!sleeping && now - last_activity > 45000000) aura_ui_sleep(1);
    if (current_page != 0) return;
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
        lv_label_set_text(mood_label, sleeping ? "Descansando" : "Estoy contigo");
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
        int height = 112, width = 86;
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

static void tick(lv_timer_t *t)
{
    (void)t;
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
        set_hand(hour_hand, hour_points, (local.tm_hour % 12 + local.tm_min / 60.f) * PI / 6, 69);
        set_hand(minute_hand, minute_points, (local.tm_min + local.tm_sec / 60.f) * PI / 30, 99);
        set_hand(second_hand, second_points, local.tm_sec * PI / 30, 106);
    }
    lv_label_set_text(home_time, hhmm);
    lv_label_set_text(clock_time, hhmm);
    lv_label_set_text(home_date, date);
    lv_label_set_text(clock_date, date);
    if (timer_state == 1) {
        int64_t left = deadline - now_us();
        remaining_seconds = left > 0 ? (int)((left + 999999) / 1000000) : 0;
        if (!remaining_seconds) {
            timer_state = 3;
            aura_ui_sleep(0);
            aura_ui_page(2);
        }
    }
    char count[16];
    snprintf(count, sizeof(count), "%02d:%02d", remaining_seconds / 60, remaining_seconds % 60);
    lv_label_set_text(timer_text, count);
    const char *states[] = {"Elige tu pausa", "Tu momento esta en marcha", "En pausa", "Tiempo cumplido"};
    const char *actions[] = {"Empezar", "Pausar", "Continuar", "Listo"};
    lv_label_set_text(timer_status, states[timer_state]);
    lv_label_set_text(timer_action, actions[timer_state]);
    lv_obj_set_width(timer_fill, selected_seconds ? 314 * remaining_seconds / selected_seconds : 0);
    lv_obj_set_style_text_color(timer_text,
        timer_state == 3 && ((now_us() / 1000000) % 2) ? lv_color_hex(PAPER) : accent(), 0);
    aura_wifi_status_t wifi;
    aura_wifi_get_status(&wifi);
    const char *presence = wifi.state == AURA_WIFI_CONNECTING ? "Conectando" :
                           wifi.state == AURA_WIFI_ONLINE ? "En linea" :
                           wifi.state == AURA_WIFI_SETUP ? "En portal" :
                           wifi.state == AURA_WIFI_SYNCED ? "Hora lista" :
                           sleeping ? "Descanso" : timer_state == 1 ? "En pausa" : "Tranquila";
    lv_label_set_text(local_label, presence);
}

static void battery_tick(lv_timer_t *timer)
{
    (void)timer;
    aura_battery_t battery;
    aura_battery_read(&battery);
    char text[40];
    if (!battery.available) snprintf(text, sizeof(text), "Bateria --");
    else if (battery.percent >= 0) snprintf(text, sizeof(text), "%s%d%%", battery.charging ? LV_SYMBOL_CHARGE " " : "", battery.percent);
    else snprintf(text, sizeof(text), "%s", battery.usb ? "USB" : "Bateria --");
    lv_label_set_text(battery_label, text);
    lv_obj_set_style_text_color(battery_label,
        battery.percent >= 0 && battery.percent < 15 ? lv_color_hex(0xff9b85) : lv_color_hex(PAPER), 0);
}

static void brightness_event(lv_event_t *e)
{
    last_activity = now_us();
    int value = lv_slider_get_value(brightness_slider);
    char text[32];
    snprintf(text, sizeof(text), "Brillo  %d%%", value);
    lv_label_set_text(brightness_label, text);
    bsp_display_brightness_set(value);
    if (lv_event_get_code(e) == LV_EVENT_RELEASED) aura_save_brightness(value);
}

static void apply_theme(void)
{
    for (int i = 0; i < 3; ++i) {
        lv_obj_set_style_border_color(theme_buttons[i], lv_color_hex(colors[i]), 0);
        lv_obj_set_style_border_width(theme_buttons[i], i == aura_theme() ? 3 : 1, 0);
    }
    lv_obj_set_style_line_color(minute_hand, accent(), 0);
    lv_obj_set_style_bg_color(clock_dot, accent(), 0);
    lv_obj_set_style_bg_color(timer_fill, accent(), 0);
    if (power_countdown) lv_obj_set_style_text_color(power_countdown, accent(), 0);
    if (wifi_icon_label) lv_obj_set_style_text_color(wifi_icon_label, accent(), 0);
    lv_obj_set_style_bg_color(brightness_slider, accent(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(brightness_slider, accent(), LV_PART_KNOB);
    aura_ui_page(current_page);
}

static void format_click(lv_event_t *e)
{
    (void)e;
    if (wake_only()) return;
    aura_save_clock_24h(!aura_clock_24h());
    lv_label_set_text(format_value, aura_clock_24h() ? "24 horas" : "12 horas");
    show_notice(aura_clock_24h() ? "Reloj en 24 horas" : "Reloj en 12 horas");
    tick(NULL);
}

static void sleep_click(lv_event_t *e) { (void)e; aura_ui_sleep(1); }

static lv_obj_t *menu_row(lv_obj_t *parent, const char *glyph, uint32_t icon_color,
                          const char *title, const char *subtitle, int y,
                          lv_event_cb_t cb, intptr_t value, lv_obj_t **value_label)
{
    lv_obj_t *row = box(parent, 28, y, 354, 72, INK, 22);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(row, lv_color_hex(0x263431), LV_STATE_PRESSED);
    lv_obj_t *orb = box(row, 14, 12, 48, 48, icon_color, LV_RADIUS_CIRCLE);
    lv_obj_t *icon = label(orb, glyph, &lv_font_montserrat_16, PAPER, 0, 0);
    lv_obj_center(icon);
    label(row, title, &lv_font_montserrat_18, PAPER, 77, subtitle && subtitle[0] ? 12 : 24);
    lv_obj_t *sub = label(row, subtitle ? subtitle : "", &lv_font_montserrat_14, MUTED, 77, 39);
    lv_obj_set_width(sub, 220);
    lv_obj_t *arrow = label(row, LV_SYMBOL_RIGHT, &lv_font_montserrat_16, MUTED, 320, 27);
    (void)arrow;
    if (cb) lv_obj_add_event_cb(row, cb, LV_EVENT_CLICKED, (void *)value);
    if (value_label) *value_label = sub;
    return row;
}

static void wifi_open(lv_event_t *e) { (void)e; if (!wake_only()) aura_ui_page(4); }
static void timer_open(lv_event_t *e) { (void)e; if (!wake_only()) aura_ui_page(2); }
static void clock_open(lv_event_t *e) { (void)e; if (!wake_only()) aura_ui_page(1); }

static void theme_cycle(lv_event_t *e)
{
    (void)e;
    if (wake_only()) return;
    int next = (aura_theme() + 1) % 3;
    aura_save_theme(next);
    const char *names[] = {"Menta", "Lila", "Sol"};
    lv_label_set_text(theme_value, names[next]);
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
    aura_wifi_status_t wifi;
    aura_wifi_get_status(&wifi);
    bool connected = wifi.state == AURA_WIFI_ONLINE || wifi.state == AURA_WIFI_SYNCED;
    if (connected && wifi.ssid[0]) {
        lv_obj_remove_flag(wifi_icon_label, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(network_label, wifi.ssid);
        lv_obj_set_pos(network_label, 73, 29);
        lv_obj_set_width(network_label, 80);
        lv_obj_set_style_text_font(network_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(network_label, lv_color_hex(MUTED), 0);
    } else {
        lv_obj_add_flag(wifi_icon_label, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(network_label, "Aura");
        lv_obj_set_pos(network_label, 52, 24);
        lv_obj_set_width(network_label, 105);
        lv_obj_set_style_text_font(network_label, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(network_label, lv_color_hex(PAPER), 0);
    }
    const char *states[] = {"Wi-Fi en reposo", "Buscando tu red", "Conectada", "Hora sincronizada",
                            "Lista para configurar", "No pude conectar"};
    lv_label_set_text(wifi_state_label, states[wifi.state]);
    char network[64];
    if (wifi.state == AURA_WIFI_SETUP) snprintf(network, sizeof(network), "AURA-SETUP  -  abre 192.168.4.1");
    else snprintf(network, sizeof(network), "%s%s", wifi.configured ? wifi.ssid : "Sin red guardada",
                  wifi.state == AURA_WIFI_ONLINE && wifi.rssi ? "  · en linea" : "");
    lv_label_set_text(wifi_ssid_label, network);
    lv_label_set_text(wifi_action_label, wifi.configured ? "Sincronizar ahora" : "Configurar Wi-Fi");
    char last[64] = "La hora aun no se ha sincronizado";
    if (wifi.last_sync > 0) {
        time_t local_epoch = (time_t)wifi.last_sync + aura_clock_offset();
        struct tm local;
        gmtime_r(&local_epoch, &local);
        snprintf(last, sizeof(last), "Ultima vez: %02d:%02d - %d %s", local.tm_hour, local.tm_min,
                 local.tm_mday, months[local.tm_mon]);
    }
    lv_label_set_text(wifi_sync_label, last);
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
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(screen, root_event, LV_EVENT_RELEASED, NULL);
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
    for (int i = 0; i < 5; ++i) {
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

    lv_obj_add_flag(pages[3], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(pages[3], LV_DIR_VER);
    lv_obj_set_scrollbar_mode(pages[3], LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_bottom(pages[3], 24, 0);
    center_label(pages[3], "Centro Aura", &lv_font_montserrat_24, PAPER, 3);
    menu_row(pages[3], "Wi", 0x176354, "Wi-Fi", "Internet en rafagas", 45, wifi_open, 0, NULL);
    menu_row(pages[3], "5m", 0x715b28, "Temporizador", "Pausas de 5, 15 o 25 min", 125, timer_open, 0, NULL);
    menu_row(pages[3], "An", 0x33495a, "Reloj analogico", "Una esfera mas clasica", 205, clock_open, 0, NULL);
    menu_row(pages[3], "12", 0x50417d, "Formato de hora", aura_clock_24h() ? "24 horas" : "12 horas",
             285, format_click, 0, &format_value);
    menu_row(pages[3], "Aa", 0x3e5270, "Apariencia", aura_theme() == 0 ? "Menta" : aura_theme() == 1 ? "Lila" : "Sol",
             365, theme_cycle, 0, &theme_value);
    char text[32];
    snprintf(text, sizeof(text), "Brillo  %d%%", aura_brightness());
    lv_obj_t *brightness_card = box(pages[3], 28, 445, 354, 88, INK, 22);
    brightness_label = label(brightness_card, text, &lv_font_montserrat_18, PAPER, 20, 13);
    brightness_slider = lv_slider_create(pages[3]);
    lv_obj_set_parent(brightness_slider, brightness_card);
    lv_obj_set_pos(brightness_slider, 22, 57);
    lv_obj_set_size(brightness_slider, 310, 8);
    lv_slider_set_range(brightness_slider, 15, 100);
    lv_slider_set_value(brightness_slider, aura_brightness(), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(INK), LV_PART_MAIN);
    lv_obj_add_flag(brightness_slider, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_event_cb(brightness_slider, brightness_event, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(brightness_slider, brightness_event, LV_EVENT_RELEASED, NULL);
    for (int i = 0; i < 3; ++i) {
        theme_buttons[i] = box(brightness_card, 244 + i * 25, 15, 16, 16, colors[i], LV_RADIUS_CIRCLE);
    }
    menu_row(pages[3], "Zz", 0x493238, "Apagar pantalla", "Tambien puedes mantener PWR", 541, sleep_click, 0, NULL);
    lv_obj_t *version = center_label(pages[3], "AURA Watch - Basic 1.2 dev.3", &lv_font_montserrat_14, MUTED, 626);
    lv_obj_set_height(pages[3], PAGE_HEIGHT);
    (void)version;

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
    bsp_display_brightness_set(aura_brightness());
    tick(NULL);
    battery_tick(NULL);
    lv_timer_create(animate, 50, NULL);
    lv_timer_create(tick, 250, NULL);
    lv_timer_create(battery_tick, 5000, NULL);
    lv_timer_create(wifi_tick, 1000, NULL);
    wifi_tick(NULL);
}

static void refresh_labels(lv_obj_t *obj)
{
    if (lv_obj_check_type(obj, &lv_label_class)) lv_label_set_text(obj, NULL);
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); ++i)
        refresh_labels(lv_obj_get_child(obj, i));
}

void aura_ui_dump_screen(void)
{
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
