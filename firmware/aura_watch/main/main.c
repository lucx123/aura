#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include "esp_err.h"
#include "bsp/esp-bsp.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_pm.h"
#include "esp_app_desc.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_private/esp_clk.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "aura_services.h"
#include "aura_wifi.h"
#include "aura_ui.h"
#include "aura_motion.h"
#include "aura_eclipse.h"
/* Diagnostics use the driver's TX path. The console VFS can discard a write
 * when its USB connection monitor briefly disagrees with RX after resume. */
static void reply(const char *format, ...) __attribute__((format(printf, 1, 2)));
static void reply(const char *format, ...)
{
    char response[768];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(response, sizeof(response), format, args);
    va_end(args);
    if (length <= 0) return;
    if (length >= (int)sizeof(response)) length = sizeof(response) - 1;
    size_t sent = 0;
    while (sent < (size_t)length) {
        int written = usb_serial_jtag_write_bytes(response + sent, length - sent, pdMS_TO_TICKS(100));
        if (written <= 0) break;
        sent += written;
    }
    usb_serial_jtag_wait_tx_done(pdMS_TO_TICKS(100));
}
static void command(char *line) {
    long long epoch; int value;
    if (sscanf(line, "TIME %lld %d", &epoch, &value)==2) {
        reply("AURA_TIME %s\n", esp_err_to_name(aura_clock_set((time_t)epoch,value)));
    } else if (!strcmp(line,"STATUS")) {
        aura_wifi_status_t wifi; aura_wifi_get_status(&wifi);
        aura_motion_status_t motion; aura_motion_get_status(&motion);
        aura_eclipse_storage_t storage; aura_eclipse_storage_status(&storage);
        if (!bsp_display_lock(1500)) { reply("AURA_ERROR display_lock_timeout\n"); fflush(stdout); return; }
        bool eclipse = aura_ui_eclipse_active();
        bsp_display_unlock();
        reply("AURA_STATUS epoch=%lld valid=%d rtc=%d offset=%d brightness=%d theme=%d clock=%s wifi=%d configured=%d ssid=%s reason=%u last_sync=%lld imu=%d xyz=%.2f,%.2f,%.2f movement=%.2f rotation=%.1f shakes=%u eclipse=%d sd=%d sd_error=%s sd_bytes=%llu radio=%d idle_seconds=%d\n",(long long)time(NULL),aura_clock_valid(),aura_rtc_available(),aura_clock_offset(),aura_brightness(),aura_theme(),aura_clock_24h()?"24h":"12h",wifi.state,wifi.configured,wifi.configured?wifi.ssid:"-",wifi.disconnect_reason,(long long)wifi.last_sync,motion.available,motion.x,motion.y,motion.z,motion.movement,motion.rotation,motion.shake_count,eclipse,storage.mounted,esp_err_to_name(storage.last_error),(unsigned long long)storage.capacity_bytes,wifi.radio_on,aura_idle_timeout());
    } else if (!strcmp(line,"STORAGE")) {
        aura_eclipse_storage_t storage; aura_eclipse_storage_status(&storage);
        aura_evidence_status_t evidence; aura_eclipse_evidence_status(&evidence);
        reply("AURA_STORAGE mounted=%d error=%s probe_error=%s format=%s bytes=%llu lba=%lu volume_sectors=%llu exfat_version=%u sector_shift=%u fats=%u records=%lu verified=%d internal_records=%lu internal_count=%u internal_verified=%d internal_error=%s\n",
              storage.mounted, esp_err_to_name(storage.last_error), esp_err_to_name(storage.probe_error),
              aura_storage_format_name(storage.detected_format), (unsigned long long)storage.capacity_bytes,
              (unsigned long)storage.volume_lba, (unsigned long long)storage.volume_sectors,
              storage.exfat_version, storage.exfat_sector_shift, storage.exfat_fat_count,
              (unsigned long)storage.records_written, storage.last_record_verified,
              (unsigned long)evidence.sequence, evidence.count, evidence.verified, esp_err_to_name(evidence.error));
    } else if (!strcmp(line,"EVIDENCE_EXPORT")) {
        if (!bsp_display_lock(1500)) { reply("AURA_ERROR display_lock_timeout\n"); return; }
        bool allowed = aura_ui_eclipse_active() && !aura_ui_is_sleeping() && !aura_ui_evidence_busy();
        bsp_display_unlock();
        if (!allowed) { reply("AURA_EVIDENCE_END error=ESP_ERR_INVALID_STATE count=0\n"); return; }
        char (*records)[AURA_EVIDENCE_LINE_MAX] = malloc(AURA_EVIDENCE_CAPACITY * AURA_EVIDENCE_LINE_MAX);
        unsigned count = 0;
        esp_err_t error = records ? aura_eclipse_evidence_copy(records, &count) : ESP_ERR_NO_MEM;
        if (error == ESP_OK)
            for (unsigned i = 0; i < count; ++i) reply("AURA_EVIDENCE %s\n", records[i]);
        free(records);
        reply("AURA_EVIDENCE_END error=%s count=%u\n", esp_err_to_name(error), error == ESP_OK ? count : 0);
    } else if (!strcmp(line,"DRAW_STATS") || !strcmp(line,"DRAW_STATS_RESET")) {
        if (bsp_display_lock(1500)) {
            if (!strcmp(line,"DRAW_STATS_RESET")) aura_display_draw_stats_reset();
            aura_display_draw_stats_t stats; aura_display_draw_stats_get(&stats);
            reply("AURA_DRAW refreshes=%lu total_us=%llu max_us=%llu period_ms=%d\n",
                  (unsigned long)stats.refresh_count, (unsigned long long)stats.total_us,
                  (unsigned long long)stats.max_us, LV_DEF_REFR_PERIOD);
            bsp_display_unlock();
        } else reply("AURA_ERROR display_lock_timeout\n");
    } else if (!strcmp(line,"DISPLAY")) {
        if (bsp_display_lock(1500)) {
            reply("AURA_DISPLAY version=%s completed=%lu uptime_ms=%lld sleeping=%d page=%d eclipse=%d cpu_mhz=%d heap=%u min_heap=%u dma_free=%u reset=%d menu_y=%d evidence_busy=%d menu_cache=%u cache_stack_free=%u eclipse_intro=%u\n",
                esp_app_get_description()->version, (unsigned long)aura_display_completed_flushes(),
                (long long)(esp_timer_get_time() / 1000), aura_ui_is_sleeping(),
                aura_ui_current_page(), aura_ui_eclipse_active(), esp_clk_cpu_freq() / 1000000,
                (unsigned)esp_get_free_heap_size(), (unsigned)esp_get_minimum_free_heap_size(),
                (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DMA), esp_reset_reason(), aura_ui_menu_scroll_y(), aura_ui_evidence_busy(), aura_ui_menu_cache_count(), aura_ui_menu_cache_stack_free(), aura_ui_eclipse_intro_stage());
            bsp_display_unlock();
        } else reply("AURA_ERROR display_lock_timeout\n");
    } else if (!strcmp(line,"WIFI_SETUP")) {
        aura_wifi_start_setup(); reply("AURA_WIFI setup\n");
    } else if (!strcmp(line,"WIFI_SYNC")) {
        aura_wifi_request_sync(); reply("AURA_WIFI sync\n");
    } else if (!strcmp(line,"WIFI_FORGET")) {
        aura_wifi_forget(); reply("AURA_WIFI forgotten\n");
    } else {
        if (!bsp_display_lock(1500)) { reply("AURA_ERROR display_lock_timeout\n"); fflush(stdout); return; }
        if (!strcmp(line,"SCREEN")) aura_ui_dump_screen();
        else if (!strncmp(line,"TAP ",4)) {
            int x, y, target; char extra;
            if (sscanf(line+4,"%d %d %d %c",&x,&y,&target,&extra)==3 && x>=0 && x<410 && y>=0 && y<502 && target>=0 && target<=4)
                aura_ui_diagnostic_tap(x,y,target);
            else reply("AURA_ERROR TAP requires x y target (0..4)\n");
        }
        else if (!strncmp(line,"DRAG ",5)) {
            int x, y, end_x, end_y; char extra;
            if (sscanf(line+5,"%d %d %d %d %c",&x,&y,&end_x,&end_y,&extra)==4 && x>=0 && x<410 && y>=0 && y<502 && end_x>=0 && end_x<410 && end_y>=0 && end_y<502)
                aura_ui_diagnostic_drag(x,y,end_x,end_y);
            else reply("AURA_ERROR DRAG requires four screen coordinates\n");
        }
        else if (sscanf(line,"PAGE %d",&value)==1 && value>=0 && value<17) aura_ui_page(value);
        else if (!strncmp(line,"CIDR ",5)) aura_ui_network_tool_input(line+5);
        else if (!strncmp(line,"RF ",3)) aura_ui_engineer_input(15,line+3);
        else if (!strncmp(line,"VLSM ",5)) aura_ui_engineer_input(16,line+5);
        else if (sscanf(line,"ENGINEER_KEY %d",&value)==1 && value>=-1 && value<=17) aura_ui_engineer_editor_action(value);
        else if (!strcmp(line,"VERSION_TAP")) aura_ui_version_tap();
        else if (!strcmp(line,"ECLIPSE_CLOSE")) aura_ui_eclipse_close();
        else if (!strcmp(line,"EVIDENCE_START")) aura_ui_evidence_start();
        else if (!strcmp(line,"SPECTRUM_START")) aura_ui_spectrum_start();
        else if (sscanf(line,"MENU_SCROLL %d",&value)==1 && value>=-2000 && value<=2000) aura_ui_menu_scroll(value);
        else if (sscanf(line,"TIMER %d",&value)==1 && value>0 && value<=5999) aura_ui_timer_start(value);
        else if (!strcmp(line,"SLEEP")) aura_ui_sleep(1);
        else if (!strcmp(line,"WAKE")) aura_ui_sleep(0);
        else if (!strcmp(line,"PWR_SHORT")) aura_ui_power_short();
        else if (!strcmp(line,"PWR_LONG")) aura_ui_power_long();
        else if (!strcmp(line,"PWR_RELEASE")) aura_ui_power_release();
        else if (!strcmp(line,"DIZZY")) aura_ui_dizzy();
        else if (!strcmp(line,"BOOT_LONG")) aura_ui_boot_long();
        else if (!strcmp(line,"STOPWATCH_START")) { aura_ui_page(5); aura_ui_stopwatch_action(0); }
        else if (!strcmp(line,"STOPWATCH_LAP")) aura_ui_stopwatch_action(1);
        else if (!strcmp(line,"STOPWATCH_RESET")) aura_ui_stopwatch_action(-1);
        else reply("AURA_ERROR unknown command\n");
        bsp_display_unlock();
    }
    fflush(stdout);
}
static void boot_button_task(void *unused) {
    (void)unused;
    gpio_config_t config={
        .pin_bit_mask=1ULL<<GPIO_NUM_0,
        .mode=GPIO_MODE_INPUT,
        .pull_up_en=GPIO_PULLUP_ENABLE,
        .pull_down_en=GPIO_PULLDOWN_DISABLE,
        .intr_type=GPIO_INTR_DISABLE,
    };
    gpio_config(&config);
    bool pressed=false, fired=false;
    int64_t started=0;
    for (;;) {
        bool down=gpio_get_level(GPIO_NUM_0)==0;
        if(down && !pressed) { pressed=true; fired=false; started=esp_timer_get_time(); }
        if(down && pressed && !fired && esp_timer_get_time()-started>=2000000) {
            fired=true;
            bsp_display_lock(0); aura_ui_boot_long(); bsp_display_unlock();
        }
        if(!down) pressed=false;
        vTaskDelay(pdMS_TO_TICKS(35));
    }
}
static void serial_task(void *unused) {
    char line[128]; size_t length=0; bool overflow=false;
    for (;;) {
        char c;
        if(usb_serial_jtag_read_bytes(&c,1,pdMS_TO_TICKS(100))!=1) continue;
        if(c=='\n') {
            if(!overflow) { line[length]=0; command(line); }
            else reply("AURA_ERROR line too long\n");
            length=0; overflow=false;
        } else if(c!='\r') {
            if(length<sizeof(line)-1) line[length++]=c; else overflow=true;
        }
    }
}
static void power_button_task(void *unused) {
    (void)unused;
    bool long_active=false;
    // Ignore the release event generated by the same PWR hold that started the watch.
    vTaskDelay(pdMS_TO_TICKS(2000));
    aura_power_button_events();
    for (;;) {
        uint8_t events=aura_power_button_events();
        if(events) {
            bool held=long_active || (events & (1<<2));
            bsp_display_lock(0);
            if(events & (1<<2)) { long_active=true; aura_ui_power_long(); }
            if((events & (1<<0)) && long_active) { long_active=false; aura_ui_power_release(); }
            if((events & (1<<3)) && !held) aura_ui_power_short();
            bsp_display_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(80));
    }
}
void app_main(void) {
    const esp_pm_config_t power = {.max_freq_mhz = 240, .min_freq_mhz = 80, .light_sleep_enable = false};
    ESP_ERROR_CHECK(esp_pm_configure(&power));
    if(!bsp_display_start()) return;
    aura_services_init();
    aura_wifi_init();
    aura_eclipse_init();
    bsp_display_lock(0); aura_ui_init(); bsp_display_unlock();
    aura_motion_init();
    usb_serial_jtag_driver_config_t config={.tx_buffer_size=4096,.rx_buffer_size=256};
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&config));
    usb_serial_jtag_vfs_use_driver();
    configASSERT(xTaskCreate(serial_task,"aura_serial",8192,NULL,3,NULL) == pdPASS);
    configASSERT(xTaskCreate(power_button_task,"aura_pwr",4096,NULL,4,NULL) == pdPASS);
    configASSERT(xTaskCreate(boot_button_task,"aura_boot",3072,NULL,3,NULL) == pdPASS);
    reply("AURA_READY %s Eclipse\n", esp_app_get_description()->version);
}
