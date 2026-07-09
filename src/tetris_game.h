#ifndef TETRIS_GAME_H
#define TETRIS_GAME_H

#include <Arduino.h>
#include <MD_MAX72xx.h>
#include "config.h"
#include "display_manager.h"

// Font bitmaps for vertical GAME OVER flashing screen
extern const uint8_t font_G[8];
extern const uint8_t font_A[8];
extern const uint8_t font_M[8];
extern const uint8_t font_E[8];
extern const uint8_t font_O[8];
extern const uint8_t font_V[8];
extern const uint8_t font_R[8];

class TetrisGame {
private:
  enum GameState {
    PLAYING,
    GAME_OVER_ANIMATION,
    GAME_OVER_SCREEN
  };

  GameState _state;
  static const uint8_t WIDTH = 8;
  static const uint8_t HEIGHT = 64; // Scaled to full 8-matrix length
  
  byte _field[WIDTH][HEIGHT];
  
  // Game piece coordinates and orientation
  int _posX, _posY;
  int _shape, _rotation;
  
  unsigned long _lastFall;
  unsigned long _lastInput;
  
  int _score;
  int _level;
  int _speed;
  const int _baseSpeed = 380;
  const int _minSpeed = 70;
  const int _fastFallSpeed = 30; // Interval while GPIO 5 is held for soft/fast drop

  // Latch for button release
  bool _btnWasPressed;

  // True while the fast-fall button is currently held down
  bool _fastFallHeld;
  
  // Flashing game over screen state
  uint32_t _lastFlashTime;
  bool _flashState;

  // 7 standard Tetris pieces (each shape has 4 rotations of 4x4 matrix)
  const byte _shapes[7][4][4][4] = {
    // 0: O-piece (Square)
    {
      {{0,1,1,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}},
      {{0,1,1,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}},
      {{0,1,1,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}},
      {{0,1,1,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}}
    },
    // 1: I-piece (Line)
    {
      {{0,0,0,0},{1,1,1,1},{0,0,0,0},{0,0,0,0}},
      {{0,1,0,0},{0,1,0,0},{0,1,0,0},{0,1,0,0}},
      {{0,0,0,0},{1,1,1,1},{0,0,0,0},{0,0,0,0}},
      {{0,1,0,0},{0,1,0,0},{0,1,0,0},{0,1,0,0}}
    },
    // 2: T-piece
    {
      {{0,1,0,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}},
      {{0,1,0,0},{0,1,1,0},{0,1,0,0},{0,0,0,0}},
      {{0,0,0,0},{1,1,1,0},{0,1,0,0},{0,0,0,0}},
      {{0,1,0,0},{1,1,0,0},{0,1,0,0},{0,0,0,0}}
    },
    // 3: L-piece
    {
      {{0,0,1,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}},
      {{0,1,0,0},{0,1,0,0},{0,1,1,0},{0,0,0,0}},
      {{0,0,0,0},{1,1,1,0},{1,0,0,0},{0,0,0,0}},
      {{1,1,0,0},{0,1,0,0},{0,1,0,0},{0,0,0,0}}
    },
    // 4: J-piece
    {
      {{1,0,0,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}},
      {{0,1,1,0},{0,1,0,0},{0,1,0,0},{0,0,0,0}},
      {{0,0,0,0},{1,1,1,0},{0,0,1,0},{0,0,0,0}},
      {{0,1,0,0},{0,1,0,0},{1,1,0,0},{0,0,0,0}}
    },
    // 5: S-piece
    {
      {{0,1,1,0},{1,1,0,0},{0,0,0,0},{0,0,0,0}},
      {{0,1,0,0},{0,1,1,0},{0,0,1,0},{0,0,0,0}},
      {{0,1,1,0},{1,1,0,0},{0,0,0,0},{0,0,0,0}},
      {{0,1,0,0},{0,1,1,0},{0,0,1,0},{0,0,0,0}}
    },
    // 6: Z-piece
    {
      {{1,1,0,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}},
      {{0,0,1,0},{0,1,1,0},{0,1,0,0},{0,0,0,0}},
      {{1,1,0,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}},
      {{0,0,1,0},{0,1,1,0},{0,1,0,0},{0,0,0,0}}
    }
  };

