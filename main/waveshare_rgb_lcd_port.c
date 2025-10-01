/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "waveshare_rgb_lcd_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static i2c_master_bus_handle_t s_i2c_bus_handle;
static i2c_master_dev_handle_t s_backlight_dev_handle;

// VSYNC event callback function
IRAM_ATTR static bool rgb_lcd_on_vsync_event(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t *edata, void *user_ctx)
{
    return lvgl_port_notify_rgb_vsync();
}

static esp_err_t waveshare_i2c_bus_init(void)
{
    if (s_i2c_bus_handle) {
        return ESP_OK;
    }

    const i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_MASTER_NUM,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .glitch_ignore_cnt = 7,
        .flags = {
            .enable_internal_pullup = true,
        },
    };

    esp_err_t err = i2c_new_master_bus(&bus_config, &s_i2c_bus_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2C bus (%s)", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

// GT911 Touch Controller Reset and Address Selection
static void gt911_select_addr_and_reset(bool use_0x5D)
{
    ESP_LOGI(TAG, "GT911 reset sequence: selecting address 0x%02X", use_0x5D ? 0x5D : 0x14);

    // Configure INT and RST pins as outputs
    gpio_config_t io_config = {
        .pin_bit_mask = (1ULL << EXAMPLE_PIN_NUM_TOUCH_INT) | (1ULL << EXAMPLE_PIN_NUM_TOUCH_RST),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_config);

    // 1) Hold reset low for 10ms
    gpio_set_level(EXAMPLE_PIN_NUM_TOUCH_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));

    // 2) Drive INT for address selection (HIGH = 0x5D, LOW = 0x14)
    gpio_set_level(EXAMPLE_PIN_NUM_TOUCH_INT, use_0x5D ? 1 : 0);
    vTaskDelay(pdMS_TO_TICKS(2));

    // 3) Release reset (datasheet recommends ~50ms+ delay)
    gpio_set_level(EXAMPLE_PIN_NUM_TOUCH_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(60));

    // 4) Set INT back to input mode
    gpio_set_direction(EXAMPLE_PIN_NUM_TOUCH_INT, GPIO_MODE_INPUT);

    ESP_LOGI(TAG, "GT911 reset sequence completed");
}

// I2C address scanner to detect GT911
static uint8_t gt911_scan_i2c_address(void)
{
    ESP_LOGI(TAG, "Scanning for GT911 I2C address...");

    // Test both possible addresses
    uint8_t test_addr = 0x00;
    uint8_t found_addr = 0;

    for (int i = 0; i < 2; i++) {
        test_addr = (i == 0) ? 0x5D : 0x14;

        // Create a temporary device handle for testing
        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = test_addr,
            .scl_speed_hz = 100000,
        };

        i2c_master_dev_handle_t test_dev_handle;
        esp_err_t err = i2c_master_bus_add_device(s_i2c_bus_handle, &dev_cfg, &test_dev_handle);

        if (err == ESP_OK) {
            // Try to write a test byte
            uint8_t test_data = 0x00;
            err = i2c_master_transmit(test_dev_handle, &test_data, 1, pdMS_TO_TICKS(20));

            if (err == ESP_OK) {
                ESP_LOGI(TAG, "✅ GT911 found at address 0x%02X", test_addr);
                found_addr = test_addr;
                i2c_master_bus_rm_device(test_dev_handle);
                break;
            } else {
                ESP_LOGI(TAG, "❌ GT911 not responding at 0x%02X: %s", test_addr, esp_err_to_name(err));
                i2c_master_bus_rm_device(test_dev_handle);
            }
        } else {
            ESP_LOGI(TAG, "❌ Cannot create device handle for 0x%02X: %s", test_addr, esp_err_to_name(err));
        }
    }

    if (found_addr == 0) {
        ESP_LOGE(TAG, "GT911 not found on I2C bus!");
        ESP_LOGE(TAG, "Check hardware: I2C pins, pull-ups, power, reset connections");
    }

    return found_addr;
}

