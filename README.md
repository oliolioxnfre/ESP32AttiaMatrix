# ESP32 Attia Matrix

A desk display built on an ESP32-S3 and eight chained MAX7219 8x8 LED matrices (one 8x64 strip). It scrolls live stock quotes, shows an NTP-synced clock, runs a countdown timer, and has a few small games and animations, all driven by three buttons and a passive buzzer.

## Features at a glance

| Screen | What it does |
| :--- | :--- |
| **Stock Ticker** | Scrolls price, change and % change for your chosen symbols. Refreshes every 5 minutes in a background task. |
| **Clock** | 12-hour time synced from NTP, with a sliding-digit animation and your chosen timezone. |
| **Timer** | Countdown timer up to 99:59. Plays a melody on the buzzer when it finishes. |
| **Stacker** | Arcade-style tower stacking game played vertically on the strip. |
| **Tetris** | Tetris on a vertical playfield, with a score display and a high score saved across reboots. |
| **Reaction Test** | Waits a random 1.5 to 4.5 s, flashes the screen, and shows your reaction time in ms. |
| **Animations** | 13 LED effects (scanner, spectrum, heartbeat, sine wave, Pac-Man, space invader and more). |

On boot the display plays a space-invader animation, scrolls "Hello, HUMAN", then connects to Wi-Fi.

## Hardware

- ESP32-S3 dev board (the build targets `esp32-s3-devkitc1-n16r8`)
- 8 x MAX7219 8x8 LED matrix modules, FC16 type, chained (for example two "4-in-1" boards)
- 3 momentary push buttons
- 1 passive buzzer
- A USB power supply that can deliver enough current for the matrices (see Troubleshooting)

### GPIO pinout

| Component | Signal | ESP32-S3 GPIO |
| :--- | :--- | :--- |
| MAX7219 matrix | DIN (SPI data) | **11** |
| MAX7219 matrix | CS (chip select) | **10** |
| MAX7219 matrix | CLK (SPI clock) | **12** |
| Button | Screen / mode | **5** |
| Button | Left | **6** |
| Button | Right | **7** |
| Passive buzzer | Signal (+) | **8** |
| Passive buzzer | Ground (-) | **15** (held LOW in firmware) |

Wiring notes:

- **Buttons** are active-low with the ESP32's internal pull-ups enabled. Wire each button between its GPIO and GND. No resistors are needed.
- **Matrix power:** VCC to 5V and GND to GND on the board.
- **Buzzer ground:** the original build ran out of GND pins, so GPIO 15 is driven LOW and used as the buzzer's ground. If you have a spare GND pin, wire the buzzer's negative leg to GND instead. The firmware holding GPIO 15 low does no harm either way.
- **GPIO 4** is defined as `POWER_BTN_PIN` in `config.h`, but the current firmware doesn't use it. Display power is toggled with the screen button (see Controls).

All pins are set in [`src/config.h`](src/config.h) if you need to change them.

## Controls

There are only three buttons, so each one has several gestures.

### Global (Screen button, GPIO 5)

| Gesture | Action |
| :--- | :--- |
| Double-press | Go to the next screen |
| 5 quick presses | Turn the display on/off |
| Hold for 5 seconds | Erase saved Wi-Fi settings and reboot into setup mode |

Screen order: Stock Ticker, Clock, Timer, Stacker, Tetris, Reaction Test, Animations, then back to Stock Ticker.

### Per screen

| Screen | Left (GPIO 6) | Right (GPIO 7) | Screen button (GPIO 5) |
| :--- | :--- | :--- | :--- |
| Timer | Add time (hold to speed up) | Remove time (hold to speed up) | Single press: start / pause / resume |
| Stacker | Drop block | | |
| Tetris | Move left | Move right | Hold for fast drop |
| Tetris | Press **Left + Right** together to rotate | | |
| Reaction Test | Tap | Tap | Single press: tap |
| Animations | Previous effect | Next effect | |

When the timer alarm is going off, any button dismisses it.

## Stock quotes: Finnhub API key (required for real data)

