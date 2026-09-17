#include "aura_motion.h"
#include "aura_ui.h"

#include <math.h>
#include <string.h>
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#undef M_PI
#include "qmi8658.h"

static const char *TAG = "aura_motion";
static qmi8658_dev_t sensor;
static aura_motion_status_t state;
static portMUX_TYPE state_lock = portMUX_INITIALIZER_UNLOCKED;

static void publish(float x, float y, float z, float movement, float rotation, bool shook)
{
    portENTER_CRITICAL(&state_lock);
    state.available = true;
    state.x = x;
    state.y = y;
    state.z = z;
    state.movement = movement;
    state.rotation = rotation;
    if (shook) state.shake_count++;
    portEXIT_CRITICAL(&state_lock);
}

static void motion_task(void *unused)
{
    (void)unused;
    bool low_power = false;
    float filtered_x = 0, filtered_y = 0, filtered_z = 9.807f;
    float previous_x = 0, previous_y = 0, previous_z = 9.807f;
    int shake_hits = 0;
    int64_t shake_window = 0, cooldown_until = 0;

    for (;;) {
        bool sleeping = aura_ui_is_sleeping();
        if (sleeping != low_power) {
            qmi8658_set_accel_odr(&sensor, sleeping ?
                QMI8658_ACCEL_ODR_LOWPOWER_21HZ : QMI8658_ACCEL_ODR_62_5HZ);
            qmi8658_enable_sensors(&sensor, sleeping ? QMI8658_ENABLE_ACCEL :
                                   QMI8658_ENABLE_ACCEL | QMI8658_ENABLE_GYRO);
            low_power = sleeping;
        }

        float x, y, z;
        if (qmi8658_read_accel(&sensor, &x, &y, &z) == ESP_OK) {
            float gx = 0, gy = 0, gz = 0;
            if (!sleeping) qmi8658_read_gyro(&sensor, &gx, &gy, &gz);
            float dx = x - previous_x, dy = y - previous_y, dz = z - previous_z;
            float movement = sqrtf(dx * dx + dy * dy + dz * dz);
            float rotation = sqrtf(gx * gx + gy * gy + gz * gz);
            previous_x = x; previous_y = y; previous_z = z;
            filtered_x += (x - filtered_x) * 0.18f;
            filtered_y += (y - filtered_y) * 0.18f;
            filtered_z += (z - filtered_z) * 0.18f;

            int64_t now = esp_timer_get_time();
            bool shook = false;
            if (!sleeping && now >= cooldown_until && movement > 18.0f && rotation > 260.0f) {
                if (!shake_window || now - shake_window > 440000) {
                    shake_window = now;
                    shake_hits = 1;
                } else if (++shake_hits >= 4) {
                    shook = true;
                    shake_hits = 0;
                    shake_window = 0;
                    cooldown_until = now + 6500000;
                }
            }
            publish(filtered_x, filtered_y, filtered_z, movement, rotation, shook);

            if (!sleeping && bsp_display_lock(0)) {
                if (shook) aura_ui_dizzy();
                else aura_ui_motion(filtered_x, filtered_y);
                bsp_display_unlock();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(sleeping ? 450 : 40));
    }
}

void aura_motion_init(void)
{
    memset(&sensor, 0, sizeof(sensor));
    esp_err_t error = qmi8658_init(&sensor, bsp_i2c_get_handle(), QMI8658_ADDRESS_HIGH);
    if (error != ESP_OK) {
        ESP_LOGW(TAG, "QMI8658 unavailable: %s", esp_err_to_name(error));
        return;
    }
    qmi8658_set_accel_range(&sensor, QMI8658_ACCEL_RANGE_8G);
    qmi8658_set_accel_odr(&sensor, QMI8658_ACCEL_ODR_62_5HZ);
    qmi8658_set_gyro_range(&sensor, QMI8658_GYRO_RANGE_512DPS);
    qmi8658_set_gyro_odr(&sensor, QMI8658_GYRO_ODR_62_5HZ);
    qmi8658_set_accel_unit_mps2(&sensor, true);
    qmi8658_set_gyro_unit_dps(&sensor, true);
    qmi8658_enable_sensors(&sensor, QMI8658_ENABLE_ACCEL | QMI8658_ENABLE_GYRO);
    portENTER_CRITICAL(&state_lock);
    state.available = true;
    portEXIT_CRITICAL(&state_lock);
    xTaskCreate(motion_task, "aura_motion", 4096, NULL, 3, NULL);
    ESP_LOGI(TAG, "Acelerometro activo en modo austero");
}

void aura_motion_get_status(aura_motion_status_t *status)
{
    if (!status) return;
    portENTER_CRITICAL(&state_lock);
    *status = state;
    portEXIT_CRITICAL(&state_lock);
}