  // Sound placeholders
  void playTone(uint16_t freq, uint16_t duration) {
    #if defined(BUZZER_PIN) && BUZZER_PIN >= 0
      tone(BUZZER_PIN, freq, duration);
    #endif
  }

  void playMoveSound()     { playTone(1200, 15); }
  void playRotateSound()   { playTone(1500, 30); }
  void playFallSound()     { playTone(180, 8);   }
  void playDropSound()     { playTone(250, 50);  }
  void playGameOverSound() { playTone(400, 150); delay(180); playTone(200, 250); }
  void playLineClearSound() {
    playTone(1318, 80); delay(90);
    playTone(1567, 80); delay(90);
    playTone(1975, 120);
  }

  // Check collision of block at (nx, ny) with rotation (r)
  bool collide(int nx, int ny, int r) {
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 4; j++) {
        if (_shapes[_shape][r][i][j]) {
          int x = nx + j;
          int y = ny + i;
          
          if (x < 0 || x >= WIDTH || y >= HEIGHT) return true;
          if (y >= 0 && _field[x][y]) return true;
        }
      }
    }
    return false;
  }

  // Spawns a new random Tetris piece
  void newPiece() {
    _shape = random(0, 7);
    _rotation = 0;
    _posX = 2; // Center
    _posY = 0;
    
    if (collide(_posX, _posY, _rotation)) {
      _state = GAME_OVER_ANIMATION;
      playGameOverSound();
    }
  }

  // Lock current piece into field grid
  void mergePiece() {
    playDropSound();
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 4; j++) {
        if (_shapes[_shape][_rotation][i][j]) {
          int x = _posX + j;
          int y = _posY + i;
          if (y >= 0 && y < HEIGHT) {
            _field[x][y] = 1;
          }
        }
      }
    }
  }

  // Scan and clear completed lines
  void clearLines(MD_MAX72XX* mx) {
    for (int y = 0; y < HEIGHT; y++) {
      bool full = true;
      for (int x = 0; x < WIDTH; x++) {
        if (!_field[x][y]) {
          full = false;
          break;
        }
      }
      
      if (full) {
        playLineClearSound();
        _score++;
        
        // Shift lines above down
        for (int yy = y; yy > 0; yy--) {
          for (int x = 0; x < WIDTH; x++) {
            _field[x][yy] = _field[x][yy - 1];
          }
        }
        for (int x = 0; x < WIDTH; x++) {
          _field[x][0] = 0;
        }
        
        // Level up every 3 lines
        if (_score % 3 == 0) {
          _level++;
          // Exponential speed scaling (0.84 feel)
          _speed = _baseSpeed * pow(0.84, _level);
          if (_speed < _minSpeed) _speed = _minSpeed;
          
          // Flash display intensity as visual feedback
          for (int i = 0; i < 2; i++) {
            mx->control(MD_MAX72XX::INTENSITY, 15);
            delay(80);
            mx->control(MD_MAX72XX::INTENSITY, 4);
            delay(80);
          }
        }
      }
    }
  }

  // Gist-style game over sweep animation
  void playGameOverAnim(MD_MAX72XX* mx) {
    // 4 screen flashes
    for (int k = 0; k < 4; k++) {
      mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
      mx->clear();
      mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
      delay(150);
      
      mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
      drawActiveField(mx);
      mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
      delay(150);
    }
    
    // Bottom-to-top pixel clear sweep
    for (int y = HEIGHT - 1; y >= 0; y--) {
      mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
      for (int x = 0; x < WIDTH; x++) {
        setPointFlipped(mx, x, y, false);
      }
      mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
      #if defined(BUZZER_PIN) && BUZZER_PIN >= 0
        tone(BUZZER_PIN, 200 + y * 6, 15);
      #endif
      delay(10);
    }
  }

  // Draw the Tetris board to buffer
  void drawActiveField(MD_MAX72XX* mx) {
    for (int x = 0; x < WIDTH; x++) {
      for (int y = 0; y < HEIGHT; y++) {
        bool pixel = _field[x][y];
        
        // Overlay current falling block
        for (int i = 0; i < 4; i++) {
          for (int j = 0; j < 4; j++) {
            if (_shapes[_shape][_rotation][i][j]) {
              if (x == _posX + j && y == _posY + i) {
                pixel = true;
              }
            }
          }
        }
        setPointFlipped(mx, x, y, pixel);
      }
    }
  }

  // Helper to draw a character rotated 90 degrees CCW
  void drawCharCCW(MD_MAX72XX* mx, uint8_t matrixIdx, const uint8_t font[8]) {
    for (uint8_t fRow = 0; fRow < 8; fRow++) {
      for (uint8_t fCol = 0; fCol < 8; fCol++) {
        bool state = (font[fRow] & (1 << (7 - fCol))) != 0;
        setPointFlipped(mx, fCol, matrixIdx * 8 + fRow, state);
      }
    }
  }

