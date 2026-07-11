#ifndef WEB_PORTAL_H
#define WEB_PORTAL_H

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include "config.h"
#include "display_manager.h"

class WebPortal {
private:
  WebServer _server;
  DNSServer _dnsServer;
  bool _isPortalActive;
  
  String _savedSSID;
  String _savedPass;
  String _savedStocks;
  String _savedApiKey;
  String _savedTZ;
  
  // HTML templates
  String getPortalHTML(int networkCount) {
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>ESP32 Matrix Ticker Configuration</title>
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;400;600&display=swap" rel="stylesheet">
    <style>
        :root {
            --bg-grad-1: #0f172a;
            --bg-grad-2: #1e1b4b;
            --glass-bg: rgba(255, 255, 255, 0.05);
            --glass-border: rgba(255, 255, 255, 0.1);
            --neon-blue: #38bdf8;
            --neon-purple: #a855f7;
            --text-light: #f8fafc;
            --text-muted: #94a3b8;
        }
        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
            font-family: 'Inter', sans-serif;
        }
        body {
            background: linear-gradient(135deg, var(--bg-grad-1), var(--bg-grad-2));
            min-height: 100vh;
            color: var(--text-light);
            display: flex;
            align-items: center;
            justify-content: center;
            padding: 20px;
        }
        .container {
            background: var(--glass-bg);
            backdrop-filter: blur(16px);
            -webkit-backdrop-filter: blur(16px);
            border: 1px solid var(--glass-border);
            border-radius: 20px;
            padding: 30px;
            width: 100%;
            max-width: 480px;
            box-shadow: 0 10px 30px rgba(0, 0, 0, 0.5);
            animation: fadeIn 0.8s ease-out;
        }
        @keyframes fadeIn {
            from { opacity: 0; transform: translateY(20px); }
            to { opacity: 1; transform: translateY(0); }
        }
        h1 {
            font-size: 1.8rem;
            font-weight: 600;
            text-align: center;
            margin-bottom: 8px;
            background: linear-gradient(to right, var(--neon-blue), var(--neon-purple));
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
        }
        .subtitle {
            font-size: 0.9rem;
            color: var(--text-muted);
            text-align: center;
            margin-bottom: 25px;
        }
        .form-group {
            margin-bottom: 20px;
        }
        label {
            display: block;
            font-size: 0.85rem;
            font-weight: 600;
            margin-bottom: 8px;
            color: var(--text-muted);
            text-transform: uppercase;
            letter-spacing: 0.05em;
        }
        input, select {
            width: 100%;
            padding: 12px 16px;
            background: rgba(0, 0, 0, 0.3);
            border: 1px solid var(--glass-border);
            border-radius: 10px;
            color: var(--text-light);
            font-size: 0.95rem;
            transition: all 0.3s ease;
        }
        input:focus, select:focus {
            outline: none;
            border-color: var(--neon-blue);
            box-shadow: 0 0 10px rgba(56, 189, 248, 0.2);
        }
        .select-wrapper {
            position: relative;
            margin-bottom: 10px;
        }
        select option {
            background-color: #0f172a;
            color: #fff;
        }
        .btn {
            width: 100%;
            padding: 14px;
            background: linear-gradient(to right, var(--neon-blue), var(--neon-purple));
            border: none;
            border-radius: 10px;
            color: #fff;
            font-size: 1rem;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.3s ease;
            box-shadow: 0 4px 15px rgba(168, 85, 247, 0.4);
            margin-top: 10px;
        }
        .btn:hover {
            transform: translateY(-2px);
            box-shadow: 0 6px 20px rgba(56, 189, 248, 0.6);
        }
        .btn:active {
            transform: translateY(1px);
        }
        .info-box {
            background: rgba(56, 189, 248, 0.1);
            border: 1px solid rgba(56, 189, 248, 0.2);
            border-radius: 10px;
            padding: 12px;
            font-size: 0.8rem;
            line-height: 1.4;
            color: var(--neon-blue);
            margin-top: 15px;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>Matrix Setup Portal</h1>
        <p class="subtitle">ESP32-S3 Stock & Clock Display</p>
        
        <form method="POST" action="/save">
            <div class="form-group">
                <label for="wifi_select">Available Networks</label>
                <div class="select-wrapper">
                    <select id="wifi_select" onchange="if(this.value) document.getElementById('ssid').value = this.value">
                        <option value="">-- Select a nearby Wi-Fi network --</option>
)rawliteral";

