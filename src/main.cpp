#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "config.h"
#include "display_manager.h"
#include "web_portal.h"
#include "stock_client.h"
#include "clock_client.h"
#include "clock_screen.h"
#include "stacker_game.h"
#include "tetris_game.h"
#include "timer_screen.h"

// --- GLOBAL INSTANCES ---
DisplayManager displayManager;
WebPortal webPortal;
StockClient stockClient;
ClockClient clockClient;
ClockScreen clockScreen;
StackerGame stackerGame;
TetrisGame tetrisGame;
TimerScreen timerScreen;

// --- STATE MANAGEMENT ---
SystemState currentState = STATE_BOOT_ANIMATION;
uint32_t lastStockFetchTime = 0;
bool stockScrollInit = true;
bool rotateTriggeredByHold = false;

// Clock states
enum ClockSubState {
  CLOCK_INIT,
  CLOCK_TIME,
  CLOCK_DATE
};
ClockSubState clockSubState = CLOCK_INIT;
uint32_t clockStateStartTime = 0;
bool colonState = true;
uint32_t lastColonBlinkTime = 0;

// --- THREAD-SAFE BACKGROUND STOCK FETCHING ---
SemaphoreHandle_t stockMutex = NULL;
String globalStockTicker = "Loading Stocks...   ";
volatile bool triggerStockFetch = false;
volatile bool stockFetchInProgress = false;

// Task handle
TaskHandle_t stockTaskHandle = NULL;

