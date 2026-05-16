#include "arduino_secrets.h"

#include "thingProperties.h"
#include <DHT.h>
#include "ArduinoGraphics.h"
#include "Arduino_LED_Matrix.h"

// --- Pin & Sensor Config ---
#define DHTPIN        8
#define DHTTYPE       DHT11
#define BUZZERPIN     11

// --- Timing Constants ---
#define READ_INTERVAL  2000UL   // DHT11 read every 2s
#define BUZZ_INTERVAL  100UL    // Buzzer toggle every 100ms
#define DISPLAY_SWAP   3000UL   // Swap Temp/Humidity display every 3s

// --- Objects ---
DHT            dht(DHTPIN, DHTTYPE);
ArduinoLEDMatrix matrix;

// --- State Variables ---
unsigned long  lastReadTime    = 0;
unsigned long  lastBuzzTime    = 0;
unsigned long  lastSwapTime    = 0;
bool           buzzerState     = false;
bool           lastOnline      = false;
bool           showTemp        = true;
bool           dhtError        = false;
float          lastTemp        = 0.0;
float          lastHumidity    = 0.0;
String         lastSensorStat  = "";

// ============================================================
//  Scroll a string on the 12x8 LED Matrix
// ============================================================
void scrollText(String msg) {
  matrix.beginDraw();
  matrix.stroke(0xFFFFFFFF);
  matrix.textScrollSpeed(60);
  matrix.textFont(Font_5x7);
  matrix.beginText(0, 1, 0xFFFFFF);
  matrix.println(" " + msg + " ");
  matrix.endText(SCROLL_LEFT);
  matrix.endDraw();
}

// ============================================================
//  Update SYSsensorSTAT only on actual state change
// ============================================================
void setSensorStat(String status) {
  if (status != lastSensorStat) {
    SYSsensorSTAT  = status;
    lastSensorStat = status;
    Serial.println("[SENSOR STATUS] " + status);
  }
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.begin(9600);
  delay(1500);

  // Buzzer init
  pinMode(BUZZERPIN, OUTPUT);
  digitalWrite(BUZZERPIN, LOW);

  // LED Matrix init
  matrix.begin();

  // DHT11 init
  dht.begin();

  // Arduino IoT Cloud init
  initProperties();
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);
  setDebugMessageLevel(2);
  ArduinoCloud.printDebugInfo();

  Serial.println("[SYSTEM] Boot complete.");
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
  ArduinoCloud.update();

  bool currentOnline = (bool)ArduinoCloud.connected();

  // ----------------------------------------------------------
  //  1. Online / Offline detection â update cloud on change
  // ----------------------------------------------------------
  if (currentOnline != lastOnline) {
    isOnline   = currentOnline;
    lastOnline = currentOnline;
    Serial.println(isOnline ? "[INFO] Board is ONLINE" : "[INFO] Board is OFFLINE");
  }

  // ----------------------------------------------------------
  //  2. Buzzer â non-blocking beep when offline
  // ----------------------------------------------------------
  if (!currentOnline) {
    if (millis() - lastBuzzTime >= BUZZ_INTERVAL) {
      lastBuzzTime = millis();
      buzzerState  = !buzzerState;
      digitalWrite(BUZZERPIN, buzzerState ? HIGH : LOW);
    }
  } else {
    // Ensure buzzer is silenced when back online
    if (buzzerState) {
      buzzerState = false;
      digitalWrite(BUZZERPIN, LOW);
    }
  }

  // ----------------------------------------------------------
  //  3. DHT11 Read â non-blocking, every 2 seconds
  // ----------------------------------------------------------
  if (millis() - lastReadTime >= READ_INTERVAL) {
    lastReadTime = millis();

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t) && !isnan(h)) {
      dhtError     = false;
      lastTemp     = t;
      lastHumidity = h;

      // Only push to cloud if value changed
      if (t != Temp)     Temp     = t;
      if (h != Humidity) Humidity = h;

      setSensorStat("SENSOR OPERATIONAL , Readings are being Transmitted");

      Serial.print("[DATA] Temp: ");     Serial.print(lastTemp,     1);
      Serial.print(" C | Humidity: ");  Serial.print(lastHumidity, 0);
      Serial.println(" %");

    } else {
      dhtError = true;
      setSensorStat("-!!!---------DHT11 FAILED TO RECIEVE READINGS ON CURRENT ENVIRONMENT---------!!!-");
      Serial.println("[ERROR] DHT11 read failed â check wiring on D8.");
    }
  }

  // ----------------------------------------------------------
  //  4. LED Matrix Display Logic
  //     Priority: No Internet > Sensor Error > Temp/Humidity
  // ----------------------------------------------------------
  if (!currentOnline) {
    // Priority 1 â No internet connection
    scrollText("NO NET");

  } else if (dhtError) {
    // Priority 2 â Sensor failure
    scrollText("SENSOR ERR");

  } else {
    // Priority 3 â Alternate Temp and Humidity every 3 seconds
    if (millis() - lastSwapTime >= DISPLAY_SWAP) {
      lastSwapTime = millis();
      showTemp     = !showTemp;
    }

    if (showTemp) {
      scrollText(String(lastTemp, 1) + "C");
    } else {
      scrollText(String(lastHumidity, 0) + "%");
    }
  }
}
/*
  Since SYSsensorSTAT is READ_WRITE variable, onSYSsensorSTATChange() is
  executed every time a new value is received from IoT Cloud.
*/
void onSYSsensorSTATChange()  {
  // Add your code here to act upon SYSsensorSTAT change
}