#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include "config.h"
#include "custom_font.h"

class DisplayManager {
private:
  MD_Parola _display;
  bool _isOn;
  char _scrollBuffer[512]; // Large buffer to hold stocks/clock text

  // Custom arrow character definitions (5 columns wide)
  // Format: {width, col0, col1, col2, col3, col4}
  // Up arrow '^' pointing up (shifted down by 1 pixel)
  uint8_t _arrowUp[6] = {5, 0x20, 0x30, 0xF8, 0x30, 0x20};
  // Down arrow 'v' pointing down (shifted down by 1 pixel)
  uint8_t _arrowDown[6] = {5, 0x20, 0x60, 0xF8, 0x60, 0x20};

public:
  DisplayManager() : 
    _display(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES),
    _isOn(true) {
    memset(_scrollBuffer, 0, sizeof(_scrollBuffer));
  }

  void begin() {
    _display.begin();
    _display.setIntensity(1); // Moderate brightness (0 to 15)
    _display.displayClear();
    
    // Set our custom shifted font
    _display.setFont(customFont);
    
    // Register our custom characters (already shifted to match the font)
    _display.addChar('^', _arrowUp);
    _display.addChar('v', _arrowDown);
    
#if FLIP_DISPLAY_180
    _display.setZoneEffect(0, true, PA_FLIP_UD);
    _display.setZoneEffect(0, true, PA_FLIP_LR);
#endif
  }

  // Toggles the display hardware state (power-saving)
  void setPower(bool on) {
    _isOn = on;
    _display.displayShutdown(!on);
    if (on) {
      _display.displayClear();
    }
  }

  bool isOn() const {
    return _isOn;
  }

  // Sets up the display to scroll a string with custom speed and pause
  void showScrollText(const char* text, uint16_t speed = 50, uint16_t pause = 0) {
    if (!_isOn) return;
    
    strncpy(_scrollBuffer, text, sizeof(_scrollBuffer) - 1);
    _scrollBuffer[sizeof(_scrollBuffer) - 1] = '\0';
    
    _display.displayClear();
#if FLIP_DISPLAY_180
    _display.displayText(_scrollBuffer, PA_LEFT, speed, pause, PA_SCROLL_RIGHT, PA_SCROLL_RIGHT);
#else
    _display.displayText(_scrollBuffer, PA_LEFT, speed, pause, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
#endif
  }

  // Sets up the display to show a static text, centered
  void showStaticText(const char* text) {
    if (!_isOn) return;
    
    strncpy(_scrollBuffer, text, sizeof(_scrollBuffer) - 1);
    _scrollBuffer[sizeof(_scrollBuffer) - 1] = '\0';
    
    _display.displayClear();
    _display.displayText(_scrollBuffer, PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
  }

  // Periodically called in loop() to run animations.
  // Returns true if the animation completed a cycle.
  bool update() {
    if (!_isOn) return false;
    
    if (_display.displayAnimate()) {
      _display.displayReset();
      return true;
    }
    return false;
  }

  void clear() {
    _display.displayClear();
  }

  void setIntensity(uint8_t intensity) {
    _display.setIntensity(intensity);
  }

  MD_MAX72XX* getGraphicObject() {
    return _display.getGraphicObject();
  }
};

extern DisplayManager displayManager;

inline void setPointFlipped(MD_MAX72XX* mx, uint8_t r, uint8_t c, bool state) {
#if FLIP_DISPLAY_180
  mx->setPoint(7 - r, 63 - c, state);
#else
  mx->setPoint(r, c, state);
#endif
}

#endif // DISPLAY_MANAGER_H
