#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

class BH1750 {
public:
  enum Mode {
    UNCONFIGURED = 0,
    CONTINUOUS_HIGH_RES_MODE = 0x10,
    CONTINUOUS_HIGH_RES_MODE_2 = 0x11,
    CONTINUOUS_LOW_RES_MODE = 0x13,
    ONE_TIME_HIGH_RES_MODE = 0x20,
    ONE_TIME_HIGH_RES_MODE_2 = 0x21,
    ONE_TIME_LOW_RES_MODE = 0x23
  };

  BH1750(byte addr = 0x23) : BH1750_I2CADDR(addr), I2C(&Wire), measurementTimeout(120) {}

  bool begin(Mode mode = CONTINUOUS_HIGH_RES_MODE) {
    return configure(mode);
  }

  bool configure(Mode mode) {
    I2C->beginTransmission(BH1750_I2CADDR);
    I2C->write((uint8_t)mode);
    if (I2C->endTransmission() == 0) {
      BH1750_MODE = mode;
      lastReadTimestamp = millis();
      return true;
    }
    return false;
  }

  void setTimeout(unsigned long timeoutMs) { measurementTimeout = timeoutMs; }

  float readLightLevel(bool nonBlocking = false) {
    unsigned long startTime = millis();
    while (!measurementReady()) {
      if (nonBlocking && millis() - startTime > measurementTimeout) return -1.0;
      vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (2 == I2C->requestFrom((int)BH1750_I2CADDR, (int)2)) {
      unsigned int level = I2C->read() << 8 | I2C->read();
      lastReadTimestamp = millis();
      return level / BH1750_CONV_FACTOR;
    }
    return -1.0;
  }

private:
  bool measurementReady() {
    unsigned long delaytime = 120;
    return (millis() - lastReadTimestamp >= delaytime);
  }

  byte BH1750_I2CADDR;
  const float BH1750_CONV_FACTOR = 1.2;
  TwoWire* I2C;
  Mode BH1750_MODE = UNCONFIGURED;
  unsigned long lastReadTimestamp = 0;
  unsigned long measurementTimeout;
};

BH1750 lightMeter(0x23);
const int systemPin = 5;
const int luxThreshold = 10;

float luxValue = 0.0;

TaskHandle_t TaskReadLuxHandle;
TaskHandle_t TaskControlSystemHandle;

void TaskReadLux(void *pvParameters) {
  for (;;) {
    luxValue = lightMeter.readLightLevel(true);
    Serial.print("Lux: ");
    Serial.println(luxValue);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void TaskControlSystem(void *pvParameters) {
  for (;;) {
    if (luxValue >= 0) { 
      if (luxValue < luxThreshold) {
        digitalWrite(systemPin, HIGH);
        Serial.println("System ON");
      } else {
        digitalWrite(systemPin, LOW);
        Serial.println("System OFF");
      }
    }
    vTaskDelay(pdMS_TO_TICKS(500)); 
  }
}

void setup() {
  Serial.begin(115200);
  
  Wire.begin(32, 33);

  lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
  lightMeter.setTimeout(200);

  pinMode(systemPin, OUTPUT);

  xTaskCreate(TaskReadLux, "TaskReadLux", 2048, NULL, 1, &TaskReadLuxHandle);
  xTaskCreate(TaskControlSystem, "TaskControlSystem", 2048, NULL, 1, &TaskControlSystemHandle);
}

void loop() {
}