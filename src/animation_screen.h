#ifndef ANIMATION_SCREEN_H
#define ANIMATION_SCREEN_H

#include <Arduino.h>
#include <MD_MAX72xx.h>
#include "config.h"
#include "display_manager.h"

// Ported from the "4 in 1 MAX7219" demo sketch (Daft Punk helmet animations),
// adapted to draw through setPointFlipped() so every effect respects
// FLIP_DISPLAY_180 and scales to the real 8x64 panel instead of the
// original 4-device demo board.
class AnimationsScreen {
private:
  enum Effect {
    FX_SCANNER,
    FX_RANDOM,
    FX_SPECTRUM,
    FX_HEARTBEAT,
    FX_HEARTS,
    FX_EYES,
    FX_BOUNCEBALL,
    FX_MIDLINE,
    FX_SINEWAVE,
    FX_FADE,
    FX_ARROWSCROLL,
    FX_INVADER,
    FX_PACMAN,
    FX_COUNT
  };

  uint8_t _effect;
  bool _init;
  uint32_t _lastFrame;
  uint32_t _effectStart;
  uint32_t _lastInput;

  // Shared scratch state used by whichever effect is currently active.
  uint8_t  _idx;
  int8_t   _dir;
  bool     _flag;
  uint8_t  _frame;
  int16_t  _idx16;

  uint8_t _scrollBuf[64]; // conveyor-scroll buffer for sinewave / arrow scroll

  // Writes an 8-bit column pattern (bit n = row n) through the flip-safe helper.
  void setCol(MD_MAX72XX* mx, int col, uint8_t bits) {
    for (uint8_t row = 0; row < 8; row++) {
      setPointFlipped(mx, row, col, (bits >> row) & 0x01);
    }
  }

  void clearAll(MD_MAX72XX* mx) {
    mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
    mx->clear();
  }

  void shiftBufLeft(uint8_t newCol) {
    for (uint8_t i = 0; i < 63; i++) _scrollBuf[i] = _scrollBuf[i + 1];
    _scrollBuf[63] = newCol;
  }

  void blitBuf(MD_MAX72XX* mx, uint8_t cols) {
    for (uint8_t c = 0; c < cols; c++) setCol(mx, c, _scrollBuf[c]);
  }

  // ---------- Effects ----------

  void fxScanner(MD_MAX72XX* mx, uint8_t cols) {
    const uint8_t width = 3;
    if (_init) {
      _idx = 0;
      _dir = 1;
      _init = false;
    }
    if (millis() - _lastFrame < 50) return;
    _lastFrame = millis();

    clearAll(mx);
    for (uint8_t i = 0; i < width; i++) setCol(mx, _idx + i, 0xff);
    mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);

