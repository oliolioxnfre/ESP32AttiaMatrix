#ifndef CLOCK_SCREEN_H
#define CLOCK_SCREEN_H

#include <Arduino.h>
#include <MD_MAX72xx.h>
#include "config.h"
#include "display_manager.h"
#include "clock_client.h"

// Digit font definitions (height 8)
// Format: {width, row0, row1, row2, row3, row4, row5, row6, row7}
const uint8_t font_t[13][9] PROGMEM = {
  { 0x07, 0x1c, 0x22, 0x26, 0x2a, 0x32, 0x22, 0x1c, 0x00 },   // 0
  { 0x07, 0x08, 0x18, 0x08, 0x08, 0x08, 0x08, 0x1c, 0x00 },   // 1
  { 0x07, 0x1c, 0x22, 0x02, 0x04, 0x08, 0x10, 0x3e, 0x00 },   // 2
  { 0x07, 0x1c, 0x22, 0x02, 0x0c, 0x02, 0x22, 0x1c, 0x00 },   // 3
  { 0x07, 0x04, 0x0c, 0x14, 0x24, 0x3e, 0x04, 0x04, 0x00 },   // 4
  { 0x07, 0x3e, 0x20, 0x3c, 0x02, 0x02, 0x22, 0x1c, 0x00 },   // 5
  { 0x07, 0x0c, 0x10, 0x20, 0x3c, 0x22, 0x22, 0x1c, 0x00 },   // 6
  { 0x07, 0x3e, 0x02, 0x04, 0x08, 0x10, 0x10, 0x10, 0x00 },   // 7
  { 0x07, 0x1c, 0x22, 0x22, 0x1c, 0x22, 0x22, 0x1c, 0x00 },   // 8
  { 0x07, 0x1c, 0x22, 0x22, 0x1e, 0x02, 0x04, 0x18, 0x00 },   // 9
  { 0x04, 0x00, 0x06, 0x06, 0x00, 0x06, 0x06, 0x00, 0x00 },   // : (10)
  { 0x04, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00 },   // - (11)
  { 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }    // Space (12)
};

class ClockScreen {
private:
  struct Digit {
    char currentVal;
    char prevVal;
    bool scrolling;
    int16_t yOffset; // Vertical offset: starts at 8 or -8, moves to 0
  };

  Digit _digits[6]; // H2, H1, M2, M1, S2, S1
  bool _scrollingUp;
  uint32_t _lastTickTime;
  int16_t _zPosX;

  int getFontIndex(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch == ':') return 10;
    if (ch == '-') return 11;
    if (ch == ' ') return 12;
    return 12;
  }

  void drawDigit(MD_MAX72XX* mx, char ch, int posX, int posY) {
    int k = getFontIndex(ch);
    uint8_t width = pgm_read_byte(&font_t[k][0]);
    uint8_t bitMaskStart = 1 << (width - 1);

    for (int col = 0; col < width; col++) {
      int targetCol = posX - col;
      if (targetCol < 0 || targetCol >= 64) continue;

      for (int row = 0; row < 8; row++) {
        int targetRow = row - posY;
        if (targetRow < 0 || targetRow >= 8) continue;

        uint8_t rowData = pgm_read_byte(&font_t[k][row + 1]);
        bool pixelOn = (rowData & (bitMaskStart >> col)) != 0;
        if (pixelOn) {
          setPointFlipped(mx, targetRow, targetCol, true);
        }
      }
    }
  }

