#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "config.h"

class LGFX : public lgfx::LGFX_Device {
  lgfx::Bus_SPI _bus;
#ifdef ILI9341_DRIVER
  lgfx::Panel_ILI9341 _panel;
  lgfx::Touch_XPT2046 _touch;
#elif defined(ST7796_DRIVER)
  lgfx::Panel_ST7796 _panel;
#else
  lgfx::Panel_GC9A01 _panel;
#endif
  lgfx::Light_PWM _light;

public:
  LGFX() {
    {
      auto cfg = _bus.config();
      cfg.spi_host = VSPI_HOST;
      cfg.freq_write = 40000000;
      cfg.pin_sclk = TFT_SCLK;
      cfg.pin_mosi = TFT_MOSI;
      cfg.pin_miso = TFT_MISO;
      cfg.pin_dc = TFT_DC;
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }
    {
      auto cfg = _panel.config();
      cfg.pin_cs = TFT_CS;
      cfg.pin_rst = -1;
      cfg.panel_width = TFT_WIDTH;
      cfg.panel_height = TFT_HEIGHT;
#ifdef ILI9341_DRIVER
      cfg.invert = false;
      cfg.rgb_order = false;
#elif defined(ST7796_DRIVER)
      cfg.invert = false;
      cfg.rgb_order = false;
#endif
      _panel.config(cfg);
    }
    {
      auto cfg = _light.config();
      cfg.pin_bl = TFT_BL;
      cfg.invert = false;
      cfg.freq = 44100;
      cfg.pwm_channel = 7;
      _light.config(cfg);
      _panel.setLight(&_light);
    }
#ifdef ILI9341_DRIVER
    {
      auto cfg = _touch.config();
      // Typical CYD XPT2046 raw AD calibration limits
      cfg.x_min      = 300;
      cfg.x_max      = 3900;
      cfg.y_min      = 3700;
      cfg.y_max      = 200;
      cfg.pin_int    = XPT2046_IRQ;
      cfg.bus_shared = false;
      cfg.offset_rotation = 0;
      cfg.spi_host = HSPI_HOST;
      cfg.freq = 2500000;
      cfg.pin_sclk = XPT2046_CLK;
      cfg.pin_mosi = XPT2046_MOSI;
      cfg.pin_miso = XPT2046_MISO;
      cfg.pin_cs   = XPT2046_CS;
      _touch.config(cfg);
      _panel.setTouch(&_touch);
    }
#endif
    setPanel(&_panel);
  }
};