static esp_err_t waveshare_i2c_backlight_init(void)
{
    if (!s_i2c_bus_handle) {
        ESP_LOGE(TAG, "I2C bus not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_backlight_dev_handle) {
        return ESP_OK;
    }

    const i2c_device_config_t backlight_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = CH422G_I2C_ADDRESS,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    esp_err_t err = i2c_master_bus_add_device(s_i2c_bus_handle, &backlight_config, &s_backlight_dev_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add backlight device (%s)", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

// Initialize RGB LCD
esp_err_t waveshare_esp32_s3_rgb_lcd_init()
{
    // I2C will be initialized only when needed for touch or backlight

    ESP_LOGI(TAG, "Install RGB LCD panel driver"); // Log the start of the RGB LCD panel driver installation
    esp_lcd_panel_handle_t panel_handle = NULL;    // Declare a handle for the LCD panel
    esp_lcd_rgb_panel_config_t panel_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT, // Set the clock source for the panel
        .timings = {
            .pclk_hz = EXAMPLE_LCD_PIXEL_CLOCK_HZ, // Pixel clock frequency
            .h_res = EXAMPLE_LCD_H_RES,            // Horizontal resolution
            .v_res = EXAMPLE_LCD_V_RES,            // Vertical resolution
#if ESP_PANEL_USE_1024_600_LCD
            .hsync_back_porch = 145, // Horizontal sync pulse width
            .hsync_front_porch = 170, // Horizontal back porch
            .hsync_pulse_width = 30, // Horizontal front porch
            .vsync_back_porch = 23,  // Vertical sync pulse width
            .vsync_front_porch = 12,  // Vertical back porch
            .vsync_pulse_width = 2,  // Vertical front porch
#else
            .hsync_pulse_width = 4, // Horizontal sync pulse width
            .hsync_back_porch = 8,  // Horizontal back porch
            .hsync_front_porch = 8, // Horizontal front porch
            .vsync_pulse_width = 4, // Vertical sync pulse width
            .vsync_back_porch = 8,  // Vertical back porch
            .vsync_front_porch = 8, // Vertical front porch
#endif
            .flags = {
                .pclk_active_neg = 1, // Active low pixel clock
            },
        },
        .data_width = EXAMPLE_RGB_DATA_WIDTH,                    // Data width for RGB
        .bits_per_pixel = EXAMPLE_RGB_BIT_PER_PIXEL,             // Bits per pixel
        .num_fbs = LVGL_PORT_LCD_RGB_BUFFER_NUMS,                // Number of frame buffers
        .bounce_buffer_size_px = EXAMPLE_RGB_BOUNCE_BUFFER_SIZE, // Bounce buffer size in pixels
        // .sram_trans_align = 4,                                   // SRAM transaction alignment - removed in ESP-IDF 6.0
        // .psram_trans_align = 64,                                 // PSRAM transaction alignment - removed in ESP-IDF 6.0
        .hsync_gpio_num = EXAMPLE_LCD_IO_RGB_HSYNC,              // GPIO number for horizontal sync
        .vsync_gpio_num = EXAMPLE_LCD_IO_RGB_VSYNC,              // GPIO number for vertical sync
        .de_gpio_num = EXAMPLE_LCD_IO_RGB_DE,                    // GPIO number for data enable
        .pclk_gpio_num = EXAMPLE_LCD_IO_RGB_PCLK,                // GPIO number for pixel clock
        .disp_gpio_num = EXAMPLE_LCD_IO_RGB_DISP,                // GPIO number for display
        .data_gpio_nums = {
            EXAMPLE_LCD_IO_RGB_DATA0,
            EXAMPLE_LCD_IO_RGB_DATA1,
            EXAMPLE_LCD_IO_RGB_DATA2,
            EXAMPLE_LCD_IO_RGB_DATA3,
            EXAMPLE_LCD_IO_RGB_DATA4,
            EXAMPLE_LCD_IO_RGB_DATA5,
            EXAMPLE_LCD_IO_RGB_DATA6,
            EXAMPLE_LCD_IO_RGB_DATA7,
            EXAMPLE_LCD_IO_RGB_DATA8,
            EXAMPLE_LCD_IO_RGB_DATA9,
            EXAMPLE_LCD_IO_RGB_DATA10,
            EXAMPLE_LCD_IO_RGB_DATA11,
            EXAMPLE_LCD_IO_RGB_DATA12,
            EXAMPLE_LCD_IO_RGB_DATA13,
            EXAMPLE_LCD_IO_RGB_DATA14,
            EXAMPLE_LCD_IO_RGB_DATA15,
        },
        .flags = {
            .fb_in_psram = 1, // Use PSRAM for framebuffer
        },
    };

    // Create a new RGB panel with the specified configuration
    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&panel_config, &panel_handle));

    ESP_LOGI(TAG, "Initialize RGB LCD panel");         // Log the initialization of the RGB LCD panel
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle)); // Initialize the LCD panel

    esp_lcd_touch_handle_t tp_handle = NULL; // Declare a handle for the touch panel (NULL for no touch)

