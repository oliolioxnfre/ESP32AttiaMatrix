#ifndef TETRIS_GAME_H
#define TETRIS_GAME_H

#include <Arduino.h>
#include <MD_MAX72xx.h>
#include <Preferences.h>
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
  static const uint8_t HEADER_MODULES = 2; // Top 2 matrices reserved for the score HUD
  static const uint8_t HEADER_ROWS = HEADER_MODULES * 8; // 16 rows
  static const uint8_t HEIGHT = 64 - HEADER_ROWS; // Playfield height (48 rows, below the HUD)

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

  // Persistent high score (NVS)
  int _highScore;
  bool _highScoreLoaded;

  // Flashing game over screen state
  uint32_t _lastFlashTime;
  uint8_t _flashPhase; // 0: "GAME OVER" text, 1: blank, 2: high score

  // Compact 8x8 digit glyphs (rows top-to-bottom, MSB-first columns) for the score HUD
  const byte _digitFont[10][8] = {
    {0x00,0x3C,0x66,0x66,0x66,0x66,0x3C,0x00}, // 0
    {0x00,0x18,0x38,0x18,0x18,0x18,0x3C,0x00}, // 1
    {0x00,0x3C,0x66,0x06,0x1C,0x30,0x7E,0x00}, // 2
    {0x00,0x3C,0x66,0x0C,0x06,0x66,0x3C,0x00}, // 3
    {0x00,0x0C,0x1C,0x2C,0x4C,0x7E,0x0C,0x00}, // 4
    {0x00,0x7E,0x60,0x7C,0x06,0x66,0x3C,0x00}, // 5
    {0x00,0x1C,0x30,0x60,0x7C,0x66,0x3C,0x00}, // 6
    {0x00,0x7E,0x06,0x0C,0x18,0x18,0x18,0x00}, // 7
    {0x00,0x3C,0x66,0x3C,0x66,0x66,0x3C,0x00}, // 8
    {0x00,0x3C,0x66,0x66,0x3E,0x0C,0x38,0x00}  // 9
  };

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
      saveHighScoreIfBeaten();
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
        
        // Level up every 2 lines (faster than traditional Tetris, since our
        // 8-row-tall matrix gives far less reaction time per level already)
        if (_score % 2 == 0) {
          _level++;
          // Steeper exponential speed scaling (0.78 vs the traditional ~0.84 feel)
          _speed = _baseSpeed * pow(0.78, _level);
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
        setPointFlipped(mx, x, y + HEADER_ROWS, false);
      }
      mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
      #if defined(BUZZER_PIN) && BUZZER_PIN >= 0
        tone(BUZZER_PIN, 200 + y * 6, 15);
      #endif
      delay(10);
    }
  }

  // Draw the Tetris board to buffer (below the reserved score HUD rows)
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
        setPointFlipped(mx, x, y + HEADER_ROWS, pixel);
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

  // Draw a single digit (0-9) rotated to match the board orientation, into HUD matrix `matrixIdx`
  void drawDigitCCW(MD_MAX72XX* mx, uint8_t matrixIdx, uint8_t digit) {
    if (digit > 9) digit = 9;
    for (uint8_t fRow = 0; fRow < 8; fRow++) {
      for (uint8_t fCol = 0; fCol < 8; fCol++) {
        bool state = (_digitFont[digit][fRow] & (1 << (7 - fCol))) != 0;
        setPointFlipped(mx, fCol, matrixIdx * 8 + fRow, state);
      }
    }
  }

  // Draw a two-digit (00-99) number into the reserved HUD rows, starting at matrix `startModule`
  void drawTwoDigitNumber(MD_MAX72XX* mx, int value, uint8_t startModule) {
    if (value > 99) value = 99;
    if (value < 0) value = 0;
    drawDigitCCW(mx, startModule, value / 10);
    drawDigitCCW(mx, startModule + 1, value % 10);
  }

  void loadHighScore() {
    if (_highScoreLoaded) return;
    Preferences prefs;
    prefs.begin(PREF_NAMESPACE, true);
    _highScore = prefs.getInt(PREF_TETRIS_HIGH_SCORE, 0);
    prefs.end();
    _highScoreLoaded = true;
  }

  void saveHighScoreIfBeaten() {
    if (_score <= _highScore) return;
    _highScore = _score;
    Preferences prefs;
    prefs.begin(PREF_NAMESPACE, false);
    prefs.putInt(PREF_TETRIS_HIGH_SCORE, _highScore);
    prefs.end();
    Serial.printf("Tetris: New high score saved: %d\n", _highScore);
  }

public:
  TetrisGame() : _state(PLAYING), _btnWasPressed(false), _fastFallHeld(false),
                 _highScore(0), _highScoreLoaded(false) {}

  void reset() {
    loadHighScore();

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
    _flashPhase = 0;

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
      _flashPhase = 0;
      return;
    }

    if (_state == GAME_OVER_SCREEN) {
      if (millis() - _lastFlashTime >= 500) {
        _lastFlashTime = millis();
        _flashPhase = (_flashPhase + 1) % 3; // 0: "GAME OVER", 1: blank, 2: high score
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
      if (_flashPhase == 0) {
        // Draw vertical "GAME OVER"
        drawCharCCW(mx, 0, font_G);
        drawCharCCW(mx, 1, font_A);
        drawCharCCW(mx, 2, font_M);
        drawCharCCW(mx, 3, font_E);
        drawCharCCW(mx, 4, font_O);
        drawCharCCW(mx, 5, font_V);
        drawCharCCW(mx, 6, font_E);
        drawCharCCW(mx, 7, font_R);
      } else if (_flashPhase == 2) {
        // Show the saved high score in the same two-module HUD spot used during play
        drawTwoDigitNumber(mx, _highScore, 0);
      }
      mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
      return;
    }

    // Draw active board with double-buffering
    mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
    mx->clear();
    drawActiveField(mx);
    drawTwoDigitNumber(mx, _score, 0);
    mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
  }

  bool isPlaying() const {
    return _state == PLAYING;
  }
};

extern TetrisGame tetrisGame;

#endif // TETRIS_GAME_H
