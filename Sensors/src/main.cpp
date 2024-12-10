#include "freertos/FreeRTOS.h"
#include <BH1750.h>
#include <Wire.h>
#include <painlessMesh.h>

// Default I2C address (use 0x5C if scanner detects it)
BH1750 lightMeter(0x23);

painlessMesh mesh;

// Task handles
TaskHandle_t lightSensorTaskHandle = NULL;
TaskHandle_t reedSwitchTaskHandle = NULL;
TaskHandle_t meshUpdateTaskHandle;

// Pin definitions
#define REED_SWITCH_PIN 15 // GPIO pin connected to the reed switch
#define BUZZER_PIN 26      // GPIO pin connected to the buzzer

// Mesh network parameters
#define MESH_PREFIX "Kelompok21"
#define MESH_PASSWORD "bangRianGanteng"
#define MESH_PORT 5555

// Variables
float lux = 0; // Light sensor value

// Functions
void lightSensorTask(void *parameter);
void reedSwitchTask(void *parameter);
void receivedCallback(uint32_t from, String &msg);
void meshUpdateTask(void *parameter);

void setup() {
  // Initialize serial communication
  Serial.begin(115200);

  // Initialize the I2C bus
  Wire.begin(21, 22); // For ESP32: SDA = GPIO 21, SCL = GPIO 22

  // Initialize mesh network
  mesh.setDebugMsgTypes(ERROR | STARTUP | CONNECTION); // Debug message types
  mesh.init(MESH_PREFIX, MESH_PASSWORD, MESH_PORT);
  mesh.onReceive(&receivedCallback); // Register the message receive callback

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

  // Create FreeRTOS task for updating mesh
  xTaskCreate(meshUpdateTask,       // Function to implement the task
              "MeshUpdateTask",     // Task name
              8192,                 // Stack size
              NULL,                 // Task input parameter
              3,                    // Priority
              &meshUpdateTaskHandle // Task handle
  );
}

void loop() {}

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
    if (lux <= 15) {
      if (digitalRead(REED_SWITCH_PIN) == LOW) {
        digitalWrite(BUZZER_PIN, HIGH); // Turn on the buzzer
        Serial.println("Reed switch triggered!");
      } else {
        digitalWrite(BUZZER_PIN, LOW); // Turn off the buzzer
      }
    } else {
      digitalWrite(
          BUZZER_PIN,
          LOW); // Turn off the buzzer if light level is above threshold
    }
    vTaskDelay(pdMS_TO_TICKS(100)); // Delay to yield control (100ms)
  }
}

// Function to handle received messages
void receivedCallback(uint32_t from, String &msg) {
  Serial.printf("Received message from node %u: %s\n", from, msg.c_str());
}

// Task to continuously update the mesh network
void meshUpdateTask(void *pvParameters) {
  while (true) {
    mesh.update();
    vTaskDelay(10 / portTICK_PERIOD_MS); // Short delay to yield to other tasks
  }
}