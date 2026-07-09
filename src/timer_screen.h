#ifndef TIMER_SCREEN_H
#define TIMER_SCREEN_H

#include <Arduino.h>
#include <MD_MAX72xx.h>
#include "config.h"
#include "display_manager.h"
#include "clock_screen.h" // Reuse font_t and drawDigit approach
#include "song_data.h" // Alarm melody (converted from MIDI)

// Timer states
enum TimerState {
  TIMER_SETTING,
  TIMER_RUNNING,
  TIMER_PAUSED,
  TIMER_ALARM
};

class TimerScreen {
private:
  // Timer value in total seconds
  uint16_t _setSeconds;      // The value the user dialed in (remembered for reset)
  uint16_t _remainSeconds;   // Current remaining seconds during countdown

  TimerState _state;

  // Countdown timing
  uint32_t _lastCountdownTick; // millis() of last 1-second decrement

  // Hold-to-accelerate tracking
  uint32_t _upHoldStart;
  uint32_t _downHoldStart;
  uint32_t _lastUpRepeat;
  uint32_t _lastDownRepeat;
  bool _prevUp;
  bool _prevDown;
  bool _prevStartStop;

  // Display
  bool _colonState;
  uint32_t _lastColonBlink;

  // Buzzer alarm melody playback (non-blocking)
  size_t _songIndex;
  uint32_t _nextNoteTime;

  // --- Font drawing (reuses font_t from clock_screen.h) ---
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

  // Determine repeat rate based on how long the button has been held
  // Returns the interval in ms between increments, or 0 if no repeat yet
  uint16_t getRepeatInterval(uint32_t holdDuration) {
    if (holdDuration < 200) return 0;      // Initial debounce — no repeat yet
    if (holdDuration < 1000) return 200;   // ±1 second every 200ms
    if (holdDuration < 3000) return 150;   // ±10 seconds every 150ms
    return 100;                             // ±1 minute every 100ms
  }

  // Determine increment amount based on hold duration
  uint16_t getIncrementAmount(uint32_t holdDuration) {
    if (holdDuration < 1000) return 1;     // ±1 second
    if (holdDuration < 3000) return 10;    // ±10 seconds
    return 60;                              // ±1 minute
  }

  void adjustTime(int16_t delta) {
    int32_t newVal = (int32_t)_setSeconds + delta;
    if (newVal < 0) newVal = 0;
    if (newVal > 5999) newVal = 5999; // Cap at 99:59
    _setSeconds = (uint16_t)newVal;
    _remainSeconds = _setSeconds;
  }

  // Plays songNotes[] non-blocking, looping for as long as the alarm is active
  void buzzerTick() {
    uint32_t now = millis();
    if ((int32_t)(now - _nextNoteTime) < 0) return; // Not time for the next note yet

    BuzzerNote note;
    memcpy_P(&note, &billadySong[_songIndex], sizeof(BuzzerNote));
    if (note.freq > 0) {
      tone(BUZZER_PIN, note.freq, note.durationMs);
    } else {
      noTone(BUZZER_PIN);
    }

    _songIndex++;
    if (_songIndex >= billadySong_LEN) _songIndex = 0; // Loop the song

    uint16_t nextGap;
    memcpy_P(&nextGap, &billadySong[_songIndex].gapMs, sizeof(nextGap));
    _nextNoteTime = now + note.durationMs + nextGap;
  }

public:
  TimerScreen() :
    _setSeconds(60),   // Default 1:00
    _remainSeconds(60),
    _state(TIMER_SETTING),
    _lastCountdownTick(0),
    _upHoldStart(0),
    _downHoldStart(0),
    _lastUpRepeat(0),
    _lastDownRepeat(0),
    _prevUp(false),
    _prevDown(false),
    _prevStartStop(false),
    _colonState(true),
    _lastColonBlink(0),
    _songIndex(0),
    _nextNoteTime(0) {}

  void reset() {
    _state = TIMER_SETTING;
    _remainSeconds = _setSeconds;
    noTone(BUZZER_PIN);
    _songIndex = 0;
  }

  TimerState getState() const { return _state; }
  bool isAlarming() const { return _state == TIMER_ALARM; }

