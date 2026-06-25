#ifndef STACKER_GAME_H
#define STACKER_GAME_H

#include <Arduino.h>
#include <MD_MAX72xx.h>
#include "config.h"
#include "display_manager.h"

// --- CUSTOM 8x8 FONT BITMAPS (UPRIGHT) ---
const uint8_t font_G[8] = { 0x3E, 0x66, 0x40, 0x4C, 0x46, 0x66, 0x3E, 0x00 };
const uint8_t font_A[8] = { 0x1C, 0x36, 0x63, 0x7F, 0x63, 0x63, 0x63, 0x00 };
const uint8_t font_M[8] = { 0x63, 0x77, 0x7F, 0x6B, 0x63, 0x63, 0x63, 0x00 };
const uint8_t font_E[8] = { 0x7F, 0x40, 0x40, 0x7C, 0x40, 0x40, 0x7F, 0x00 };
const uint8_t font_O[8] = { 0x3E, 0x66, 0x63, 0x63, 0x63, 0x66, 0x3E, 0x00 };
const uint8_t font_V[8] = { 0x63, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x08, 0x00 };
const uint8_t font_R[8] = { 0x7C, 0x66, 0x66, 0x7C, 0x78, 0x6C, 0x66, 0x00 };

const uint8_t font_Y[8] = { 0x63, 0x63, 0x63, 0x3E, 0x08, 0x08, 0x08, 0x00 };
const uint8_t font_U[8] = { 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x3E, 0x00 };
const uint8_t font_W[8] = { 0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00 };
const uint8_t font_I[8] = { 0x3E, 0x08, 0x08, 0x08, 0x08, 0x08, 0x3E, 0x00 };
const uint8_t font_N[8] = { 0x63, 0x73, 0x7B, 0x6F, 0x67, 0x63, 0x63, 0x00 };
const uint8_t font_space[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

class StackerGame {
private:
  enum GameState {
    PLAYING,
    GAME_OVER,
    GAME_WON
  };

  GameState _state;
  uint8_t _currentBlockSize;
  int8_t _currentBlockLeft;
  int8_t _currentRow; // 63 (bottom) to 0 (top/win)
  int8_t _moveDirection;
  uint32_t _lastMoveTime;
  uint32_t _speedMs;
  
  uint8_t _foundationLeft[64];
  uint8_t _foundationRight[64];
  
  // Consecutive perfect matches tracking
  uint8_t _perfectMatchCount;

  // Stacker width is 8 pixels (mapped to the 8 physical rows of the display)
  const uint8_t PLAY_LEFT = 0;
  const uint8_t PLAY_RIGHT = 7;
  
  bool _btnWasPressed;

  // Flashing animation variables
  uint32_t _lastFlashTime;
  bool _flashState;

  // Helper to draw a character rotated 90 degrees CCW (mirrored corrected)
  void drawCharCCW(MD_MAX72XX* mx, uint8_t matrixIdx, const uint8_t font[8]) {
    for (uint8_t fRow = 0; fRow < 8; fRow++) {
      for (uint8_t fCol = 0; fCol < 8; fCol++) {
        bool state = (font[fRow] & (1 << (7 - fCol))) != 0;
        // Rotated 90 degrees CCW (horizontal flip corrected by using fCol instead of 7 - fCol):
        // row (horizontal) = fCol
        // column (vertical) = matrixIdx * 8 + fRow
        mx->setPoint(fCol, matrixIdx * 8 + fRow, state);
      }
    }
  }

public:
  StackerGame() : _state(PLAYING), _btnWasPressed(false) {}

  void reset() {
    _state = PLAYING;
    _currentBlockSize = 7;  // Start with 7-pixel base size
    _currentBlockLeft = 0;  // Position from 0 to 7
    _currentRow = 63;       // Start at the bottom of the 64-column vertical layout
    _moveDirection = 1;
    _lastMoveTime = millis();
    _speedMs = 180;         // Starting speed (ms per step)
    _perfectMatchCount = 0; // Reset perfect match streak
    
    for (int i = 0; i < 64; i++) {
      _foundationLeft[i] = 0;
      _foundationRight[i] = 0;
    }
    
    _btnWasPressed = false;
    _lastFlashTime = 0;
    _flashState = true;
    Serial.println("Stacker Game Reset (Double Buffering Active).");
  }

  void handleInput(bool btnPressed) {
    if (btnPressed && !_btnWasPressed) {
      _btnWasPressed = true;
      
      if (_state == PLAYING) {
        placeBlock();
      } else {
        reset();
        // Prevent immediate placement on restart
        _btnWasPressed = true;
      }
    } else if (!btnPressed) {
      _btnWasPressed = false;
    }
  }

  void placeBlock() {
    Serial.print("Placed block at vertical row ");
    Serial.println(_currentRow);

    bool isPerfectMatch = false;

    if (_currentRow == 63) {
      // First row always succeeds
      _foundationLeft[63] = _currentBlockLeft;
      _foundationRight[63] = _currentBlockLeft + _currentBlockSize - 1;
    } else {
      // Calculate overlap with the layer directly underneath (row + 1)
      uint8_t prevLeft = _foundationLeft[_currentRow + 1];
      uint8_t prevRight = _foundationRight[_currentRow + 1];
      uint8_t currLeft = _currentBlockLeft;
      uint8_t currRight = _currentBlockLeft + _currentBlockSize - 1;
      
      // Check for perfect match
      if (currLeft == prevLeft && currRight == prevRight) {
        isPerfectMatch = true;
        _perfectMatchCount++;
        Serial.print("PERFECT MATCH! Consecutive streak: ");
        Serial.println(_perfectMatchCount);
      } else {
        // Streak broken! Reset to 0.
        _perfectMatchCount = 0;
        Serial.println("Streak broken! Perfect match count reset.");
      }

      int8_t overlapLeft = (currLeft > prevLeft) ? currLeft : prevLeft;
      int8_t overlapRight = (currRight < prevRight) ? currRight : prevRight;
      
      if (overlapLeft > overlapRight) {
        // No overlap: Game Over!
        _state = GAME_OVER;
        _lastFlashTime = millis();
        _flashState = true;
        Serial.println("Missed overlap! Game Over!");
        return;
      }
      
      // Trim block size to overlap region
      _foundationLeft[_currentRow] = overlapLeft;
      _foundationRight[_currentRow] = overlapRight;
      _currentBlockSize = overlapRight - overlapLeft + 1;

      // Handle perfect match bonuses:
      // 10 consecutive matches -> regain 1 pixel. 11 consecutive -> regain another, up to 7 pixels max.
      if (isPerfectMatch && _perfectMatchCount >= 10) {
        if (_currentBlockSize < 7) {
          _currentBlockSize++;
          // Expand the physical placed foundation so next level has a wider base
          if (_foundationRight[_currentRow] < 7) {
            _foundationRight[_currentRow]++;
          } else if (_foundationLeft[_currentRow] > 0) {
            _foundationLeft[_currentRow]--;
          }
          Serial.print("Streak Bonus! Gained bonus pixel. New size: ");
          Serial.println(_currentBlockSize);
        }
      }
    }
    
    // Check Win Condition
    if (_currentRow == 0) {
      _state = GAME_WON;
      _lastFlashTime = millis();
      _flashState = true;
      Serial.println("Reached top level! Win!");
      return;
    }
    
    // Move up
    _currentRow--;
    
    // Gradual difficulty speed curve
    int difficulty = 63 - _currentRow;
    _speedMs = (difficulty < 30) ? (180 - difficulty * 3) : (90 - (difficulty - 30) * 1.5);
    if (_speedMs < 35) _speedMs = 35;
    
    // Reset block at left edge
    _currentBlockLeft = 0;
    _moveDirection = 1;
    _lastMoveTime = millis();
  }

  void updateLogic() {
    if (_state != PLAYING) {
      // Update flashing timer for win/lose screens
      if (millis() - _lastFlashTime >= 500) {
        _lastFlashTime = millis();
        _flashState = !_flashState;
      }
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
    if (_state == GAME_OVER) {
      mx->clear();
      if (_flashState) {
        // Draw vertical "GAME OVER"
        drawCharCCW(mx, 0, font_G);
        drawCharCCW(mx, 1, font_A);
        drawCharCCW(mx, 2, font_M);
        drawCharCCW(mx, 3, font_E);
        drawCharCCW(mx, 4, font_O);
        drawCharCCW(mx, 5, font_V);
        drawCharCCW(mx, 6, font_E);
        drawCharCCW(mx, 7, font_R);
      }
      return;
    }

    if (_state == GAME_WON) {
      mx->clear();
      if (_flashState) {
        // Draw vertical "YOU WIN"
        drawCharCCW(mx, 0, font_Y);
        drawCharCCW(mx, 1, font_O);
        drawCharCCW(mx, 2, font_U);
        drawCharCCW(mx, 3, font_space);
        drawCharCCW(mx, 4, font_W);
        drawCharCCW(mx, 5, font_I);
        drawCharCCW(mx, 6, font_N);
        drawCharCCW(mx, 7, font_space);
      }
      return;
    }
    
    // 1. Disable automatic hardware updates to construct the frame in RAM buffer
    mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);

    // 2. Clear raw frame buffer in software
    mx->clear();
    
    // 3. Draw placed block stack
    for (int8_t c = 63; c > _currentRow; c--) {
      for (uint8_t r = _foundationLeft[c]; r <= _foundationRight[c]; r++) {
        mx->setPoint(r, c, true);
      }
    }
    
    // 4. Draw currently moving block
    for (uint8_t i = 0; i < _currentBlockSize; i++) {
      mx->setPoint(_currentBlockLeft + i, _currentRow, true);
    }

    // 5. Re-enable auto-updates and flush the entire buffer atomically to the hardware.
    // This removes screen tearing, strobing, and flicker completely.
    mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
  }
  
  bool isPlaying() const {
    return _state == PLAYING;
  }
};

extern StackerGame stackerGame;

#endif // STACKER_GAME_H
