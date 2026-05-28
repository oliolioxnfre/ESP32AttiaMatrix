#ifndef CLOCK_CLIENT_H
#define CLOCK_CLIENT_H

#include <Arduino.h>
#include <time.h>
#include "config.h"

class ClockClient {
private:
  bool _isSynced;

public:
  ClockClient() : _isSynced(false) {}

  void begin(const String& tzString) {
    Serial.print("Configuring NTP with Timezone: ");
    Serial.println(tzString);
    
    // Set local timezone and start NTP background client
    configTzTime(tzString.c_str(), "pool.ntp.org", "time.nist.gov", "time.google.com");
  }

  // Checks if the time has been successfully synchronized from the NTP server
  bool checkSync() {
    if (_isSynced) return true;
    
    time_t now;
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      _isSynced = true;
      Serial.print("Time successfully synced via NTP! Current time: ");
      Serial.println(asctime(&timeinfo));
      return true;
    }
    return false;
  }

  bool isSynced() const {
    return _isSynced;
  }

  // Returns a static time string like "12:34 PM" or "12 34 PM" (blinking colon)
  String getFormattedTime(bool showColon = true) {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
      return "00:00  ";
    }

    char timeBuf[32];
    if (showColon) {
      // 12-hour format with seconds: e.g. "07:05 PM"
      strftime(timeBuf, sizeof(timeBuf), "%I:%M %p", &timeinfo);
    } else {
      // Blinking colon replaced with space: e.g. "07 05 PM"
      strftime(timeBuf, sizeof(timeBuf), "%I %M %p", &timeinfo);
    }
    
    // Strip leading zero for cleaner look (e.g. " 7:05 PM" -> "7:05 PM")
    String timeStr = String(timeBuf);
    if (timeStr.startsWith("0")) {
      timeStr = timeStr.substring(1);
    }
    return timeStr;
  }

  // Returns formatted date string: e.g. "Wednesday, May 27"
  String getFormattedDate() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
      return "Loading Date...  ";
    }

    char dateBuf[64];
    // E.g., "Wednesday, May 27"
    strftime(dateBuf, sizeof(dateBuf), "%A, %B %d  ", &timeinfo);
    return String(dateBuf);
  }
};

extern ClockClient clockClient;

#endif // CLOCK_CLIENT_H
