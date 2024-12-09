#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

class BH1750 {
public:
  enum Mode {
    UNCONFIGURED = 0,
    CONTINUOUS_HIGH_RES_MODE = 0x10
  };

  BH1750(byte addr = 0x23) : BH1750_I2CADDR(addr), I2C(&Wire) {}

  bool begin(Mode mode = CONTINUOUS_HIGH_RES_MODE) {
    return configure(mode);
  }

private:
  bool configure(Mode mode) {
    I2C->beginTransmission(BH1750_I2CADDR);
    I2C->write((uint8_t)mode);
    return (I2C->endTransmission() == 0);
  }

  byte BH1750_I2CADDR;
  TwoWire* I2C;
};

BH1750 lightMeter(0x23);

TaskHandle_t TaskInitHandle;

void TaskInit(void *pvParameters) {
  for (;;) {
    Serial.println("Initializing BH1750 sensor...");
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

void setup() {
  Serial.begin(115200);

  Wire.begin(32, 33);

  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println("BH1750 initialized successfully");
  } else {
    Serial.println("BH1750 initialization failed");
  }

  xTaskCreate(TaskInit, "TaskInit", 2048, NULL, 1, &TaskInitHandle);
}

void loop() {
}
