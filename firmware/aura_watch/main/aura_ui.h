#pragma once

#include <stdbool.h>

int aura_ui_current_page(void);
bool aura_ui_eclipse_active(void);
unsigned aura_ui_menu_cache_count(void);
unsigned aura_ui_menu_cache_stack_free(void);

void aura_ui_init(void);
// Call from another task only while holding bsp_display_lock().
void aura_ui_page(int page);
void aura_ui_timer_start(int seconds);
void aura_ui_sleep(int sleeping);
void aura_ui_power_short(void);
void aura_ui_power_long(void);
void aura_ui_power_release(void);
void aura_ui_boot_long(void);
// Seven taps on the menu version within five seconds open Eclipse directly.
void aura_ui_version_tap(void);
void aura_ui_eclipse_close(void);
void aura_ui_spectrum_start(void);
void aura_ui_evidence_start(void);
bool aura_ui_evidence_busy(void);
// Sets and calculates IPv4/prefix using the same path as the watch keypad.
void aura_ui_network_tool_input(const char *cidr);
// RF page 15 or VLSM page 16, with Eclipse active, that page visible and screen awake.
// Uses the same calculation and input path as the watch keypad (80 chars max).
void aura_ui_engineer_input(int page, const char *text);
// Diagnostic editor event queued to LVGL: -1 open, 0..17 keypad map index.
// Requires the visible awake RF/VLSM page; discards actions after changing page.
void aura_ui_engineer_editor_action(int key);
// Positive pixels scroll down in the visible Aura (3) or Eclipse (6) menu.
// Animation uses LVGL's native scroll engine.
void aura_ui_menu_scroll(int pixels);
// Read Eclipse when page 6 is visible, otherwise Aura. Hold bsp_display_lock().
int aura_ui_menu_scroll_y(void);
bool aura_ui_is_sleeping(void);
// Diagnostic LVGL tap: target 0 background, 1 slider, 2 version, 3 Aura face.
// Simulates events; does not verify the physical touch controller.
void aura_ui_diagnostic_tap(int x, int y, int target_kind);
void aura_ui_diagnostic_drag(int x, int y, int end_x, int end_y);
void aura_ui_motion(float x, float y);
void aura_ui_dizzy(void);
void aura_ui_stopwatch_action(int action);
void aura_ui_dump_screen(void);