Stock prices come from the [Finnhub](https://finnhub.io) quote API. **You need your own free API key.** None ships with this repo.

1. Create a free account at [finnhub.io](https://finnhub.io) and copy your API key.
2. Enter it in the **Finnhub API Key** field of the setup portal (below).

**Without a valid key, the ticker shows simulated prices.** If the key is empty, or a request fails (bad key, rate limit, network error), the firmware generates mock prices so the screen still scrolls. These mock prices look like real quotes. Don't treat what's on the ticker as real market data unless you've set a working key.

The free Finnhub tier is rate-limited. The firmware fetches each symbol once every 5 minutes, with a short delay between requests. It accepts up to 16 symbols, but keep the list short to stay within the free limits.

## Getting started

### 1. Build and flash

The project uses [PlatformIO](https://platformio.org/).

```bash
git clone https://github.com/oliolioxnfre/ESP32AttiaMatrix.git
cd ESP32AttiaMatrix
pio run -t upload
pio device monitor   # optional: serial log at 115200 baud
```

PlatformIO installs the libraries automatically: `MD_Parola`, `MD_MAX72XX` and `ArduinoJson`.

### 2. First-time setup (captive portal)

1. On first boot, with no Wi-Fi saved, the display scrolls **"WiFi Setup Mode!"**.
2. On your phone or laptop, join the Wi-Fi network **`Attia-Matrix`**.
3. A setup page should open automatically. If it doesn't, browse to `http://192.168.4.1`.
4. Fill in:
   - **Wi-Fi network and password.** The ESP32 supports 2.4 GHz networks only.
   - **Stock tickers**, comma-separated, for example `AAPL,MSFT,TSLA`
   - **Finnhub API key**
   - **Timezone**
5. Click **Connect & Save**. The device saves your settings to flash and reboots.

The device then scrolls its local IP address and moves to the Stock Ticker. If it can't connect within 15 seconds, it goes back to setup mode.

To change settings later, hold the screen button for 5 seconds to erase the saved Wi-Fi and get back to the portal.

## Customising

All of these are in [`src/config.h`](src/config.h) unless noted:

- `MAX_DEVICES`: number of chained 8x8 modules (default 8)
- `FLIP_DISPLAY_180`: set to `1` if your strip is mounted upside down
- `DEFAULT_STOCKS` and `DEFAULT_TZ`: the values pre-filled in the portal
- `AP_SSID`: the name of the setup Wi-Fi network
- Boot greeting: `"Hello, HUMAN"` in [`src/main.cpp`](src/main.cpp)
- Timer alarm melody: [`src/song_data.h`](src/song_data.h). Generate a new one from a monophonic MIDI file with `python3 scripts/midi_to_buzzer.py song.mid src/song_data.h` (needs `pip install mido`).

## Troubleshooting

| Symptom | Likely cause / fix |
| :--- | :--- |
| Display too dim or too bright | Brightness is set by `setIntensity()` in `src/display_manager.h` (0 to 15). The default is 1, because eight matrices at higher brightness can draw more current than the board supplies comfortably. |
| Display flickers, resets or browns out | Not enough power. Use a better USB supply, or power the matrices from a separate 5V source with a shared ground. |
| Image is upside down or mirrored | Toggle `FLIP_DISPLAY_180` in `config.h`. |
| Ticker shows prices that look wrong | You're probably seeing simulated data. Check your Finnhub key and the serial log ("Using simulated fallback"). |
| Stuck in setup mode | Wrong password, or a 5 GHz-only network. Rejoin `Attia-Matrix` and re-enter your details. |

## Security notes

This is a hobby device. Know these limits before you put it on a network you care about:

- **The setup portal is an open Wi-Fi network.** Anyone nearby can join it while the device is in setup mode. If the device drops back into setup mode after a failed connection, the portal page includes the saved Wi-Fi password and API key in its form fields.
- **TLS certificates aren't verified** when contacting Finnhub (`setInsecure()`), and the API key is sent as a URL query parameter.
- **Settings in flash (NVS) aren't encrypted.** Anyone with physical access to the board can read them.

Use a dedicated or guest network and a free-tier API key.

## Project layout

```
src/
  main.cpp             State machine, button handling, background stock task
  config.h             Pins, timings, defaults
  web_portal.h         Captive portal, settings storage (NVS), Wi-Fi connect
  stock_client.h       Finnhub client and simulated-data fallback
  clock_client.h       NTP / timezone
  display_manager.h    MD_Parola wrapper, display flip helper
  clock_screen.h, timer_screen.h, stacker_game.h, tetris_game.h,
  reaction_game.h, animation_screen.h   Individual screens
scripts/midi_to_buzzer.py   MIDI to buzzer melody converter
```