#if CONFIG_EXAMPLE_LCD_TOUCH_CONTROLLER_GT911
    // Initialize I2C bus for touch controller
    ESP_ERROR_CHECK(waveshare_i2c_bus_init());

    // GT911 reset sequence and address detection
    gt911_select_addr_and_reset(true);  // Try 0x5D first (INT high during reset)

    // Scan for GT911 address
    uint8_t gt911_addr = gt911_scan_i2c_address();

    if (gt911_addr == 0) {
        ESP_LOGE(TAG, "GT911 not found, disabling touch controller");
        tp_handle = NULL;
    } else {
        esp_lcd_panel_io_handle_t tp_io_handle = NULL;
        esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
        tp_io_config.scl_speed_hz = 100000;  // Lower speed for reliability
        tp_io_config.dev_addr = gt911_addr;  // Use detected address

        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(s_i2c_bus_handle, &tp_io_config, &tp_io_handle));

        const esp_lcd_touch_config_t tp_cfg = {
            .x_max = EXAMPLE_LCD_H_RES,
            .y_max = EXAMPLE_LCD_V_RES,
            .rst_gpio_num = EXAMPLE_PIN_NUM_TOUCH_RST,
            .int_gpio_num = EXAMPLE_PIN_NUM_TOUCH_INT,
            .levels = {
                .reset = 0,
                .interrupt = 0,
            },
            .flags = {
                .swap_xy = 0,
                .mirror_x = 0,
                .mirror_y = 0,
            },
        };

        ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, &tp_handle));

        ESP_LOGI(TAG, "Touch controller initialized successfully at address 0x%02X", gt911_addr);
    }
#endif

    // Enable backlight BEFORE LVGL initialization so display is visible
    ESP_ERROR_CHECK(wavesahre_rgb_lcd_bl_on());

    // Small delay to ensure display is stable before LVGL starts
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_ERROR_CHECK(lvgl_port_init(panel_handle, tp_handle)); // Initialize LVGL with the panel and touch handles

    // Register callbacks for RGB panel events
    esp_lcd_rgb_panel_event_callbacks_t cbs = {
#if EXAMPLE_RGB_BOUNCE_BUFFER_SIZE > 0
        .on_vsync = rgb_lcd_on_vsync_event, // Callback for vertical sync (changed from on_bounce_frame_finish)
#else
        .on_vsync = rgb_lcd_on_vsync_event, // Callback for vertical sync
#endif
    };
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(panel_handle, &cbs, NULL)); // Register event callbacks

    return ESP_OK; // Return success
}

/******************************* Turn on the screen backlight **************************************/
esp_err_t wavesahre_rgb_lcd_bl_on()
{
    // Use GPIO control instead of I2C for backlight
    gpio_config_t bk_gpio_config = {
        .pin_bit_mask = (1ULL << EXAMPLE_PIN_NUM_BK_LIGHT),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));

    // Set backlight on level
    ESP_ERROR_CHECK(gpio_set_level(EXAMPLE_PIN_NUM_BK_LIGHT, EXAMPLE_LCD_BK_LIGHT_ON_LEVEL));

    ESP_LOGI(TAG, "Backlight ON (GPIO control)");
    return ESP_OK;
}

