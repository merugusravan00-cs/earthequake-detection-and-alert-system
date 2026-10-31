
#include <Wire.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include <math.h>

// ESP32 PIN CONFIGURATION
#define SDA_PIN 21
#define SCL_PIN 22
#define LED_PIN 2
#define BUZZER_PIN 4

// ENTER YOUR OWN HOTSPOT DETAILS
const char* WIFI_SSID = "NOTHING";
const char* WIFI_PASSWORD = "123456789";

// ENTER YOUR OWN TELEGRAM DETAILS
const char* BOT_TOKEN = "9146675480:AAFeDG3s6uSM3pmeGP9d5uGFt34hlBLZl98";
const char* CHAT_ID = "7544437015";

// DETECTION SETTINGS
const float VIBRATION_THRESHOLD = 0.10;
const int REQUIRED_SAMPLES = 3;
const unsigned long ALERT_COOLDOWN = 30000;
const unsigned long WIFI_RETRY_INTERVAL = 10000;

Adafruit_ADXL345_Unified accel =
    Adafruit_ADXL345_Unified(12345);

int vibrationCount = 0;
unsigned long lastAlertTime = 0;
unsigned long lastWiFiAttempt = 0;
bool alertSent = false;

// SEND A TELEGRAM MESSAGE
bool sendTelegramMessage(const String& message) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Telegram failed: Wi-Fi is disconnected.");
    return false;
  }

  WiFiClientSecure client;

  // Suitable for a classroom demonstration only.
  // This disables server certificate verification.
  client.setInsecure();

  HTTPClient https;

  String url = "https://api.telegram.org/bot";
  url += BOT_TOKEN;
  url += "/sendMessage";

  if (!https.begin(client, url)) {
    Serial.println("Could not start HTTPS connection.");
    return false;
  }

  https.setTimeout(10000);
  https.addHeader("Content-Type", "application/json");

  // Escape quotation marks and backslashes in the message.
  String escapedMessage;
  for (unsigned int i = 0; i < message.length(); i++) {
    char c = message[i];

    if (c == '"' || c == '\\') {
      escapedMessage += '\\';
    }

    if (c == '\n') {
      escapedMessage += "\\n";
    } else {
      escapedMessage += c;
    }
  }

  String payload = "{\"chat_id\":\"";
  payload += CHAT_ID;
  payload += "\",\"text\":\"";
  payload += escapedMessage;
  payload += "\"}";

  int httpCode = https.POST(payload);

  Serial.print("Telegram HTTP status: ");
  Serial.println(httpCode);

  String response = https.getString();
  Serial.println(response);

  https.end();

  if (httpCode == 200) {
    Serial.println("Telegram message sent successfully.");
    return true;
  }

  Serial.println("Telegram message was not sent.");
  return false;
}

// CONNECT TO THE PHONE HOTSPOT
bool connectWiFi() {
  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startTime = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - startTime < 20000) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Wi-Fi connected!");
    Serial.print("ESP32 IP address: ");
    Serial.println(WiFi.localIP());
    return true;
  }

  Serial.println("Wi-Fi connection failed.");
  Serial.println("Check SSID, password, and 2.4 GHz hotspot.");
  return false;
}

// RECONNECT WHEN WI-FI IS LOST
void maintainWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  if (millis() - lastWiFiAttempt >= WIFI_RETRY_INTERVAL) {
    lastWiFiAttempt = millis();

    Serial.println("Wi-Fi disconnected. Reconnecting...");
    WiFi.disconnect();
    delay(200);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
}

// INITIALIZE THE SYSTEM
void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  Wire.begin(SDA_PIN, SCL_PIN);

  Serial.println();
  Serial.println("==================================");
  Serial.println("IoT Vibration Detection System");
  Serial.println("==================================");

  // Check the ADXL345 sensor
  if (!accel.begin()) {
    Serial.println("ERROR: ADXL345 not detected!");
    Serial.println("Check SDA, SCL, power, CS and SDO.");

    while (true) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      delay(300);
    }
  }

  Serial.println("ADXL345 detected successfully.");

  accel.setRange(ADXL345_RANGE_2_G);

  // Connect to Wi-Fi
  bool connected = connectWiFi();

  if (connected) {
    sendTelegramMessage(
      "System online: ESP32 and ADXL345 vibration monitor started."
    );
  }

  lastWiFiAttempt = millis();

  Serial.println("System ready.");
  Serial.println("Gently tap the sensor surface to test.");
}

// READ SENSOR AND DETECT VIBRATION
void loop() {
  maintainWiFi();

  sensors_event_t event;
  accel.getEvent(&event);

  // Convert acceleration to units of g
  float x = event.acceleration.x / 9.80665;
  float y = event.acceleration.y / 9.80665;
  float z = event.acceleration.z / 9.80665;

  // Calculate total acceleration
  float magnitude = sqrt(x * x + y * y + z * z);

  // Approximate acceleration change from gravity
  float vibration = fabs(magnitude - 1.0);

  Serial.print("X: ");
  Serial.print(x, 3);
  Serial.print(" g | Y: ");
  Serial.print(y, 3);
  Serial.print(" g | Z: ");
  Serial.print(z, 3);
  Serial.print(" g | Vibration: ");
  Serial.print(vibration, 3);
  Serial.println(" g");

  if (vibration >= VIBRATION_THRESHOLD) {
    vibrationCount++;

    Serial.print("Vibration sample: ");
    Serial.println(vibrationCount);

    if (vibrationCount >= REQUIRED_SAMPLES) {
      Serial.println("VIBRATION ALERT!");

      digitalWrite(LED_PIN, HIGH);
      digitalWrite(BUZZER_PIN, HIGH);

      unsigned long now = millis();

      if (!alertSent ||
          now - lastAlertTime >= ALERT_COOLDOWN) {

        bool sent = sendTelegramMessage(
          "ALERT: Vibration detected by the ADXL345 sensor. Check the location."
        );

        if (sent) {
          alertSent = true;
          lastAlertTime = millis();
        }
      }

      delay(1500);

      digitalWrite(LED_PIN, LOW);
      digitalWrite(BUZZER_PIN, LOW);

      vibrationCount = 0;
    }
  } else {
    vibrationCount = 0;
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
  }

  delay(100);
}