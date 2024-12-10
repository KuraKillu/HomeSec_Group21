// Initializes Blynk connection and sets up necessary libraries for WiFi,
// camera, and web server functionalities
#define BLYNK_PRINT Serial
#define BLYNK_TEMPLATE_ID "TMPL6QZa40Bzv"
#define BLYNK_TEMPLATE_NAME "HomeSec"
#define BLYNK_AUTH_TOKEN "6QLjf9huM1TwE3H6V5gGNsIwsFJtCgE5"

#include "Arduino.h"
#include "driver/rtc_io.h"
#include "esp_camera.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "img_converters.h"
#include "soc/rtc_cntl_reg.h" // Disable brownour problems
#include "soc/soc.h"          // Disable brownour problems
#include <BlynkSimpleEsp32.h>
#include <ESPAsyncWebServer.h>
#include <StringArray.h>
#include <WiFi.h>
#include <painlessMesh.h>

char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "Kura";
char pass[] = "supacabra12";

// OV2640 camera module pins (CAMERA_MODEL_AI_THINKER)
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

// Select camera model
// #define CAMERA_MODEL_WROVER_KIT
// #define CAMERA_MODEL_ESP_EYE
// #define CAMERA_MODEL_M5STACK_PSRAM
// #define CAMERA_MODEL_M5STACK_WIDE
#define CAMERA_MODEL_AI_THINKER

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Photo File Name to save in SPIFFS
#define FILE_PHOTO "/photo.jpg"
#define ledPin 4

bool checkPhoto(fs::FS &fs);
void capturePhotoSaveSpiffs(void);

boolean takeNewPhoto = false;

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { text-align: center; font-family: Arial, sans-serif; }
    img { margin-top: 20px; max-width: 90%; height: auto; }
  </style>
  <script>
    async function checkForNewPhoto() {
      try {
        const response = await fetch('/check-photo');
        const data = await response.json();
        if (data.newPhoto) {
          // Refresh the image with cache-busting
          const img = document.querySelector('img');
          img.src = `/saved-photo?t=${new Date().getTime()}`;
        }
      } catch (error) {
        console.error('Error checking for new photo:', error);
      }
    }

    // Periodically check for a new photo every 5 seconds
    setInterval(checkForNewPhoto, 5000);
  </script>
</head>
<body>
  <h2>Live Photo Viewer</h2>
  <p>The image below will update automatically when a new photo is taken.</p>
  <img src="/saved-photo" alt="Captured Photo">
</body>
</html>
)rawliteral";

void setup() {
  // Turn-off the 'brownout detector'
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  delay(100);

  // Serial port for debugging purposes
  Serial.begin(115200);
  delay(500);

  // Connect to Wi-Fi
  Blynk.begin(auth, ssid, pass);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP Address: http://");
  Serial.println(WiFi.localIP());

  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount failed");
    ESP.restart();
  }
  Serial.println("SPIFFS mounted successfully");

  // Camera Configuration
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_SVGA; // Moderate resolution
    config.jpeg_quality = 12;           // Lower quality, faster capture
    config.fb_count = 2;                // Double buffer for PSRAM
  } else {
    config.frame_size = FRAMESIZE_VGA; // Low resolution for stability
    config.jpeg_quality = 15;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    ESP.restart();
  }
  Serial.println("Camera initialized successfully");

  // Configure LED pin
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  // Start Web Server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });
  server.on("/saved-photo", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (SPIFFS.exists(FILE_PHOTO)) {
      request->send(SPIFFS, FILE_PHOTO, "image/jpeg");
    } else {
      request->send(404, "Photo not found");
    }
  });
  server.on("/check-photo", HTTP_GET, [](AsyncWebServerRequest *request) {
    String jsonResponse =
        "{\"newPhoto\":" + String(takeNewPhoto ? "true" : "false") + "}";
    takeNewPhoto = false;
    request->send(200, "application/json", jsonResponse);
  });
  server.begin();
  Serial.println("Server started");
}

void loop() {
  // Trigger photo capture every 5 seconds
  static unsigned long lastCapture = 0;
  unsigned long currentMillis = millis();

  if (currentMillis - lastCapture >= 10000) { // 5 seconds interval
    lastCapture = currentMillis;
    capturePhotoSaveSpiffs();
  }

  Blynk.run(); // Keep Blynk connection alive
}

// Check if photo capture was successful
bool checkPhoto(fs::FS &fs) {
  File f_pic = fs.open(FILE_PHOTO);
  if (!f_pic) {
    Serial.println("Failed to open photo file for verification");
    return false;
  }
  unsigned int pic_sz = f_pic.size();
  f_pic.close();
  return (pic_sz > 100); // Ensure the photo file is not empty
}

// Capture Photo and Save it to SPIFFS
void capturePhotoSaveSpiffs(void) {
  camera_fb_t *fb = NULL;

  // Turn on the LED before taking a photo
  digitalWrite(ledPin, HIGH);

  // Take a photo
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    digitalWrite(ledPin, LOW);
    return;
  }

  // Save the captured photo
  File file = SPIFFS.open(FILE_PHOTO, FILE_WRITE);
  if (file) {
    file.write(fb->buf, fb->len);
    file.close();
    takeNewPhoto = true; // Indicate a new photo is available
    Serial.println("Photo saved successfully");
  } else {
    Serial.println("Failed to save photo");
  }

  // Clean up
  esp_camera_fb_return(fb);
  digitalWrite(ledPin, LOW);

  // Verify the saved photo
  if (!checkPhoto(SPIFFS)) {
    Serial.println("Photo verification failed");
  } else {
    Serial.println("Photo verified successfully");
  }
}