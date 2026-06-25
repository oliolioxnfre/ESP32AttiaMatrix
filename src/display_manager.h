#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include "config.h"

class DisplayManager {
private:
  MD_Parola _display;
  bool _isOn;
  char _scrollBuffer[512]; // Large buffer to hold stocks/clock text

  // Custom arrow character definitions (5 columns wide)
  // Format: {width, col0, col1, col2, col3, col4}
  // Up arrow '^' pointing up
  uint8_t _arrowUp[6] = {5, 0x10, 0x18, 0x7C, 0x18, 0x10};
  // Down arrow 'v' pointing down
  uint8_t _arrowDown[6] = {5, 0x10, 0x30, 0x7C, 0x30, 0x10};

public:
  DisplayManager() : 
    _display(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES),
    _isOn(true) {
    memset(_scrollBuffer, 0, sizeof(_scrollBuffer));
  }

  void begin() {
    _display.begin();
    _display.setIntensity(15); // Moderate brightness (0 to 15)
    _display.displayClear();
    
    // Register our custom characters
    _display.addChar('^', _arrowUp);
    _display.addChar('v', _arrowDown);
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
    _display.displayText(_scrollBuffer, PA_LEFT, speed, pause, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
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

#endif // DISPLAY_MANAGER_H