void stockFetchTask(void* pvParameters) {
  Serial.println("[Task] Background Stock Fetcher started on Core 0");
  while (true) {
    if (triggerStockFetch) {
      // Reset trigger
      triggerStockFetch = false;
      stockFetchInProgress = true;
      
      Serial.println("[Task] Fetching fresh stock quotes...");
      String stocks = webPortal.getStocks();
      String apiKey = webPortal.getApiKey();
      
      String freshTicker = stockClient.getStocksTicker(stocks, apiKey);
      
      // Update the global string safely with mutex
      if (xSemaphoreTake(stockMutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
        globalStockTicker = freshTicker;
        xSemaphoreGive(stockMutex);
        Serial.println("[Task] Global stock ticker string updated successfully.");
      } else {
        Serial.println("[Task] Failed to acquire mutex for stock ticker update!");
      }
      
      stockFetchInProgress = false;
    }
    // Sleep for 1 second before checking trigger again
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// --- BUTTON DEBOUNCING AND INTERACTION ---
bool lastScreenBtnState = HIGH;
uint32_t screenBtnPressTime = 0;

// Double-press detection state
uint8_t screenPressCount = 0;
uint32_t firstPressTime = 0;
bool pendingSinglePress = false;

void cycleScreen() {
  // Cycle: Stock -> Clock -> Timer -> Stacker -> Tetris -> Stock
  if (currentState == STATE_STOCK_TICKER) {
    currentState = STATE_CLOCK;
    clockSubState = CLOCK_INIT;
    Serial.println("Button: Switched to Clock Screen");
  } else if (currentState == STATE_CLOCK) {
    currentState = STATE_TIMER;
    timerScreen.reset();
    Serial.println("Button: Switched to Timer Screen");
  } else if (currentState == STATE_TIMER) {
    // Make sure buzzer is off when leaving timer
    noTone(BUZZER_PIN);
    currentState = STATE_GAME;
    stackerGame.reset();
    Serial.println("Button: Switched to Stacker Game Screen");
  } else if (currentState == STATE_GAME) {
    currentState = STATE_TETRIS;
    tetrisGame.reset();
    Serial.println("Button: Switched to Tetris Screen");
  } else {
    currentState = STATE_STOCK_TICKER;
    stockScrollInit = true;
    Serial.println("Button: Switched to Stock Ticker Screen");
  }
}

void handleButtons() {
  bool screenVal = digitalRead(SCREEN_BTN_PIN);
  bool leftVal = digitalRead(GAME_LEFT_BTN_PIN);
  bool rightVal = digitalRead(GAME_RIGHT_BTN_PIN);
  uint32_t now = millis();
  
  // Pass button inputs to active game
  if (currentState == STATE_GAME) {
    stackerGame.handleInput(leftVal == LOW);
  } else if (currentState == STATE_TETRIS) {
    bool rotatePressed = (leftVal == LOW && rightVal == LOW) || rotateTriggeredByHold;
    rotateTriggeredByHold = false; // Reset trigger
    tetrisGame.handleInput(leftVal == LOW, rightVal == LOW, rotatePressed);
  } else if (currentState == STATE_TIMER) {
    // Pass GPIO 6 (up), GPIO 7 (down), and pending single-press of GPIO 5 (start/stop)
    // Note: start/stop is handled below via the pendingSinglePress mechanism
    timerScreen.handleInput(leftVal == LOW, rightVal == LOW, pendingSinglePress);
    pendingSinglePress = false;
  }
  
  // Screen Button (Double-Press Cycle / Long-Press Setup Reset / 5-Click Power Toggle)
  if (screenVal != lastScreenBtnState) {
    delay(10); // debounce
    screenVal = digitalRead(SCREEN_BTN_PIN);
    if (screenVal == LOW && lastScreenBtnState == HIGH) {
      screenBtnPressTime = millis();
    } else if (screenVal == HIGH && lastScreenBtnState == LOW) {
      // Released!
      uint32_t pressDuration = millis() - screenBtnPressTime;
      if (pressDuration < LONG_PRESS_TIME_MS) {
        if (pressDuration < CYCLE_HOLD_TIME_MS) {
          // Short press detected — count for double-press / 5-click
          static uint8_t rapidClickCount = 0;
          static uint32_t lastRapidClickTime = 0;
          
          if (now - lastRapidClickTime > 500) {
            rapidClickCount = 1;
          } else {
            rapidClickCount++;
          }
          lastRapidClickTime = now;
          
          // 5-click power toggle takes priority
          if (rapidClickCount == 5) {
            Serial.println("Button: 5 clicks detected! Toggling display power.");
            displayManager.setPower(!displayManager.isOn());
            rapidClickCount = 0;
            screenPressCount = 0; // Reset double-press state too
          } else {
            // Double-press detection for screen cycling
            screenPressCount++;
            if (screenPressCount == 1) {
              firstPressTime = now;
            } else if (screenPressCount >= 2) {
              if (now - firstPressTime <= DOUBLE_PRESS_WINDOW_MS) {
                // Double-press confirmed — cycle screen
                if (displayManager.isOn() && (currentState == STATE_STOCK_TICKER || currentState == STATE_CLOCK || 
                    currentState == STATE_TIMER || currentState == STATE_GAME || currentState == STATE_TETRIS)) {
                  cycleScreen();
                }
              }
              screenPressCount = 0;
            }
          }
        } else {
          // Medium Hold (between 1.5s and 5s): Rotate piece in Tetris
          if (displayManager.isOn() && currentState == STATE_TETRIS) {
            rotateTriggeredByHold = true;
            Serial.println("Button: Rotate triggered by hold!");
          }
        }
      }
    }
    lastScreenBtnState = screenVal;
  }
  
  // Check if a single-press window expired without a second press (single press for timer start/stop)
  if (screenPressCount == 1 && (now - firstPressTime > DOUBLE_PRESS_WINDOW_MS)) {
    screenPressCount = 0;
    // A confirmed single press — used for timer start/stop
    if (currentState == STATE_TIMER) {
      pendingSinglePress = true;
    }
  }
  
  // Check for Long Press (forces setup mode reset)
  if (screenVal == LOW && (millis() - screenBtnPressTime > LONG_PRESS_TIME_MS)) {
    Serial.println("Button: LONG PRESS! Resetting Wi-Fi configuration...");
    displayManager.setPower(true);
    displayManager.showScrollText("RESETTING WIFI SETUP... REBOOTING...", 50);
    
    // Non-blocking animation loop for 4 seconds so user sees the message scroll
    uint32_t resetAnimStart = millis();
    while (millis() - resetAnimStart < 4000) {
      displayManager.update();
      delay(10);
    }
    
    webPortal.resetWiFiSettings();
    ESP.restart();
  }
}

// --- SETUP & INITIALIZATION ---
void setup() {
  Serial.begin(115200);
  delay(1000); // Wait for serial to stabilize
  Serial.println("\n--- Multipurpose 1x8 Matrix Ticker starting ---");
  
  // Set up pins
  pinMode(SCREEN_BTN_PIN, INPUT_PULLUP);
  pinMode(GAME_LEFT_BTN_PIN, INPUT_PULLUP);
  pinMode(GAME_RIGHT_BTN_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  noTone(BUZZER_PIN);
  pinMode(BUZZER_GND_PIN, OUTPUT);
  digitalWrite(BUZZER_GND_PIN, LOW);
  
  // Create Mutex for stock fetching
  stockMutex = xSemaphoreCreateMutex();
  if (stockMutex == NULL) {
    Serial.println("Critical Error: Failed to create stockMutex!");
  }
  
  // Initialize MD_Parola display
  displayManager.begin();
  
  // Load configuration from NVS
  webPortal.loadSettings();
  
  // Start background task on Core 0 for network calls
  xTaskCreatePinnedToCore(
    stockFetchTask,        /* Task function */
    "StockFetchTask",      /* Name of task */
    8192,                  /* Stack size in words (large enough for SSL/HTTP client) */
    NULL,                  /* Parameter of the task */
    1,                     /* Priority of the task */
    &stockTaskHandle,      /* Task handle */
    0                      /* Pin task to Core 0 (Wi-Fi core) */
  );
  
  // Start Boot Animation: "Hello, Mr Attia"
  currentState = STATE_BOOT_ANIMATION;
  displayManager.showScrollText("Hello, Mr Attia", 60);
  Serial.println("Displaying Welcome message...");
}

// --- MAIN LOOP ---
void loop() {
  // 1. Process Buttons (Handles power and screen switching)
  handleButtons();
  
  // If the display is powered off, we clear the display and wait
  if (!displayManager.isOn()) {
    delay(100);
    return;
  }
  
  // 2. State Machine
  switch (currentState) {
    
    case STATE_BOOT_ANIMATION: {
      // Animate the welcome text. When scroll finishes once, transition
      if (displayManager.update()) {
        displayManager.clear();
        
        // Check if WiFi settings are stored
        if (webPortal.getSSID().length() > 0) {
          currentState = STATE_CONNECTING_WIFI;
        } else {
          Serial.println("No WiFi configuration found. Entering Web Setup Portal.");
          currentState = STATE_WIFI_SETUP;
          webPortal.startCaptivePortal();
        }
      }
      break;
    }
    
    case STATE_WIFI_SETUP: {
      // Let the portal handle network requests and DNS redirects
      webPortal.handleClient();
      displayManager.update(); // Keeps captive portal informational text scrolling
      break;
    }
    
    case STATE_CONNECTING_WIFI: {
      // Attempts to connect to the saved Wi-Fi SSID
      if (webPortal.connectWiFi()) {
        // Sync time from NTP server
        clockClient.begin(webPortal.getTZ());
        
        // Trigger initial stock price fetch in background task
        triggerStockFetch = true;
        lastStockFetchTime = millis();
        
        // Transition to Stock Ticker Screen
        currentState = STATE_STOCK_TICKER;
        stockScrollInit = true;
        Serial.println("Successfully connected to Wi-Fi. Switched to Stock Ticker.");
      } else {
        Serial.println("Failed to connect to configured Wi-Fi. Redirecting to Web Portal.");
        currentState = STATE_WIFI_SETUP;
        webPortal.startCaptivePortal();
      }
      break;
    }
    
    case STATE_STOCK_TICKER: {
      // Sync clock time if NTP sync is still in progress
      clockClient.checkSync();
      
      // Periodically trigger a fresh stock quotes fetch in the background
      if (millis() - lastStockFetchTime > 5 * 60 * 1000) { // Every 5 minutes
        if (!stockFetchInProgress && !triggerStockFetch) {
          triggerStockFetch = true;
          lastStockFetchTime = millis();
          Serial.println("5-minute timer elapsed. Background stock update triggered.");
        }
      }
      
      // In stock screen, we scroll the global stock ticker string
      if (stockScrollInit) {
        stockScrollInit = false;
        String currentTicker;
        if (xSemaphoreTake(stockMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
          currentTicker = globalStockTicker;
          xSemaphoreGive(stockMutex);
        } else {
          currentTicker = "Loading stock ticker...  ";
        }
        
        displayManager.showScrollText(currentTicker.c_str(), 60, 0);
      }
      
      // Animate the display. When the scroll finishes a full loop:
      if (displayManager.update()) {
        // Retrieve latest stock quote string and reload scroll to display fresh values
        String currentTicker;
        if (xSemaphoreTake(stockMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
          currentTicker = globalStockTicker;
          xSemaphoreGive(stockMutex);
        } else {
          currentTicker = "Updating Stocks...  ";
        }
        displayManager.showScrollText(currentTicker.c_str(), 60, 0);
      }
      break;
    }
    
    case STATE_CLOCK: {
      // Ensure the NTP background client sync status is maintained
      clockClient.checkSync();
      
      switch (clockSubState) {
        case CLOCK_INIT:
          clockScreen.reset();
          clockScreen.update(displayManager.getGraphicObject());
          clockSubState = CLOCK_TIME;
          break;
          
        case CLOCK_TIME:
          // Continuously run the sliding clock animation — no date scroll
          clockScreen.update(displayManager.getGraphicObject());
          break;
          
        default:
          break;
      }
      break;
    }
    
    case STATE_TIMER: {
      timerScreen.update(displayManager.getGraphicObject());
      break;
    }
    
    case STATE_GAME: {
      stackerGame.updateLogic();
      stackerGame.draw(displayManager.getGraphicObject());
      break;
    }
    
    case STATE_TETRIS: {
      tetrisGame.updateLogic(displayManager.getGraphicObject());
      tetrisGame.draw(displayManager.getGraphicObject());
      break;
    }
  }
  
  // yield to FreeRTOS background tasks
  delay(1);
}