/******************************* Turn off the screen backlight **************************************/
esp_err_t wavesahre_rgb_lcd_bl_off()
{
    // Use GPIO control instead of I2C for backlight
    gpio_config_t bk_gpio_config = {
        .pin_bit_mask = (1ULL << EXAMPLE_PIN_NUM_BK_LIGHT),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));

    // Set backlight off level
    ESP_ERROR_CHECK(gpio_set_level(EXAMPLE_PIN_NUM_BK_LIGHT, EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL));

    ESP_LOGI(TAG, "Backlight OFF (GPIO control)");
    return ESP_OK;
}

/******************************* Example code **************************************/
static void draw_event_cb(lv_event_t *e) // Draw event callback function
{
    lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e); // Get the draw part descriptor
    if (dsc->part == LV_PART_ITEMS)
    {                                                                 // If drawing chart items
        lv_obj_t *obj = lv_event_get_target(e);                       // Get the target object of the event
        lv_chart_series_t *ser = lv_chart_get_series_next(obj, NULL); // Get the series of the chart
        uint32_t cnt = lv_chart_get_point_count(obj);                 // Get the number of points in the chart
        /* Make older values more transparent */
        dsc->rect_dsc->bg_opa = (LV_OPA_COVER * dsc->id) / (cnt - 1); // Set opacity based on the index

        /* Make smaller values blue, higher values red  */
        lv_coord_t *x_array = lv_chart_get_x_array(obj, ser); // Get the X-axis array
        lv_coord_t *y_array = lv_chart_get_y_array(obj, ser); // Get the Y-axis array
        /* dsc->id is the drawing order, but we need the index of the point being drawn dsc->id  */
        uint32_t start_point = lv_chart_get_x_start_point(obj, ser); // Get the start point of the chart
        uint32_t p_act = (start_point + dsc->id) % cnt;              // Calculate the actual index based on the start point
        lv_opa_t x_opa = (x_array[p_act] * LV_OPA_50) / 200;         // Calculate X-axis opacity
        lv_opa_t y_opa = (y_array[p_act] * LV_OPA_50) / 1000;        // Calculate Y-axis opacity

        dsc->rect_dsc->bg_color = lv_color_mix(lv_palette_main(LV_PALETTE_RED), // Mix colors
                                               lv_palette_main(LV_PALETTE_BLUE),
                                               x_opa + y_opa);
    }
}

static void add_data(lv_timer_t *timer) // Timer callback to add data to the chart
{
    lv_obj_t *chart = timer->user_data;                                                                        // Get the chart associated with the timer
    lv_chart_set_next_value2(chart, lv_chart_get_series_next(chart, NULL), lv_rand(0, 200), lv_rand(0, 1000)); // Add random data to the chart
}

// This demo UI is adapted from LVGL official example: https://docs.lvgl.io/master/examples.html#scatter-chart
void example_lvgl_demo_ui() // LVGL demo UI initialization function
{
    lv_obj_t *scr = lv_scr_act();                                              // Get the current active screen
    lv_obj_t *chart = lv_chart_create(scr);                                    // Create a chart object
    lv_obj_set_size(chart, 200, 150);                                          // Set chart size
    lv_obj_align(chart, LV_ALIGN_CENTER, 0, 0);                                // Center the chart on the screen
    lv_obj_add_event_cb(chart, draw_event_cb, LV_EVENT_DRAW_PART_BEGIN, NULL); // Add draw event callback
    lv_obj_set_style_line_width(chart, 0, LV_PART_ITEMS);                      /* Remove chart lines  */

    lv_chart_set_type(chart, LV_CHART_TYPE_SCATTER); // Set chart type to scatter

    lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_X, 5, 5, 5, 1, true, 30);  // Set X-axis ticks
    lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_Y, 10, 5, 6, 5, true, 50); // Set Y-axis ticks

    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_X, 0, 200);  // Set X-axis range
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 1000); // Set Y-axis range

    lv_chart_set_point_count(chart, 50); // Set the number of points in the chart

    lv_chart_series_t *ser = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y); // Add a series to the chart
    for (int i = 0; i < 50; i++)
    {                                                                            // Add random points to the chart
        lv_chart_set_next_value2(chart, ser, lv_rand(0, 200), lv_rand(0, 1000)); // Set X and Y values
    }

    lv_timer_create(add_data, 100, chart); // Create a timer to add new data every 100ms
}
