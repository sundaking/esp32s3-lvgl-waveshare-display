/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "waveshare_rgb_lcd_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Using TAG from waveshare_rgb_lcd_port.h

void app_main()
{
    ESP_LOGI(TAG, "Starting ESP32-S3 LVGL Porting Demo");

    // Initialize the display and backlight
    ESP_ERROR_CHECK(waveshare_esp32_s3_rgb_lcd_init());

    ESP_LOGI(TAG, "Display and backlight initialized successfully");

    // Test basic display functionality first (no LVGL)
    ESP_LOGI(TAG, "Testing basic display functionality...");

    // Keep the task alive to test basic display
    int counter = 0;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGI(TAG, "Basic display test running... counter: %d", counter++);
        if (counter > 10) {
            ESP_LOGI(TAG, "Basic display seems stable, would initialize LVGL now");
            break;
        }
    }

    // Now try LVGL initialization
    ESP_LOGI(TAG, "Initializing LVGL...");
    // Lock the mutex due to the LVGL APIs are not thread-safe
    if (lvgl_port_lock(-1)) {
        ESP_LOGI(TAG, "Running LVGL widgets demo");
        // lv_demo_stress();
        // lv_demo_benchmark();
        // lv_demo_music();
#if CONFIG_EXAMPLE_LCD_TOUCH_CONTROLLER_GT911
        lv_demo_widgets();  // Touch-enabled demo
#else
        lv_demo_widgets();  // Use widgets demo even without touch for better visuals
#endif
        // example_lvgl_demo_ui();
        // Release the mutex
        lvgl_port_unlock();
        ESP_LOGI(TAG, "LVGL demo started successfully");
    } else {
        ESP_LOGE(TAG, "Failed to lock LVGL mutex");
    }

    // Keep the task alive
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGI(TAG, "Main task running...");
    }
}
