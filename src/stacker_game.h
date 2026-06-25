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
  int8_t _currentRow;
  int8_t _moveDirection;
  uint32_t _lastMoveTime;
  uint32_t _speedMs;
  
  uint8_t _foundationLeft[8];
  uint8_t _foundationRight[8];
  
  const uint8_t PLAY_LEFT = 24;
  const uint8_t PLAY_RIGHT = 39;
  const uint8_t BORDER_LEFT = 23;
  const uint8_t BORDER_RIGHT = 40;
  
  bool _btnWasPressed;

public:
  StackerGame() : _state(PLAYING), _btnWasPressed(false) {}

  void reset() {
    _state = PLAYING;
    _currentBlockSize = 4;
    _currentBlockLeft = PLAY_LEFT;
    _currentRow = 7; // Start at bottom row (7)
    _moveDirection = 1;
    _lastMoveTime = millis();
    _speedMs = 180; // Starting speed (ms per horizontal step)
    
    for (int i = 0; i < 8; i++) {
      _foundationLeft[i] = 0;
      _foundationRight[i] = 0;
    }
    
    _btnWasPressed = false;
    Serial.println("Stacker Game Reset.");
  }

  void handleInput(bool btnPressed) {
    if (btnPressed && !_btnWasPressed) {
      _btnWasPressed = true;
      
      if (_state == PLAYING) {
        placeBlock();
      } else if (_state == GAME_OVER || _state == GAME_WON || _state == MESSAGE_SCROLL) {
        reset();
      }
    } else if (!btnPressed) {
      _btnWasPressed = false;
    }
  }

  void placeBlock() {
    Serial.print("Placed block at row ");
    Serial.println(_currentRow);

    if (_currentRow == 7) {
      // Bottom row always succeeds
      _foundationLeft[7] = _currentBlockLeft;
      _foundationRight[7] = _currentBlockLeft + _currentBlockSize - 1;
    } else {
      // Calculate overlap with the layer directly underneath
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
      Serial.println("Reached top! Win!");
      displayManager.showScrollText("YOU WIN! Press Game button to restart.  ", 50);
      return;
    }
    
    // Move up
    _currentRow--;
    
    // Speed increases as tower climbs
    _speedMs = max(35, 180 - (7 - _currentRow) * 20);
    
    // Reset block at left edge
    _currentBlockLeft = PLAY_LEFT;
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
      
      // Bounce off borders
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
    
    // 1. Draw boundary borders
    for (uint8_t r = 0; r < 8; r++) {
      mx->setPoint(r, BORDER_LEFT, true);
      mx->setPoint(r, BORDER_RIGHT, true);
    }
    
    // 2. Draw placed block stack
    for (int8_t r = 7; r > _currentRow; r--) {
      for (uint8_t col = _foundationLeft[r]; col <= _foundationRight[r]; col++) {
        mx->setPoint(r, col, true);
      }
    }
    
    // 3. Draw moving block
    for (uint8_t i = 0; i < _currentBlockSize; i++) {
      mx->setPoint(_currentRow, _currentBlockLeft + i, true);
    }
  }
  
  bool isPlaying() const {
    return _state == PLAYING;
  }
};

extern StackerGame stackerGame;

#endif // STACKER_GAME_H
