#include <DHT.h>
#include <ESP8266WiFi.h>
#include <Wire.h>
#include "ccs811.h"
#include "MQ135.h"

// WiFi credentials
const char* ssid = "NoPass";
const char* pass = "0328880092";
const char* server = "api.thingspeak.com";
String apiKey = "7Q0KURQVYFIAM3YV";

// Pin definitions
#define DHTPIN D4
#define DHTTYPE DHT11
#define FLAME_PIN D0
#define MQ135_PIN A0
#define LED_PIN D7
#define BUZZER_PIN D8

DHT dht(DHTPIN, DHTTYPE);
CCS811 ccs811(D3); // nWAKE on D3
WiFiClient client;
MQ135 gasSensor = MQ135(MQ135_PIN);

void setup() {
  Serial.begin(115200);
  Serial.println("Initializing system...");

  // Initialize sensors
  dht.begin();
  Serial.println("initialize DHT11");

  Wire.begin(D2, D1); // SDA, SCL pins
  Serial.println("initialize CS811");

  if (!ccs811.begin()) {
    Serial.println("CCS811 initialization failed!");
    while (1);
  }

  // Initialize WiFi
  WiFi.begin(ssid, pass);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  // Initialize pins
  pinMode(FLAME_PIN, INPUT);
  pinMode(MQ135_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Start CCS811 measurement
  if (!ccs811.start(CCS811_MODE_1SEC)) {
    Serial.println("CCS811 start failed!");
    while (1);
  }

  // Allow some time for sensors to stabilize
  delay(2000);
}

void loop() {
  // Read sensors
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();
  int flame = digitalRead(FLAME_PIN);
  float air_quality = gasSensor.getPPM();
  uint16_t eco2, etvoc, errstat, raw;

  ccs811.read(&eco2, &etvoc, &errstat, &raw);

  // Check if any reads failed
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  // Print sensor values to Serial
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.print(" *C, Humidity: ");
  Serial.print(humidity);
  Serial.print(" %, Flame: ");
  Serial.print(flame);
  Serial.print(", MQ135: ");
  Serial.print(air_quality);
  Serial.print(" ppm, eCO2: ");
  Serial.print(eco2);
  Serial.print(" ppm, TVOC: ");
  Serial.print(etvoc);
  Serial.println(" ppb");

  // Fire detection logic
  if (temperature > 45 || flame == LOW || air_quality > 300 || eco2 > 2000) {
    digitalWrite(LED_PIN, HIGH);
    digitalWrite(BUZZER_PIN, HIGH);
  } else {
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
  }

  // Send data to ThingSpeak
  if (client.connect(server, 80)) {
    String postStr = apiKey;
    postStr += "&field1=" + String(temperature);
    postStr += "&field2=" + String(humidity);
    postStr += "&field3=" + String(air_quality);
    postStr += "&field4=" + String(eco2);
    postStr += "&field5=" + String(etvoc);
    postStr += "&field6=" + String(flame);
    postStr += "\r\n\r\n";

    client.print("POST /update HTTP/1.1\n");
    client.print("Host: api.thingspeak.com\n");
    client.print("Connection: close\n");
    client.print("X-THINGSPEAKAPIKEY: " + apiKey + "\n");
    client.print("Content-Type: application/x-www-form-urlencoded\n");
    client.print("Content-Length: " + String(postStr.length()) + "\n\n");
    client.print(postStr);

    Serial.println("Data sent to ThingSpeak");
  }

  client.stop();
  delay(2000); // Wait before next loop
}