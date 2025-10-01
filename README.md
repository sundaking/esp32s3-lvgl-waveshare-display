# ESP32-S3 LVGL Waveshare Display

[![ESP-IDF 6.0](https://img.shields.io/badge/ESP--IDF-6.0-orange.svg)](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/)
[![LVGL](https://img.shields.io/badge/LVGL-8.x-green.svg)](https://lvgl.io/)
[![License](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

Complete LVGL (Light and Versatile Graphics Library) port for ESP32-S3 with Waveshare 4.3" RGB LCD touch display. Features ESP-IDF 6.0 compatibility, optimized graphics rendering, and touch screen support.

## 🎯 Features

- **ESP32-S3** microcontroller with dual-core processing
- **Waveshare 4.3" RGB LCD** (800x480 resolution)
- **GT911 Touch Controller** with I2C communication
- **LVGL 8.x** graphics library integration
- **ESP-IDF 6.0** compatibility
- **PSRAM support** for enhanced graphics performance
- **Touch-enabled UI demos**

## 📋 Hardware Requirements

- **ESP32-S3 Development Board** (e.g., ESP32-S3-DevKitC-1)
- **Waveshare 4.3" RGB LCD** with touch panel
- **USB-C cable** for power and programming

### Pin Connections

| ESP32-S3 Pin | LCD Signal | Description |
|-------------|------------|-------------|
| GPIO 1      | LCD_RST   | LCD Reset |
| GPIO 2      | LCD_RS    | LCD Register Select |
| GPIO 3      | LCD_CS    | LCD Chip Select |
| GPIO 4      | TP_RST    | Touch Reset (GT911) |
| GPIO 5      | LCD_SCLK  | LCD SPI Clock |
| GPIO 6      | TP_INT    | Touch Interrupt (GT911) |
| GPIO 7      | LCD_MOSI  | LCD SPI MOSI |
| GPIO 8      | TP_SDA    | Touch I2C SDA (GT911) |
| GPIO 9      | TP_SCL    | Touch I2C SCL (GT911) |
| GPIO 10     | LCD_DC    | LCD Data/Command |
| GPIO 11     | TP_CS     | Touch Chip Select |
| GPIO 12     | LCD_BL    | LCD Backlight Control |

## 🚀 Quick Start

### Prerequisites

1. **Install ESP-IDF 6.0**:
   ```bash
   # Follow official ESP-IDF installation guide
   # https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/
   ```

2. **Clone this repository**:
   ```bash
   git clone https://github.com/sundaking/esp32s3-lvgl-waveshare-display.git
   cd esp32s3-lvgl-waveshare-display
   ```

3. **Set target and configure**:
   ```bash
   idf.py set-target esp32s3
   idf.py menuconfig  # Configure project settings
   ```

4. **Build and flash**:
   ```bash
   idf.py build
   idf.py flash
   idf.py monitor  # View serial output
   ```

## 📁 Project Structure

```
esp32s3-lvgl-waveshare-display/
├── main/                          # Main application code
│   ├── CMakeLists.txt            # CMake configuration
│   ├── main.c                    # Application entry point
│   ├── lvgl_port.c              # LVGL port implementation
│   ├── lvgl_port.h              # LVGL configuration
│   ├── waveshare_rgb_lcd_port.c # LCD driver implementation
│   └── waveshare_rgb_lcd_port.h # LCD hardware definitions
├── components/                   # ESP-IDF components
│   ├── lvgl__lvgl/              # LVGL library
│   └── espressif__esp_lcd_touch_gt911/ # GT911 touch driver
├── CODEX.md                     # Implementation guide and documentation
├── .gitignore                   # Git ignore rules
├── CMakeLists.txt              # Project CMake configuration
└── sdkconfig.defaults          # Default SDK configuration
```

## 🎨 LVGL Demos

The project includes several LVGL demo applications:

- **Widgets Demo** (`lv_demo_widgets`) - Interactive UI components
- **Music Demo** (`lv_demo_music`) - Audio player interface
- **Benchmark Demo** (`lv_demo_benchmark`) - Performance testing

Switch between demos by modifying `main/main.c`:

```c
// Use touch-enabled demo
lv_demo_widgets();

// Or use non-touch demo
lv_demo_music();
```

## 🔧 Configuration

### Touch Controller Setup

Edit `main/lvgl_port.h`:
```c
#define CONFIG_EXAMPLE_LCD_TOUCH_CONTROLLER_GT911 1  // Enable touch
#define EXAMPLE_LCD_H_RES 800                         // Display width
#define EXAMPLE_LCD_V_RES 480                         // Display height
```

### Display Configuration

Edit `main/waveshare_rgb_lcd_port.h`:
```c
#define EXAMPLE_LCD_H_RES 800      // LCD horizontal resolution
#define EXAMPLE_LCD_V_RES 480      // LCD vertical resolution
#define EXAMPLE_PIN_NUM_TOUCH_RST  4  // Touch reset pin
#define EXAMPLE_PIN_NUM_TOUCH_INT  6  // Touch interrupt pin
```

## 📚 Documentation

- **[CODEX.md](./CODEX.md)** - Complete implementation guide and troubleshooting
- **[ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/)** - Official ESP-IDF docs
- **[LVGL Documentation](https://docs.lvgl.io/)** - LVGL graphics library docs

## 🔍 Troubleshooting

### Common Issues

1. **Display not showing**:
   - Check pin connections
   - Verify ESP-IDF installation
   - Check serial monitor for error messages

2. **Touch not working**:
   - Enable touch controller in `lvgl_port.h`
   - Check I2C pin connections
   - Verify GT911 driver configuration

3. **Build errors**:
   - Ensure ESP-IDF 6.0 is properly installed
   - Check component dependencies
   - Verify target is set to `esp32s3`

### Debug Information

Check the serial monitor output for:
- Hardware initialization status
- LVGL initialization messages
- Touch controller status
- Error codes and descriptions

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request

## 📄 License

This project is licensed under the MIT License - see the LICENSE file for details.

## 🙏 Acknowledgments

- **Espressif Systems** for ESP-IDF framework
- **LVGL Team** for the excellent graphics library
- **Waveshare** for the RGB LCD display hardware

## 📞 Support

For issues and questions:
1. Check the [CODEX.md](./CODEX.md) implementation guide
2. Review ESP-IDF and LVGL documentation
3. Create an issue in this repository

---

**Happy coding!** 🎉