  // Called every loop iteration with current button states (true = pressed)
  void handleInput(bool upPressed, bool downPressed, bool startStopPressed) {
    uint32_t now = millis();

    switch (_state) {
      case TIMER_SETTING: {
        // --- UP button (GPIO 6) ---
        if (upPressed && !_prevUp) {
          // Just pressed — initial increment
          _upHoldStart = now;
          _lastUpRepeat = now;
          adjustTime(1);
        } else if (upPressed) {
          // Held — accelerating repeat
          uint32_t holdDur = now - _upHoldStart;
          uint16_t interval = getRepeatInterval(holdDur);
          if (interval > 0 && (now - _lastUpRepeat >= interval)) {
            _lastUpRepeat = now;
            adjustTime(getIncrementAmount(holdDur));
          }
        }

        // --- DOWN button (GPIO 7) ---
        if (downPressed && !_prevDown) {
          _downHoldStart = now;
          _lastDownRepeat = now;
          adjustTime(-1);
        } else if (downPressed) {
          uint32_t holdDur = now - _downHoldStart;
          uint16_t interval = getRepeatInterval(holdDur);
          if (interval > 0 && (now - _lastDownRepeat >= interval)) {
            _lastDownRepeat = now;
            adjustTime(-((int16_t)getIncrementAmount(holdDur)));
          }
        }

        // --- START button (GPIO 5) — single press starts timer ---
        if (startStopPressed && !_prevStartStop && _setSeconds > 0) {
          _state = TIMER_RUNNING;
          _remainSeconds = _setSeconds;
          _lastCountdownTick = now;
          Serial.println("Timer: Started countdown.");
        }
        break;
      }

      case TIMER_RUNNING: {
        // Single press pauses
        if (startStopPressed && !_prevStartStop) {
          _state = TIMER_PAUSED;
          Serial.println("Timer: Paused.");
        }
        break;
      }

      case TIMER_PAUSED: {
        // Single press resumes
        if (startStopPressed && !_prevStartStop) {
          _state = TIMER_RUNNING;
          _lastCountdownTick = now;
          Serial.println("Timer: Resumed.");
        }
        break;
      }

      case TIMER_ALARM: {
        // Any button press dismisses
        if ((startStopPressed && !_prevStartStop) ||
            (upPressed && !_prevUp) ||
            (downPressed && !_prevDown)) {
          noTone(BUZZER_PIN);
          _songIndex = 0;
          _state = TIMER_SETTING;
          _remainSeconds = _setSeconds; // Reset to last-set time
          Serial.println("Timer: Alarm dismissed.");
        }
        break;
      }
    }

    _prevUp = upPressed;
    _prevDown = downPressed;
    _prevStartStop = startStopPressed;
  }

  // Called every loop iteration to update countdown and render display
  void update(MD_MAX72XX* mx) {
    uint32_t now = millis();

    // --- Countdown logic ---
    if (_state == TIMER_RUNNING) {
      if (now - _lastCountdownTick >= 1000) {
        _lastCountdownTick += 1000; // Drift-corrected
        if (_remainSeconds > 0) {
          _remainSeconds--;
        }
        if (_remainSeconds == 0) {
          _state = TIMER_ALARM;
          _songIndex = 0;
          _nextNoteTime = now; // Start the melody immediately
          Serial.println("Timer: ALARM! Time's up!");
        }
      }
    }

    // --- Buzzer pattern in alarm state ---
    if (_state == TIMER_ALARM) {
      buzzerTick();
    }

    // --- Colon blink ---
    uint16_t blinkRate = (_state == TIMER_SETTING) ? 800 : 500;
    if (now - _lastColonBlink >= blinkRate) {
      _lastColonBlink = now;
      _colonState = !_colonState;
    }

    // --- Draw frame ---
    uint16_t displaySeconds = (_state == TIMER_RUNNING || _state == TIMER_PAUSED || _state == TIMER_ALARM)
                              ? _remainSeconds : _setSeconds;
    uint8_t mins = displaySeconds / 60;
    uint8_t secs = displaySeconds % 60;

    char digits[4];
    digits[0] = '0' + (mins / 10);
    digits[1] = '0' + (mins % 10);
    digits[2] = '0' + (secs / 10);
    digits[3] = '0' + (secs % 10);

    mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
    mx->clear();

    // Center MM:SS on the 64-column display
    // Using similar positioning to clock screen but only 4 digits + 1 colon
    // Total width: 4 * 7 (digits) + 4 (colon) + 3 * 2 (gaps) = ~36 pixels
    // Center offset: (64 - 36) / 2 = 14, so start from column ~50
    int baseX = 49;
    int positions[4] = {
      baseX,        // M2 (tens of minutes)
      baseX - 8,    // M1 (ones of minutes)
      baseX - 17,   // S2 (tens of seconds)
      baseX - 25    // S1 (ones of seconds)
    };

    for (int i = 0; i < 4; i++) {
      drawDigit(mx, digits[i], positions[i], 0);
    }

    // Draw colon between minutes and seconds
    // In TIMER_PAUSED, blink colon faster to indicate paused
    bool showColon = true;
    if (_state == TIMER_PAUSED) {
      showColon = (now / 250) % 2 == 0; // Fast blink when paused
    } else if (_state == TIMER_RUNNING) {
      showColon = _colonState;
    } else if (_state == TIMER_ALARM) {
      showColon = (now / 150) % 2 == 0; // Rapid blink during alarm
    }
    // In TIMER_SETTING, colon is always on for clarity
    if (_state == TIMER_SETTING) {
      showColon = true;
    }

    if (showColon) {
      drawDigit(mx, ':', baseX - 13, 0);
    }

    mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
  }
};

#endif // TIMER_SCREEN_H
