#include "Adafruit_VL53L0X.h"
Adafruit_VL53L0X lox = Adafruit_VL53L0X();

#include <ESP8266WiFi.h>
WiFiClient espClient;

#include <PubSubClient.h>
PubSubClient client(espClient);

#include <ArduinoJson.h>
const size_t JSON_BUFFER_SIZE = 512;
StaticJsonDocument<JSON_BUFFER_SIZE> jsonBuffer;
char jsonString[512];

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Variabel untuk perhitungan rata-rata
const int numReadings = 25;  // Jumlah pengukuran untuk rata-rata
int readings[numReadings];   // Array untuk menyimpan nilai jarak
int currentIndex = 0;        // Indeks untuk array
int total = 0;               // Total nilai jarak
int count = 0;               // Counter untuk valid readings
float average;
const char* MQTT_server = "123.123.123.123";
const char* MQTT_username = "1";
const char* MQTT_password = "1";
// const char* MQTT_username = "2";
// const char* MQTT_password = "2";
const int MQTT_Port = 1883;
String WiFiSSID = "masch.internal";
String WiFiPassword = "jlsrbz20@3";

long currentMillisUpload = 0;
long previousMillisUpload = 0;
int intervalReconnectMQTT = 60000;  //waktu Reconnect MQTT
unsigned long currentMillisReconnectMQTT = 0;
long previousMillisReconnectMQTT = 0;
unsigned long CurrentWiFiReconnect = 0;
long previousWiFiReconnect = 0;

unsigned long Currentpublish = 0;
long previouspublish = 0;

void callback(String topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();
}

void ConnectMQTT() {
  client.setServer(MQTT_server, MQTT_Port);
  Serial.println("Menghubungkan ke MQTT...");
  // client.setCallback(callback);
}

void ReconnectMQTT() {
  if (!client.connected()) {

    currentMillisReconnectMQTT = millis();

    if (currentMillisReconnectMQTT - previousMillisReconnectMQTT >= intervalReconnectMQTT) {
      previousMillisReconnectMQTT = currentMillisReconnectMQTT;

      Serial.print("Menghubungkan ke MQTT...");
      client.setServer(MQTT_server, MQTT_Port);
      client.setCallback(callback);
      if (client.connect("ESP8266Client", MQTT_username, MQTT_password)) {
        Serial.println("Reconnecting MQTT berhasil!");
        // Berlangganan topic di sini

      } else {
        Serial.print("Gagal, rc=");
        Serial.print(client.state());
        Serial.println("Reconnecting MQTT Gagal!");
        Serial.println("Mencoba lagi dalam " + String(intervalReconnectMQTT) + "ms");
      }
    }
  }
}
//connection checking
void PublishMQTT() {
  currentMillisUpload = millis();

  if (WiFi.status() != WL_CONNECTED) {
    ReconnectWiFi();
  }
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      ReconnectMQTT();
    }
    client.loop();
  }
}

void displayAvrDistance() {
  //clear display
  display.clearDisplay();

  // display temperature
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.print("AVR Distance :");

  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 10);
  display.printf("%0.1f", average);//"%0.2f" di gunakan untuk menunjukan 2 desimal "%0.3f" di gunakan untuk menunjukan 3 desimal 
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.print(" cm");

  display.display();
}

void ConnectWiFi() {
  if (WiFiSSID.length() > 0 && WiFiPassword.length() > 0) {
    WiFi.begin(WiFiSSID, WiFiPassword);
    if (WiFi.waitForConnectResult() == WL_CONNECTED) {
      Serial.println("Koneksi WiFi berhasil!");
      Serial.println("IP Address: " + WiFi.localIP().toString());
    } else {
      Serial.println("Koneksi WiFi gagal!");
    }
  } else {
    Serial.println("SSID atau Password WiFi kosong.");
  }
}
void ReconnectWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    CurrentWiFiReconnect = millis();
    if (CurrentWiFiReconnect - previousWiFiReconnect >= 30000) {
      previousWiFiReconnect = CurrentWiFiReconnect;
      Serial.println("Attempting to reconnect to WiFi...");
      // WiFi.disconnect();
      // readConfig();
      WiFiSSID.trim();
      WiFiPassword.trim();
      WiFi.begin(WiFiSSID, WiFiPassword);
    }
  }
  if (WiFi.status() == WL_CONNECTED) {
    ("Reconnecting WiFi berhasil!");
    ReconnectMQTT();
  }
}


void setup() {
  Serial.begin(115200);

  // Tunggu serial port siap (khusus perangkat USB native)
  while (!Serial) {
    delay(1);
  }

  Serial.println("Adafruit VL53L0X test.");
  if (!lox.begin()) {
    Serial.println(F("Failed to boot VL53L0X"));
    while (1)
      ;
  }

  Serial.println(F("VL53L0X API Continuous Ranging example\n\n"));

  // Memulai pengukuran jarak secara kontinu
  lox.startRangeContinuous();

  //CONNECT WIFI
  ConnectWiFi();
  //CONNECT MQTT
  ConnectMQTT();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {  // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }
}

void loop() {
  PublishMQTT();
  if (lox.isRangeComplete()) {
    int distance = lox.readRange();

    // Menyimpan hasil pengukuran ke array
    total -= readings[currentIndex];    // Mengurangi nilai lama dari total
    readings[currentIndex] = distance;  // Menyimpan pengukuran baru ke array
    total += distance;                  // Menambahkan nilai baru ke total

    currentIndex++;                       // Pindah ke indeks berikutnya
    count = min(count + 1, numReadings);  // Hitung jumlah pembacaan valid

    if (currentIndex >= numReadings) {
      currentIndex = 0;  // Reset indeks ke awal
    }

    // Ubah jarak dari mm ke cm
    float distanceInCm = distance / 10.0;

    // Tampilkan rata-rata setelah 10 pembacaan
    if (count >= numReadings) {
      average = total / float(numReadings) / 10.0;  // Rata-rata dalam cm
      Serial.print("Average distance over ");
      Serial.print(numReadings);
      Serial.print(" readings: ");
      Serial.println(average);

      displayAvrDistance();
    }
  }
  // delay(100);
  if (client.connected()) {
    Currentpublish = millis();
    if (Currentpublish - previouspublish >= 1000) {
      jsonBuffer["avr_distance"] = average;
      serializeJson(jsonBuffer, jsonString);
      client.publish("TOF/publish/telemetry", jsonString);
      Serial.println("send");
      previouspublish = Currentpublish;
    }
  }
}