    for (int i = 0; i < networkCount; ++i) {
      String ssid = WiFi.SSID(i);
      int rssi = WiFi.RSSI(i);
      String encType = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "Open" : "Secured";
      html += "<option value=\"" + ssid + "\">" + ssid + " (" + rssi + " dBm - " + encType + ")</option>\n";
    }

    html += R"rawliteral(
                    </select>
                </div>
                <input type="text" id="ssid" name="ssid" placeholder="Or enter SSID manually" required value=")rawliteral" + _savedSSID + R"rawliteral(">
            </div>
            
            <div class="form-group">
                <label for="password">Wi-Fi Password</label>
                <input type="password" id="password" name="password" placeholder="Enter password" value=")rawliteral" + _savedPass + R"rawliteral(">
            </div>
            
            <div class="form-group">
                <label for="stocks">Stock Tickers (comma-separated)</label>
                <input type="text" id="stocks" name="stocks" placeholder="e.g. AAPL,MSFT,TSLA,BTC-USD" required value=")rawliteral" + _savedStocks + R"rawliteral(">
            </div>

            <div class="form-group">
                <label for="api_key">Finnhub API Key</label>
                <input type="text" id="api_key" name="api_key" placeholder="Enter Finnhub API Key" value=")rawliteral" + (_savedApiKey.length() > 0 ? _savedApiKey : String(DEFAULT_API_KEY)) + R"rawliteral(">
            </div>

            <div class="form-group">
                <label for="timezone">Timezone</label>
                <select id="timezone" name="timezone">
                    <option value="EST5EDT,M3.2.0,M11.1.0" )rawliteral" + (_savedTZ == "EST5EDT,M3.2.0,M11.1.0" ? "selected" : "") + R"rawliteral(">US Eastern Time (EST/EDT)</option>
                    <option value="CST6CDT,M3.2.0,M11.1.0" )rawliteral" + (_savedTZ == "CST6CDT,M3.2.0,M11.1.0" ? "selected" : "") + R"rawliteral(">US Central Time (CST/CDT)</option>
                    <option value="MST7MDT,M3.2.0,M11.1.0" )rawliteral" + (_savedTZ == "MST7MDT,M3.2.0,M11.1.0" ? "selected" : "") + R"rawliteral(">US Mountain Time (MST/MDT)</option>
                    <option value="PST8PDT,M3.2.0,M11.1.0" )rawliteral" + (_savedTZ == "PST8PDT,M3.2.0,M11.1.0" ? "selected" : "") + R"rawliteral(">US Pacific Time (PST/PDT)</option>
                    <option value="GMT0" )rawliteral" + (_savedTZ == "GMT0" ? "selected" : "") + R"rawliteral(">UTC / GMT</option>
                    <option value="GMT0BST,M3.5.0/2,M10.5.0/2" )rawliteral" + (_savedTZ == "GMT0BST,M3.5.0/2,M10.5.0/2" ? "selected" : "") + R"rawliteral(">London / UK Time</option>
                    <option value="CET1CEST,M3.5.0,M10.5.0/3" )rawliteral" + (_savedTZ == "CET1CEST,M3.5.0,M10.5.0/3" ? "selected" : "") + R"rawliteral(">Central European Time (CET/CEST)</option>
                    <option value="EET-2EEST,M4.5.5/0,M10.5.4/24" )rawliteral" + (_savedTZ == "EET-2EEST,M4.5.5/0,M10.5.4/24" ? "selected" : "") + R"rawliteral(">Cairo / Egypt Time</option>
                    <option value="JST-9" )rawliteral" + (_savedTZ == "JST-9" ? "selected" : "") + R"rawliteral(">Japan Standard Time (JST)</option>
                    <option value="AEST-10AEDT,M10.1.0,A4.1.0" )rawliteral" + (_savedTZ == "AEST-10AEDT,M10.1.0,A4.1.0" ? "selected" : "") + R"rawliteral(">Sydney / Melbourne Time</option>
                </select>
            </div>
            
            <button type="submit" class="btn">Connect & Save</button>
        </form>
        
        <div class="info-box">
            Get a free real-time Stock API key at <strong>finnhub.io</strong>. If left blank, some functionalities might be rate-limited.
        </div>
    </div>
</body>
</html>
)rawliteral";
    return html;
  }

  String getSuccessHTML() {
    return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Configuration Saved!</title>
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;400;600&display=swap" rel="stylesheet">
    <style>
        :root {
            --bg-grad-1: #0f172a;
            --bg-grad-2: #1e1b4b;
            --glass-bg: rgba(255, 255, 255, 0.05);
            --glass-border: rgba(255, 255, 255, 0.1);
            --neon-blue: #38bdf8;
            --neon-purple: #a855f7;
            --text-light: #f8fafc;
            --text-muted: #94a3b8;
        }
        body {
            background: linear-gradient(135deg, var(--bg-grad-1), var(--bg-grad-2));
            min-height: 100vh;
            color: var(--text-light);
            display: flex;
            align-items: center;
            justify-content: center;
            padding: 20px;
            font-family: 'Inter', sans-serif;
            text-align: center;
        }
        .container {
            background: var(--glass-bg);
            backdrop-filter: blur(16px);
            border: 1px solid var(--glass-border);
            border-radius: 20px;
            padding: 40px 30px;
            width: 100%;
            max-width: 450px;
            box-shadow: 0 10px 30px rgba(0, 0, 0, 0.5);
        }
        h1 {
            color: var(--neon-blue);
            margin-bottom: 15px;
            font-size: 1.8rem;
        }
        p {
            font-size: 1rem;
            color: var(--text-muted);
            line-height: 1.6;
            margin-bottom: 25px;
        }
        .checkmark {
            font-size: 4rem;
            color: #22c55e;
            margin-bottom: 20px;
            animation: bounce 1.2s ease-in-out infinite alternate;
        }
        @keyframes bounce {
            from { transform: translateY(0); }
            to { transform: translateY(-8px); }
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="checkmark">&#10004;</div>
        <h1>Settings Saved Successfully!</h1>
        <p>The ESP32 is now saving your preferences and attempting to connect to your Wi-Fi network. <br><br>The display will show the connection status shortly.</p>
    </div>
</body>
</html>
)rawliteral";
  }

