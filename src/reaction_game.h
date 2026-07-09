#ifndef REACTION_GAME_H
#define REACTION_GAME_H

#include <Arduino.h>
#include <MD_MAX72xx.h>
#include "config.h"
#include "display_manager.h"

// Reaction Time Test phases
enum ReactionPhase {
  REACT_INTRO,   // Scrolling "Reaction Test! Tap When Ready" — waiting for a button to arm
  REACT_ARMED,   // Random delay counting down, screen blank — presses ignored
  REACT_FLASH,   // Screen flashing — waiting for the user's reaction press
  REACT_RESULT   // Showing the measured reaction time — waiting for a press to restart
};

class ReactionGame {
private:
  ReactionPhase _phase;
  uint32_t _phaseStartTime;
  uint32_t _armedDelay;
  uint32_t _reactionStartTime;
  uint32_t _lastFlashToggle;
  bool _flashOn;
  uint16_t _lastReactionMs;

  bool _prevLeft;
  bool _prevRight;

public:
  ReactionGame() :
    _phase(REACT_INTRO), _phaseStartTime(0), _armedDelay(0),
    _reactionStartTime(0), _lastFlashToggle(0), _flashOn(true),
    _lastReactionMs(0), _prevLeft(false), _prevRight(false) {}

  void reset() {
    _phase = REACT_INTRO;
    _phaseStartTime = millis();
    _prevLeft = false;
    _prevRight = false;
  }

  ReactionPhase getPhase() const { return _phase; }
  uint16_t getLastReactionMs() const { return _lastReactionMs; }

  // Pass raw button levels (true = pressed) plus the debounced single-press
  // pulse from the screen button (GPIO 5). Left/Right are GPIO 6/7.
  void handleInput(bool leftHeld, bool rightHeld, bool screenTap) {
    bool anyPressed = screenTap || (leftHeld && !_prevLeft) || (rightHeld && !_prevRight);
    _prevLeft = leftHeld;
    _prevRight = rightHeld;

    if (!anyPressed) return;

    switch (_phase) {
      case REACT_INTRO:
        _armedDelay = random(1500, 4500);
        _phaseStartTime = millis();
        _phase = REACT_ARMED;
        break;

      case REACT_ARMED:
        // Pressed too early — ignored, must wait for the flash.
        break;

      case REACT_FLASH:
        _lastReactionMs = (uint16_t)(millis() - _reactionStartTime);
        _phaseStartTime = millis();
        _phase = REACT_RESULT;
        break;

      case REACT_RESULT:
        _phase = REACT_INTRO;
        _phaseStartTime = millis();
        break;
    }
  }

  // Advances timer-driven phase transitions (call every loop). Returns true
  // if the phase changed this call, so the caller can refresh the display.
  bool update() {
    uint32_t now = millis();
    if (_phase == REACT_ARMED && now - _phaseStartTime >= _armedDelay) {
      _phase = REACT_FLASH;
      _reactionStartTime = now;
      _lastFlashToggle = now;
      _flashOn = true;
      tone(BUZZER_PIN, 2200, 120); // "Go!" beep
      return true;
    }
    return false;
  }

  // Fills every pixel with the current flash state, toggling periodically.
  void drawFlash(MD_MAX72XX* mx) {
    uint32_t now = millis();
    if (now - _lastFlashToggle >= 120) {
      _lastFlashToggle = now;
      _flashOn = !_flashOn;
    }

    mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
    for (uint8_t r = 0; r < 8; r++) {
      for (uint8_t c = 0; c < MAX_DEVICES * 8; c++) {
        mx->setPoint(r, c, _flashOn);
      }
    }
    mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
  }
};

extern ReactionGame reactionGame;

#endif // REACTION_GAME_H
