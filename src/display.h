#pragma once

#include <Arduino.h>
#include <LovyanGFX.hpp>

// ============================================================
// Display Driver - SmallTV Pro (ST7789V 240x240)
// LovyanGFX-based with differential rendering
// ============================================================

// Weather data struct is defined in weather.h
struct WeatherData;

// --- LovyanGFX hardware configuration ---

class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ST7789  _panel;
    lgfx::Bus_SPI       _bus;
    lgfx::Light_PWM     _light;

public:
    LGFX() {
        // SPI bus
        auto busCfg = _bus.config();
        busCfg.spi_host   = VSPI_HOST;
        busCfg.spi_mode   = 3;
        busCfg.freq_write  = 40000000;
        busCfg.freq_read   = 16000000;
        busCfg.pin_sclk    = TFT_SCK;
        busCfg.pin_mosi    = TFT_MOSI;
        busCfg.pin_miso    = -1;
        busCfg.pin_dc      = TFT_DC;
        _bus.config(busCfg);
        _panel.setBus(&_bus);

        // Panel
        auto panelCfg = _panel.config();
        panelCfg.pin_cs    = -1;
        panelCfg.pin_rst   = TFT_RST;
        panelCfg.panel_width   = DISPLAY_WIDTH;
        panelCfg.panel_height  = DISPLAY_HEIGHT;
        panelCfg.invert    = true;
        panelCfg.readable  = false;
        _panel.config(panelCfg);

        // Backlight (PWM)
        auto lightCfg = _light.config();
        lightCfg.pin_bl     = TFT_BL;
        lightCfg.invert     = true;
        lightCfg.freq       = 44100;
        lightCfg.pwm_channel = 0;
        _light.config(lightCfg);
        _panel.setLight(&_light);

        setPanel(&_panel);
    }
};

// --- Public API ---

void    displayInit();

// Rendering
void    displayRenderClock(const char* timeStr, const char* dateStr, const WeatherData* weather);
void    displayRenderAPMode(const char* ssid, const char* ip);
void    displayRenderMessage(const char* msg);
void    displayRenderOTAProgress(int percent);

// Brightness (0-100)
void    displaySetBrightness(uint8_t brightness);

// Raw access for advanced use
LGFX*   displayGetLCD();