public:
  WebPortal() : _server(80), _isPortalActive(false) {}

  void loadSettings() {
    Preferences prefs;
    prefs.begin(PREF_NAMESPACE, true);
    _savedSSID = prefs.getString(PREF_SSID, "");
    _savedPass = prefs.getString(PREF_PASS, "");
    _savedStocks = prefs.getString(PREF_STOCKS, DEFAULT_STOCKS);
    _savedApiKey = prefs.getString(PREF_API_KEY, DEFAULT_API_KEY);
    _savedTZ = prefs.getString(PREF_TZ, DEFAULT_TZ);
    prefs.end();
    
    Serial.println("Loaded Settings from NVS:");
    Serial.println("SSID: " + _savedSSID);
    Serial.println("Stocks: " + _savedStocks);
    Serial.println("Timezone: " + _savedTZ);
  }

  void saveSettings(const String& ssid, const String& pass, const String& stocks, const String& apiKey, const String& tz) {
    Preferences prefs;
    prefs.begin(PREF_NAMESPACE, false);
    prefs.putString(PREF_SSID, ssid);
    prefs.putString(PREF_PASS, pass);
    prefs.putString(PREF_STOCKS, stocks);
    prefs.putString(PREF_API_KEY, apiKey);
    prefs.putString(PREF_TZ, tz);
    prefs.end();
    
    _savedSSID = ssid;
    _savedPass = pass;
    _savedStocks = stocks;
    _savedApiKey = apiKey;
    _savedTZ = tz;
    
    Serial.println("Saved Settings to NVS!");
  }

  // Erases the stored Wi-Fi settings so that setup portal starts on reboot
  void resetWiFiSettings() {
    Preferences prefs;
    prefs.begin(PREF_NAMESPACE, false);
    prefs.remove(PREF_SSID);
    prefs.remove(PREF_PASS);
    prefs.end();
    Serial.println("Erased WiFi Settings.");
  }

  String getSSID() const { return _savedSSID; }
  String getPassword() const { return _savedPass; }
  String getStocks() const { return _savedStocks; }
  String getApiKey() const { return _savedApiKey; }
  String getTZ() const { return _savedTZ; }

  // Starts softAP and DNSServer to capture requests
  void startCaptivePortal() {
    Serial.println("Starting Wi-Fi scan for portal list...");
    
    // Quick WiFi scan in STA mode
    WiFi.mode(WIFI_AP_STA);
    WiFi.disconnect();
    delay(100);
    int networkCount = WiFi.scanNetworks();
    
    Serial.print("Scan complete. Found networks: ");
    Serial.println(networkCount);
    
    // Start AP
    WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
    bool apSuccess = WiFi.softAP(AP_SSID);
    
    if (apSuccess) {
      Serial.print("Access Point started successfully. SSID: ");
      Serial.println(AP_SSID);
      Serial.print("Connect to AP and open: http://");
      Serial.println(WiFi.softAPIP());
    } else {
      Serial.println("Failed to start softAP.");
    }
    
    // Start DNS server on port 53 (redirect all queries to softAP IP)
    _dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    _dnsServer.start(53, "*", AP_IP);
    
    // WebServer handlers
    _server.on("/", [this, networkCount]() {
      _server.send(200, "text/html", getPortalHTML(networkCount));
    });
    
    _server.on("/save", HTTP_POST, [this]() {
      String ssid = _server.arg("ssid");
      String pass = _server.arg("password");
      String stocks = _server.arg("stocks");
      String apiKey = _server.arg("api_key");
      String tz = _server.arg("timezone");
      
      // Trim values
      ssid.trim();
      pass.trim();
      stocks.trim();
      apiKey.trim();
      tz.trim();
      
      saveSettings(ssid, pass, stocks, apiKey, tz);
      
      _server.send(200, "text/html", getSuccessHTML());
      
      // Delay slightly so response is fully delivered, then reboot!
      delay(2000);
      ESP.restart();
    });
    
    // Captive Portal Redirect Handler
    _server.onNotFound([this]() {
      // Redirect all requests to the root index of portal
      _server.sendHeader("Location", "http://192.168.4.1/", true);
      _server.send(302, "text/plain", "");
    });
    
    _server.begin();
    _isPortalActive = true;
    
    displayManager.showScrollText("WiFi Setup Mode! Connect Wifi to Attia-Matrix!", 60);
  }

  void handleClient() {
    if (_isPortalActive) {
      _dnsServer.processNextRequest();
      _server.handleClient();
    }
  }

  // Tries to connect to the saved SSID and password.
  // Returns true on success, false on failure / timeout.
  bool connectWiFi(uint32_t timeoutMs = 15000) {
    if (_savedSSID.length() == 0) {
      Serial.println("No SSID saved in Preferences.");
      return false;
    }
    
    Serial.print("Connecting to Wi-Fi: ");
    Serial.println(_savedSSID);
    
    String statusMsg = "Connecting: " + _savedSSID;
    displayManager.showScrollText(statusMsg.c_str(), 60);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(_savedSSID.c_str(), _savedPass.c_str());
    
    uint32_t startTime = millis();
    while (WiFi.status() != WL_CONNECTED) {
      delay(300);
      displayManager.update(); // Keep the scrolling animation alive while connecting!
      
      if (millis() - startTime > timeoutMs) {
        Serial.println("\nWi-Fi connection timed out.");
        WiFi.disconnect();
        return false;
      }
      Serial.print(".");
    }
    
    Serial.println("\nWi-Fi Connected successfully!");
    Serial.print("Local IP Address: ");
    Serial.println(WiFi.localIP());
    
    String successMsg = "Connected! IP: " + WiFi.localIP().toString();
    displayManager.showScrollText(successMsg.c_str(), 60);
    
    // Keep showing the IP address scroll until it completes once
    uint32_t waitTime = millis();
    while (millis() - waitTime < 6000) {
      displayManager.update();
      delay(10);
    }
    
    return true;
  }
  
  bool isPortalActive() const { return _isPortalActive; }
};

extern WebPortal webPortal;

#endif // WEB_PORTAL_H