    _idx += _dir;
    if (_idx == 0 || _idx + width >= cols) _dir = -_dir;
  }

  void fxRandom(MD_MAX72XX* mx, uint8_t cols) {
    if (millis() - _lastFrame < 150) return;
    _lastFrame = millis();

    clearAll(mx);
    for (uint8_t c = 0; c < cols; c++) setCol(mx, c, (uint8_t)random(256));
    mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
  }

  void fxSpectrum(MD_MAX72XX* mx, uint8_t cols) {
    if (millis() - _lastFrame < 100) return;
    _lastFrame = millis();

    clearAll(mx);
    for (uint8_t c = 0; c < cols; c++) {
      uint8_t r = random(8);
      uint8_t bar = 0;
      for (uint8_t j = 0; j < r; j++) bar |= (1 << j);
      setCol(mx, c, bar);
    }
    mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
  }

  void fxHeartbeat(MD_MAX72XX* mx, uint8_t cols) {
    static const uint8_t BASELINE = 4;
    static uint8_t state;
    static uint8_t r, c;
    static bool point;

    if (_init) {
      state = 0;
      r = BASELINE;
      c = cols - 1;
      point = true;
      _init = false;
      clearAll(mx);
      mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
    }

    if (millis() - _lastFrame < 20) return;
    _lastFrame = millis();

    setPointFlipped(mx, r, c, point);

    switch (state) {
      case 0: // straight line from the right
        if (c == cols / 2 + 8) state = 1;
        if (c > 0) c--;
        break;
      case 1: // up-stroke
        if (r != 0) { r--; if (c > 0) c--; }
        else state = 2;
        break;
      case 2: // down-stroke
        if (r != 7) { r++; if (c > 0) c--; }
        else state = 3;
        break;
      case 3: // recover to baseline
        if (r != BASELINE) { r--; if (c > 0) c--; }
        else state = 4;
        break;
      case 4: // straight line to the left, then restart
        if (c == 0) {
          c = cols - 1;
          point = !point;
          state = 0;
        } else c--;
        break;
    }
  }

  void fxHearts(MD_MAX72XX* mx, uint8_t cols) {
    const uint8_t full[]  = { 0x1c, 0x3e, 0x7e, 0xfc };
    const uint8_t empty[] = { 0x1c, 0x22, 0x42, 0x84 };
    const uint8_t dataSize = 4;
    const uint8_t numHearts = (MAX_DEVICES / 2) + 1;
    const uint8_t offset = cols / (numHearts + 1);

    if (_init) {
      _flag = true; // empty
      _init = false;
    }
    if (millis() - _lastFrame < 700) return;
    _lastFrame = millis();

    clearAll(mx);
    for (uint8_t h = 1; h <= numHearts; h++) {
      for (uint8_t i = 0; i < dataSize; i++) {
        uint8_t v = _flag ? empty[i] : full[i];
        setCol(mx, (h * offset) - dataSize + i, v);
        setCol(mx, (h * offset) + dataSize - i - 1, v);
      }
    }
    mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
    _flag = !_flag;
  }

  void fxEyes(MD_MAX72XX* mx, uint8_t cols) {
    const uint8_t eyeOpen[]  = { 0x18, 0x3c, 0x66, 0x66 };
    const uint8_t eyeClose[] = { 0x18, 0x3c, 0x3c, 0x3c };
    const uint8_t dataSize = 4;
    const uint8_t numEyes = 2;
    const uint8_t offset = cols / (numEyes + 1);

    if (_init) _init = false;
    if (millis() - _lastFrame < 500) return;
    _lastFrame = millis();

    bool open = (random(1000) > 150);
    clearAll(mx);
    for (uint8_t e = 1; e <= numEyes; e++) {
      for (uint8_t i = 0; i < dataSize; i++) {
        uint8_t v = open ? eyeOpen[i] : eyeClose[i];
        setCol(mx, (e * offset) - dataSize + i, v);
        setCol(mx, (e * offset) + dataSize - i - 1, v);
      }
    }
    mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
  }

  void fxBounceBall(MD_MAX72XX* mx, uint8_t cols) {
    if (_init) {
      _idx = 0;
      _dir = 1;
      _init = false;
    }
    if (millis() - _lastFrame < 50) return;
    _lastFrame = millis();

    clearAll(mx);
    setCol(mx, _idx, 0x18);
    setCol(mx, _idx + 1, 0x18);
    mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);

    _idx += _dir;
    if (_idx == 0 || _idx + 2 >= cols) _dir = -_dir;
  }

  void fxMidline(MD_MAX72XX* mx, uint8_t cols) {
    if (_init) {
      _idx = 0;
      _dir = 1;
      _init = false;
    }
    if (millis() - _lastFrame < 150) return;
    _lastFrame = millis();

    clearAll(mx);
    for (uint8_t c = 0; c < cols; c++) {
      setPointFlipped(mx, _idx, c, true);
      setPointFlipped(mx, 7 - _idx, c, true);
    }
    mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);

    _idx += _dir;
    if (_idx == 0 || _idx == 3) _dir = -_dir;
  }

  void fxSinewave(MD_MAX72XX* mx, uint8_t cols) {
    static const uint8_t SW_WIDTH = 11;
    static const uint8_t waves[][SW_WIDTH] = {
      {  9,   8,  6,   1,   6,  24,  96, 128,  96,  16,   0 },
      {  6,  12,  2,  12,  48,  64,  48,   0,   0,   0,   0 },
      { 10,  12,  2,   1,   2,  12,  48,  64, 128,  64,  48 },
    };
    static const uint8_t WAVE_COUNT = 3;

    if (_init) {
      memset(_scrollBuf, 0, sizeof(_scrollBuf));
      _frame = 0;
      _idx = 1;
      _init = false;
    }
    if (millis() - _lastFrame < 50) return;
    _lastFrame = millis();

    shiftBufLeft(waves[_frame][_idx++]);
    if (_idx > waves[_frame][0]) {
      _frame = random(WAVE_COUNT);
      _idx = 1;
    }

    clearAll(mx);
    blitBuf(mx, cols);
    mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
  }

  void fxFade(MD_MAX72XX* mx, uint8_t cols) {
    if (_init) {
      _idx = 0;
      _dir = 1;
      clearAll(mx);
      for (uint8_t c = 0; c < cols; c++) setCol(mx, c, 0xff);
      mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
      mx->control(MD_MAX72XX::INTENSITY, 0);
      _init = false;
    }
    if (millis() - _lastFrame < 200) return;
    _lastFrame = millis();

    _idx += _dir;
    if (_idx == 0 || _idx == MAX_INTENSITY) _dir = -_dir;
    mx->control(MD_MAX72XX::INTENSITY, _idx);
  }

  void fxArrowScroll(MD_MAX72XX* mx, uint8_t cols) {
    const uint8_t arrow[] = { 0x3c, 0x66, 0xc3, 0x99 };
    const uint8_t dataSize = 4;

    if (_init) {
      memset(_scrollBuf, 0, sizeof(_scrollBuf));
      _idx = 0;
      _init = false;
    }
    if (millis() - _lastFrame < 90) return;
    _lastFrame = millis();

    shiftBufLeft(arrow[_idx++]);
    if (_idx == dataSize) _idx = 0;

    clearAll(mx);
    blitBuf(mx, cols);
    mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
  }

  void fxInvader(MD_MAX72XX* mx, uint8_t cols) {
    const uint8_t invA[] = { 0x0e, 0x98, 0x7d, 0x36, 0x3c };
    const uint8_t invB[] = { 0x70, 0x18, 0x7d, 0xb6, 0x3c };
    const uint8_t dataSize = 5;

    if (_init) {
      _idx16 = -dataSize;
      _flag = false;
      _init = false;
    }
    if (millis() - _lastFrame < 150) return;
    _lastFrame = millis();

    clearAll(mx);
    for (uint8_t i = 0; i < dataSize; i++) {
      const uint8_t* set = _flag ? invA : invB;
      int left  = _idx16 - dataSize + i;
      int right = _idx16 + dataSize - i - 1;
      if (left >= 0 && left < cols) setCol(mx, left, set[i]);
      if (right >= 0 && right < cols) setCol(mx, right, set[i]);
    }
    mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);

    _idx16++;
    _flag = !_flag;
    if (_idx16 >= cols + (dataSize * 2)) _idx16 = -dataSize;
  }

  void fxPacman(MD_MAX72XX* mx, uint8_t cols) {
    static const uint8_t FRAMES = 4;
    static const uint8_t WIDTH = 18;
    static const uint8_t pac[FRAMES][WIDTH] = {
      { 0x3c, 0x7e, 0x7e, 0xff, 0xe7, 0xc3, 0x81, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x7b, 0xf3, 0x7f, 0xfb, 0x73, 0xfe },
      { 0x3c, 0x7e, 0xff, 0xff, 0xe7, 0xe7, 0x42, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x7b, 0xf3, 0x7f, 0xfb, 0x73, 0xfe },
      { 0x3c, 0x7e, 0xff, 0xff, 0xff, 0xe7, 0x66, 0x24, 0x00, 0x00, 0x00, 0xfe, 0x7b, 0xf3, 0x7f, 0xfb, 0x73, 0xfe },
      { 0x3c, 0x7e, 0xff, 0xff, 0xff, 0xff, 0x7e, 0x3c, 0x00, 0x00, 0x00, 0xfe, 0x7b, 0xf3, 0x7f, 0xfb, 0x73, 0xfe },
    };

    if (_init) {
      _idx16 = -1;
      _frame = 0;
      _dir = 1;
      _init = false;
    }
    if (millis() - _lastFrame < 100) return;
    _lastFrame = millis();

    _idx16++;
    clearAll(mx);
    for (uint8_t i = 0; i < WIDTH; i++) {
      int col = _idx16 - WIDTH + i;
      if (col >= 0 && col < cols) setCol(mx, col, pac[_frame][i]);
    }
    mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);

    _frame += _dir;
    if (_frame == 0 || _frame == FRAMES - 1) _dir = -_dir;

    if (_idx16 >= cols + WIDTH) _idx16 = -1;
  }

  void runEffect(MD_MAX72XX* mx) {
    uint8_t cols = mx->getColumnCount();
    switch (_effect) {
      case FX_SCANNER:     fxScanner(mx, cols);     break;
      case FX_RANDOM:      fxRandom(mx, cols);      break;
      case FX_SPECTRUM:    fxSpectrum(mx, cols);    break;
      case FX_HEARTBEAT:   fxHeartbeat(mx, cols);   break;
      case FX_HEARTS:      fxHearts(mx, cols);      break;
      case FX_EYES:        fxEyes(mx, cols);        break;
      case FX_BOUNCEBALL:  fxBounceBall(mx, cols);  break;
      case FX_MIDLINE:     fxMidline(mx, cols);     break;
      case FX_SINEWAVE:    fxSinewave(mx, cols);    break;
      case FX_FADE:        fxFade(mx, cols);        break;
      case FX_ARROWSCROLL: fxArrowScroll(mx, cols); break;
      case FX_INVADER:     fxInvader(mx, cols);     break;
      case FX_PACMAN:      fxPacman(mx, cols);      break;
    }
  }

public:
  AnimationsScreen() : _effect(0), _init(true), _lastFrame(0), _effectStart(0), _lastInput(0) {}

  void reset() {
    _effect = 0;
    _init = true;
    _lastFrame = millis();
    _effectStart = millis();
    _lastInput = millis();
  }

  void nextEffect() {
    _effect = (_effect + 1) % FX_COUNT;
    _init = true;
    _effectStart = millis();
  }

  void prevEffect() {
    _effect = (_effect == 0) ? FX_COUNT - 1 : _effect - 1;
    _init = true;
    _effectStart = millis();
  }

  // GAME_LEFT / GAME_RIGHT buttons manually step through effects.
  void handleInput(bool leftPressed, bool rightPressed) {
    if (millis() - _lastInput < 250) return;
    if (leftPressed && !rightPressed) {
      prevEffect();
      _lastInput = millis();
    } else if (rightPressed && !leftPressed) {
      nextEffect();
      _lastInput = millis();
    }
  }

  void update(MD_MAX72XX* mx) {
    runEffect(mx);
  }
};

extern AnimationsScreen animationsScreen;

#endif // ANIMATION_SCREEN_H
