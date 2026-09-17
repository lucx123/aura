#pragma once

#include <stdbool.h>

void aura_ui_init(void);
// Call from another task only while holding bsp_display_lock().
void aura_ui_page(int page);
void aura_ui_timer_start(int seconds);
void aura_ui_sleep(int sleeping);
void aura_ui_power_short(void);
void aura_ui_power_long(void);
void aura_ui_power_release(void);
bool aura_ui_is_sleeping(void);
void aura_ui_motion(float x, float y);
void aura_ui_dizzy(void);
void aura_ui_dump_screen(void);