public:
  ClockScreen() : 
    _scrollingUp(false), // Default to scroll down
    _lastTickTime(0),
    _zPosX(56) {
    reset();
  }

  void reset() {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      int hour = timeinfo.tm_hour;
      if (hour == 0) hour = 12;
      else if (hour > 12) hour -= 12;

      _digits[0].currentVal = (hour / 10 == 0) ? ' ' : ('0' + (hour / 10));
      _digits[1].currentVal = '0' + (hour % 10);
      _digits[2].currentVal = '0' + (timeinfo.tm_min / 10);
      _digits[3].currentVal = '0' + (timeinfo.tm_min % 10);
      _digits[4].currentVal = '0' + (timeinfo.tm_sec / 10);
      _digits[5].currentVal = '0' + (timeinfo.tm_sec % 10);
    } else {
      // Not synced yet
      for (int i = 0; i < 6; i++) {
        _digits[i].currentVal = '-';
      }
    }

    for (int i = 0; i < 6; i++) {
      _digits[i].prevVal = _digits[i].currentVal;
      _digits[i].scrolling = false;
      _digits[i].yOffset = 0;
    }
    _lastTickTime = millis();
  }

  void update(MD_MAX72XX* mx) {
    uint32_t now = millis();

    // 1. Fetch current time
    struct tm timeinfo;
    char newDigits[6];
    if (getLocalTime(&timeinfo)) {
      int hour = timeinfo.tm_hour;
      if (hour == 0) hour = 12;
      else if (hour > 12) hour -= 12;

      newDigits[0] = (hour / 10 == 0) ? ' ' : ('0' + (hour / 10));
      newDigits[1] = '0' + (hour % 10);
      newDigits[2] = '0' + (timeinfo.tm_min / 10);
      newDigits[3] = '0' + (timeinfo.tm_min % 10);
      newDigits[4] = '0' + (timeinfo.tm_sec / 10);
      newDigits[5] = '0' + (timeinfo.tm_sec % 10);
    } else {
      // Draw dashes if not synced
      for (int i = 0; i < 6; i++) {
        newDigits[i] = '-';
      }
    }

    // 2. Manage digit scroll tick
    if (now - _lastTickTime >= 40) { // 40ms sliding steps
      _lastTickTime = now;

      // Update offsets of currently scrolling digits
      for (int i = 0; i < 6; i++) {
        if (_digits[i].scrolling) {
          if (_scrollingUp) {
            _digits[i].yOffset++;
            if (_digits[i].yOffset >= 0) {
              _digits[i].yOffset = 0;
              _digits[i].scrolling = false;
            }
          } else {
            _digits[i].yOffset--;
            if (_digits[i].yOffset <= 0) {
              _digits[i].yOffset = 0;
              _digits[i].scrolling = false;
            }
          }
        }
      }

      // Check if we need to initiate a new scroll transition for any digit
      for (int i = 0; i < 6; i++) {
        if (!_digits[i].scrolling && _digits[i].currentVal != newDigits[i]) {
          _digits[i].prevVal = _digits[i].currentVal;
          _digits[i].currentVal = newDigits[i];
          _digits[i].scrolling = true;
          _digits[i].yOffset = _scrollingUp ? -8 : 8; // Starts off-screen
        }
      }
    }

    // 3. Draw frame atomically using offscreen buffering
    mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
    mx->clear();

    // Map digit position coordinates on the 64-column matrix
    int positions[6] = {
      _zPosX - 2,  // H2
      _zPosX - 8,  // H1
      _zPosX - 19, // M2
      _zPosX - 25, // M1
      _zPosX - 36, // S2
      _zPosX - 42  // S1
    };

    for (int i = 0; i < 6; i++) {
      if (_digits[i].scrolling) {
        // Draw the old value sliding away
        int oldYOffset = _digits[i].yOffset + (_scrollingUp ? 8 : -8);
        drawDigit(mx, _digits[i].prevVal, positions[i], oldYOffset);
        // Draw the new value sliding in
        drawDigit(mx, _digits[i].currentVal, positions[i], _digits[i].yOffset);
      } else {
        // Static display
        drawDigit(mx, _digits[i].currentVal, positions[i], 0);
      }
    }

    // Draw static colons
    drawDigit(mx, ':', _zPosX - 15, 0);
    drawDigit(mx, ':', _zPosX - 32, 0);

    mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
  }
};

#endif // CLOCK_SCREEN_H