public:
  TetrisGame() : _state(PLAYING), _btnWasPressed(false), _fastFallHeld(false) {}

  void reset() {
    _state = PLAYING;
    memset(_field, 0, sizeof(_field));
    _score = 0;
    _level = 0;
    _speed = _baseSpeed;
    _lastFall = millis();
    _lastInput = millis();
    _btnWasPressed = false;
    _fastFallHeld = false;
    _lastFlashTime = 0;
    _flashState = true;
    
    newPiece();
    Serial.println("Tetris Game Reset (8x64).");
  }

  void handleInput(bool leftPressed, bool rightPressed, bool rotatePressed, bool fastFallHeld = false) {
    _fastFallHeld = fastFallHeld;

    if (_state == PLAYING) {
      if (rotatePressed && (millis() - _lastInput > 200)) {
        playRotateSound();
        int nr = (_rotation + 1) % 4;
        if (!collide(_posX, _posY, nr)) {
          _rotation = nr;
        }
        _lastInput = millis();
      } else {
        // Move Left (only if not moving right or rotating)
        if (leftPressed && !rightPressed && (millis() - _lastInput > 120)) {
          playMoveSound();
          if (!collide(_posX - 1, _posY, _rotation)) {
            _posX--;
          }
          _lastInput = millis();
        }
        // Move Right (only if not moving left or rotating)
        else if (rightPressed && !leftPressed && (millis() - _lastInput > 120)) {
          playMoveSound();
          if (!collide(_posX + 1, _posY, _rotation)) {
            _posX++;
          }
          _lastInput = millis();
        }
      }
    } else if (_state == GAME_OVER_SCREEN) {
      if ((leftPressed || rightPressed || rotatePressed) && !_btnWasPressed) {
        _btnWasPressed = true;
        reset();
        _btnWasPressed = true; // Latch so we release first
      } else if (!leftPressed && !rightPressed && !rotatePressed) {
        _btnWasPressed = false;
      }
    }
  }

  void updateLogic(MD_MAX72XX* mx) {
    if (_state == GAME_OVER_ANIMATION) {
      playGameOverAnim(mx);
      _state = GAME_OVER_SCREEN;
      _lastFlashTime = millis();
      _flashState = true;
      return;
    }
    
    if (_state == GAME_OVER_SCREEN) {
      if (millis() - _lastFlashTime >= 500) {
        _lastFlashTime = millis();
        _flashState = !_flashState;
      }
      return;
    }
    
    // Normal gravity drop (accelerated while fast-fall button is held)
    int interval = _fastFallHeld ? min(_speed, _fastFallSpeed) : _speed;
    if (millis() - _lastFall > (unsigned long)interval) {
      _lastFall = millis();
      
      if (!collide(_posX, _posY + 1, _rotation)) {
        _posY++;
        playFallSound();
      } else {
        mergePiece();
        clearLines(mx);
        newPiece();
      }
    }
  }

  void draw(MD_MAX72XX* mx) {
    if (_state == GAME_OVER_ANIMATION) {
      // Handled by updateLogic's sequential sweep animation
      return;
    }
    
    if (_state == GAME_OVER_SCREEN) {
      mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
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
      mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
      return;
    }
    
    // Draw active board with double-buffering
    mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
    mx->clear();
    drawActiveField(mx);
    mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
  }

  bool isPlaying() const {
    return _state == PLAYING;
  }
};

extern TetrisGame tetrisGame;

#endif // TETRIS_GAME_H
