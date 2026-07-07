# ESP32 Attia Matrix

A multipurpose 8-matrix display ticker using ESP32-S3. Supports clock synchronization, stock ticker updates, and interactive retro games (Tetris, Stacker).

## GPIO Hardware Pin Configuration

Below are the configured GPIO connections on the ESP32-S3 dev kit:

### 1. MAX7219 Dot Matrix Display (FC16 Module)
| Pin Name | ESP32-S3 GPIO | Description |
| :--- | :--- | :--- |
| **DIN** | GPIO 11 | SPI Data Input |
| **CS** | GPIO 10 | SPI Chip Select |
| **CLK** | GPIO 12 | SPI Clock |

### 2. Control Buttons
*All buttons are configured as active-low inputs with internal pull-ups enabled (`INPUT_PULLUP`). Connect them to active GND.*

| Button Function | ESP32-S3 GPIO | Description |
| :--- | :--- | :--- |
| **Power Button** | GPIO 4 | Toggles the display screen On/Off |
| **Screen Button** | GPIO 5 | Cycle screens / Hold for Tetris rotate / Long hold to reset WiFi |
| **Game Left** | GPIO 6 | Move left in Tetris / Drop block in Stacker |
| **Game Right** | GPIO 7 | Move right in Tetris |
