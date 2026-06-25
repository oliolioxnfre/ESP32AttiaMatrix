#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <MD_MAX72xx.h>

// --- MATRIX DISPLAY HARDWARE ---
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 8 // 1 Row of 8 matrices

// Pins connected to MAX7219
#define DATA_PIN  11  // DIN
#define CS_PIN    10  // CS
#define CLK_PIN   12  // CLK

// --- BUTTONS ---
#define POWER_BTN_PIN  4 // Toggle Display On/Off (GND active)
#define SCREEN_BTN_PIN 5 // Toggle Screen / Hold to Reset WiFi (GND active)

// Button timings
#define DEBOUNCE_TIME_MS 50
#define LONG_PRESS_TIME_MS 5000

// --- NVS PREFERENCES ---
#define PREF_NAMESPACE "matrix_cfg"
#define PREF_SSID "ssid"
#define PREF_PASS "password"
#define PREF_STOCKS "stocks"
#define PREF_API_KEY "finnhub_key"
#define PREF_TZ "timezone"

// --- DEFAULT FALLBACKS ---
#define DEFAULT_STOCKS "AAPL,MSFT,GOOG,TSLA"
#define DEFAULT_API_KEY "" // Finnhub free key
#define DEFAULT_TZ "EST5EDT,M3.2.0,M11.1.0" // Eastern Time

// --- ACCESS POINT CONFIG ---
#define AP_SSID "Attia-Matrix"
#define AP_IP IPAddress(192, 168, 4, 1)

// --- SYSTEM STATE ---
enum SystemState {
  STATE_BOOT_ANIMATION,
  STATE_WIFI_SETUP,
  STATE_CONNECTING_WIFI,
  STATE_STOCK_TICKER,
  STATE_CLOCK
};

#endif // CONFIG_H
