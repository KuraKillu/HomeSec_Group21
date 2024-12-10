#include "freertos/FreeRTOS.h"
#include <BH1750.h>
#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>

// Default I2C address (use 0x5C if scanner detects it)
BH1750 lightMeter(0x23);

// Task handles
TaskHandle_t lightSensorTaskHandle = NULL;
TaskHandle_t reedSwitchTaskHandle = NULL;

// Pin definitions
#define REED_SWITCH_PIN 15 // GPIO pin connected to the reed switch
#define BUZZER_PIN 26      // GPIO pin connected to the buzzer

// Variables
float lux = 0; // Light sensor value

// WiFi credentials
const char* ssid = "your_SSID";
const char* password = "your_PASSWORD";

// MQTT broker details
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* light_topic = "home/sensors/light";
const char* reed_topic = "home/sensors/reed";

// MQTT client
WiFiClient espClient;
PubSubClient client(espClient);

// Functions
void lightSensorTask(void *parameter);
void reedSwitchTask(void *parameter);
void setup_wifi();
void reconnect();

void setup() {
  // Initialize serial communication
  Serial.begin(115200);

  // Initialize the I2C bus
  Wire.begin(21, 22); // For ESP32: SDA = GPIO 21, SCL = GPIO 22

  // Create the FreeRTOS task for light sensor reading
  xTaskCreate(lightSensorTask,       // Task function
              "Light Sensor Task",   // Name of the task
              2048,                  // Stack size (in words)
              NULL,                  // Task input parameter
              2,                     // Priority
              &lightSensorTaskHandle // Task handle
  );

  // Create the reed switch task
  xTaskCreate(reedSwitchTask,        // Task function
              "Reed Switch Monitor", // Name of the task
              1024,                  // Stack size (in words)
              NULL,                  // Task input parameter
              1,                     // Priority
              &reedSwitchTaskHandle  // Task handle
  );

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  // Leave empty as FreeRTOS tasks handle everything
}

// Task to read the light sensor
void lightSensorTask(void *parameter) {
  // Initialize the BH1750 sensor
  if (!lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println("Error initializing BH1750. Check wiring or I2C address.");
    while (1) {
      vTaskDelay(
          pdMS_TO_TICKS(1000)); // Halt task execution if initialization fails
    }
  } else {
    Serial.println(F("BH1750 is configured and ready."));
  }

  // Continuous light level reading
  while (1) {
    lux = lightMeter.readLightLevel();
    if (lux < 0) {
      Serial.println("Error reading light level!");
    } else {
      Serial.print("Light: ");
      Serial.print(lux);
      Serial.println(" lx");
      client.publish(light_topic, String(lux).c_str());
    }
    vTaskDelay(pdMS_TO_TICKS(1000)); // Delay 1 second
  }
}

// Task to monitor reed switch
void reedSwitchTask(void *parameter) {
  pinMode(REED_SWITCH_PIN, INPUT); // Reed switch uses pull-up configuration
  pinMode(BUZZER_PIN, OUTPUT);

  while (1) {
    // Check if light level is below the threshold
    if (lux <= 15 ) {
      if (digitalRead(REED_SWITCH_PIN) == LOW) {
        digitalWrite(BUZZER_PIN, HIGH); // Turn on the buzzer
        Serial.println("Reed switch triggered!");
        client.publish(reed_topic, "triggered");
      } else {
        digitalWrite(BUZZER_PIN, LOW); // Turn off the buzzer
        client.publish(reed_topic, "not triggered");
      }
    } else {
      digitalWrite(
          BUZZER_PIN,
          LOW); // Turn off the buzzer if light level is above threshold
    }
    vTaskDelay(pdMS_TO_TICKS(100)); // Delay to yield control (100ms)
  }
}

// Function to connect to WiFi
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

// Function to connect to MQTT broker
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32Client")) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}