#ifndef STACKER_GAME_H
#define STACKER_GAME_H

#include <Arduino.h>
#include <MD_MAX72xx.h>
#include "config.h"
#include "display_manager.h"

class StackerGame {
private:
  enum GameState {
    PLAYING,
    GAME_OVER,
    GAME_WON,
    MESSAGE_SCROLL
  };

  GameState _state;
  uint8_t _currentBlockSize;
  int8_t _currentBlockLeft;
  int8_t _currentRow; // Ranges from 63 (bottom/base) to 0 (top/win)
  int8_t _moveDirection;
  uint32_t _lastMoveTime;
  uint32_t _speedMs;
  
  uint8_t _foundationLeft[64];
  uint8_t _foundationRight[64];
  
  // Stacker width is 8 pixels (mapped to the 8 physical rows of the display)
  const uint8_t PLAY_LEFT = 0;
  const uint8_t PLAY_RIGHT = 7;
  
  bool _btnWasPressed;

public:
  StackerGame() : _state(PLAYING), _btnWasPressed(false) {}

  void reset() {
    _state = PLAYING;
    _currentBlockSize = 3;  // 3 pixels is a perfect starting size for 8-pixel width
    _currentBlockLeft = 2;  // Center the block initially
    _currentRow = 63;       // Start at the bottom of the 64-column vertical layout
    _moveDirection = 1;
    _lastMoveTime = millis();
    _speedMs = 180;         // Starting speed (ms per step)
    
    for (int i = 0; i < 64; i++) {
      _foundationLeft[i] = 0;
      _foundationRight[i] = 0;
    }
    
    _btnWasPressed = false;
    Serial.println("Stacker Game Reset (Rotated Layout).");
  }

  void handleInput(bool btnPressed) {
    if (btnPressed && !_btnWasPressed) {
      _btnWasPressed = true;
      
      if (_state == PLAYING) {
        placeBlock();
      } else if (_state == GAME_OVER || _state == GAME_WON || _state == MESSAGE_SCROLL) {
        reset();
        // Prevent immediate block placement on restart by forcing the user
        // to release the button first before a new click can be registered.
        _btnWasPressed = true;
      }
    } else if (!btnPressed) {
      _btnWasPressed = false;
    }
  }

  void placeBlock() {
    Serial.print("Placed block at vertical row ");
    Serial.println(_currentRow);

    if (_currentRow == 63) {
      // First row at bottom always succeeds
      _foundationLeft[63] = _currentBlockLeft;
      _foundationRight[63] = _currentBlockLeft + _currentBlockSize - 1;
    } else {
      // Calculate overlap with the layer directly underneath (row + 1)
      uint8_t prevLeft = _foundationLeft[_currentRow + 1];
      uint8_t prevRight = _foundationRight[_currentRow + 1];
      uint8_t currLeft = _currentBlockLeft;
      uint8_t currRight = _currentBlockLeft + _currentBlockSize - 1;
      
      int8_t overlapLeft = (currLeft > prevLeft) ? currLeft : prevLeft;
      int8_t overlapRight = (currRight < prevRight) ? currRight : prevRight;
      
      if (overlapLeft > overlapRight) {
        // No overlap: Game Over!
        _state = GAME_OVER;
        Serial.println("Missed overlap! Game Over!");
        displayManager.showScrollText("GAME OVER! Press Game button to restart.  ", 50);
        return;
      }
      
      // Trim block size to overlap region
      _foundationLeft[_currentRow] = overlapLeft;
      _foundationRight[_currentRow] = overlapRight;
      _currentBlockSize = overlapRight - overlapLeft + 1;
    }
    
    // Check Win Condition (reached top row 0)
    if (_currentRow == 0) {
      _state = GAME_WON;
      Serial.println("Reached top level! Win!");
      displayManager.showScrollText("YOU WIN! Press Game button to restart.  ", 50);
      return;
    }
    
    // Move up to next vertical layer
    _currentRow--;
    
    // Gradual difficulty speed curve spanning 63 levels
    int difficulty = 63 - _currentRow;
    _speedMs = (difficulty < 30) ? (180 - difficulty * 3) : (90 - (difficulty - 30) * 1.5);
    if (_speedMs < 35) _speedMs = 35; // Cap maximum speed at 35ms per step
    
    // Reset block at left edge
    _currentBlockLeft = 0;
    _moveDirection = 1;
    _lastMoveTime = millis();
  }

  void updateLogic() {
    if (_state != PLAYING) {
      return;
    }
    
    if (millis() - _lastMoveTime >= _speedMs) {
      _lastMoveTime = millis();
      
      _currentBlockLeft += _moveDirection;
      
      // Bounce off side edges
      if (_currentBlockLeft <= PLAY_LEFT) {
        _currentBlockLeft = PLAY_LEFT;
        _moveDirection = 1;
      } else if (_currentBlockLeft >= PLAY_RIGHT - _currentBlockSize + 1) {
        _currentBlockLeft = PLAY_RIGHT - _currentBlockSize + 1;
        _moveDirection = -1;
      }
    }
  }

  void draw(MD_MAX72XX* mx) {
    if (_state == GAME_OVER || _state == GAME_WON) {
      // Transition immediately to message scrolling state
      _state = MESSAGE_SCROLL;
      return;
    }
    
    if (_state == MESSAGE_SCROLL) {
      // Animate Parola message
      displayManager.update();
      return;
    }
    
    // Clear raw frame buffer
    mx->clear();
    
    // Note: No vertical borders needed since the borders are now the physical edges of the 8-pixel width.
    
    // 1. Draw placed block stack (from bottom row 63 up to current row)
    for (int8_t c = 63; c > _currentRow; c--) {
      for (uint8_t r = _foundationLeft[c]; r <= _foundationRight[c]; r++) {
        mx->setPoint(r, c, true);
      }
    }
    
    // 2. Draw currently moving block at the current active row
    for (uint8_t i = 0; i < _currentBlockSize; i++) {
      mx->setPoint(_currentBlockLeft + i, _currentRow, true);
    }
  }
  
  bool isPlaying() const {
    return _state == PLAYING;
  }
};

extern StackerGame stackerGame;

#endif // STACKER_GAME_H
