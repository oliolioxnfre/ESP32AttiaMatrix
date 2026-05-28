#ifndef STOCK_CLIENT_H
#define STOCK_CLIENT_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "config.h"

class StockClient {
private:
  // Helper to split comma-separated string into a Vector of strings
  int splitString(const String& input, char delimiter, String results[], int maxResults) {
    int count = 0;
    int fromIdx = 0;
    int toIdx = -1;
    
    while (count < maxResults) {
      toIdx = input.indexOf(delimiter, fromIdx);
      if (toIdx == -1) {
        // Last element
        String sub = input.substring(fromIdx);
        sub.trim();
        if (sub.length() > 0) {
          results[count++] = sub;
        }
        break;
      } else {
        String sub = input.substring(fromIdx, toIdx);
        sub.trim();
        if (sub.length() > 0) {
          results[count++] = sub;
        }
        fromIdx = toIdx + 1;
      }
    }
    return count;
  }

  // Generates simulated/mock stock data with realistic numbers and random walk fluctuations
  String getMockStockData(const String& symbol) {
    float basePrice = 150.0;
    
    if (symbol.equalsIgnoreCase("AAPL")) basePrice = 189.50;
    else if (symbol.equalsIgnoreCase("MSFT")) basePrice = 425.20;
    else if (symbol.equalsIgnoreCase("GOOG") || symbol.equalsIgnoreCase("GOOGL")) basePrice = 175.40;
    else if (symbol.equalsIgnoreCase("TSLA")) basePrice = 178.90;
    else if (symbol.equalsIgnoreCase("NVDA")) basePrice = 950.00;
    else if (symbol.equalsIgnoreCase("AMZN")) basePrice = 180.15;
    else if (symbol.equalsIgnoreCase("BTC-USD")) basePrice = 67500.00;
    else if (symbol.equalsIgnoreCase("ETH-USD")) basePrice = 3500.00;
    else {
      // Hash-like deterministic base price for any custom stock ticker
      uint32_t hash = 0;
      for (size_t i = 0; i < symbol.length(); i++) {
        hash = symbol[i] + (hash << 6) + (hash << 16) - hash;
      }
      basePrice = (hash % 800) + 10.0;
    }

    // Use a slow-moving sine wave + random small noise to simulate real-time price fluctuations
    float timeFactor = millis() / 100000.0;
    float seed = (symbol.length() > 0) ? (float)symbol[0] : 1.0;
    float wave = sin(timeFactor + seed) * 0.015; // Max 1.5% change
    float noise = (float)(random(-100, 100)) / 10000.0; // 0.01% noise
    
    float percentChange = (wave + noise) * 100.0;
    float priceChange = basePrice * (wave + noise);
    float currentPrice = basePrice + priceChange;

    char arrow = (percentChange >= 0) ? '^' : 'v';
    String sign = (percentChange >= 0) ? "+" : "";

    String symbolUpper = symbol;
    symbolUpper.toUpperCase();

    char buffer[128];
    snprintf(buffer, sizeof(buffer), "%s: $%.2f %c%s%.2f (%.2f%%)    ", 
             symbolUpper.c_str(), currentPrice, arrow, sign.c_str(), priceChange, percentChange);
    
    return String(buffer);
  }

public:
  StockClient() {}

  // Fetches latest quotes for all configured stocks, returns combined string
  String getStocksTicker(const String& stocksList, const String& apiKey) {
    if (stocksList.length() == 0) {
      return "No Stocks Configured.  ";
    }

    String stocks[16];
    int count = splitString(stocksList, ',', stocks, 16);
    
    String finalTicker = "    "; // Starting spacer

    // If API key is empty, bypass network entirely and use mock engine
    if (apiKey.length() == 0) {
      Serial.println("Finnhub API Key is empty. Running high-fidelity mock stock engine.");
      for (int i = 0; i < count; i++) {
        finalTicker += getMockStockData(stocks[i]);
      }
      return finalTicker;
    }

    Serial.println("Fetching real-time stock prices from Finnhub...");
    WiFiClientSecure client;
    client.setInsecure(); // Disable SSL certificate verification (avoids large footprint)
    
    HTTPClient http;

    for (int i = 0; i < count; i++) {
      String symbol = stocks[i];
      symbol.toUpperCase();
      
      String url = "https://finnhub.io/api/v1/quote?symbol=" + symbol + "&token=" + apiKey;
      
      http.begin(client, url);
      int httpCode = http.GET();
      
      bool fetchSuccess = false;
      
      if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        
        // Parse JSON
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);
        
        if (!error) {
          // Verify response actually has data (c != 0)
          float currentPrice = doc["c"] | 0.0;
          float priceChange = doc["d"] | 0.0;
          float percentChange = doc["dp"] | 0.0;
          
          if (currentPrice > 0.001) {
            char arrow = (priceChange >= 0) ? '^' : 'v';
            String sign = (priceChange >= 0) ? "+" : "";
            
            char buffer[128];
            snprintf(buffer, sizeof(buffer), "%s: $%.2f %c%s%.2f (%.2f%%)    ", 
                     symbol.c_str(), currentPrice, arrow, sign.c_str(), priceChange, percentChange);
            finalTicker += String(buffer);
            fetchSuccess = true;
          }
        }
      }
      
      http.end();

      // Fall back to mock data if fetch failed for any reason (rate limits, wrong API key, network error)
      if (!fetchSuccess) {
        Serial.print("Failed to fetch real-time price for ");
        Serial.print(symbol);
        Serial.println(". Using simulated fallback.");
        finalTicker += getMockStockData(symbol);
      }
      
      // Delay briefly between HTTP requests to avoid hammering the API
      delay(150);
    }

    return finalTicker;
  }
};

extern StockClient stockClient;

#endif // STOCK_CLIENT_H
