#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "esp_camera.h"
#include "config.h"

//#define CAMERA_MODEL_ESP32S3_EYE // Has PSRAM

// #include "camera_pins.h"

const char* ssid = ssidd;
const char* password = passwordd;

String serveraddr = serveraddrr;
String upload_frame_url = upload_frame_urll;

// Motor 1
int motor1Pin1 = 19;
int motor1Pin2 = 20;
int enable1Pin = 21;

// Motor 2
int motor2Pin1 = 47;
int motor2Pin2 = 48;
int enable2Pin = 45;

// PINB correspondance for AVR
int PB0 = 38;
int PB1 = 39;
int PB2 = 40;

const int freq = 30000;
int dutyCycle = 0;  // PWM duty cycle for motor speed

void handleForward()
{
  Serial.println("Forward");
  digitalWrite(motor1Pin1, HIGH);
  digitalWrite(motor1Pin2, LOW);
  digitalWrite(motor2Pin1, HIGH);
  digitalWrite(motor2Pin2, LOW);

  digitalWrite(PB0, LOW);
  digitalWrite(PB1, HIGH);
  digitalWrite(PB2, LOW);
}

void handleReverse()
{
  Serial.println("Reverse");
  digitalWrite(motor1Pin1, LOW);
  digitalWrite(motor1Pin2, HIGH);
  digitalWrite(motor2Pin1, LOW);
  digitalWrite(motor2Pin2, HIGH);
 
  digitalWrite(PB0, HIGH);
  digitalWrite(PB1, HIGH);
  digitalWrite(PB2, LOW);
}

void handleLeft()
{
  Serial.println("Left");
  digitalWrite(motor1Pin1, HIGH);
  digitalWrite(motor1Pin2, LOW);
  digitalWrite(motor2Pin1, LOW);
  digitalWrite(motor2Pin2, HIGH);

  digitalWrite(PB0, LOW);
  digitalWrite(PB1, LOW);
  digitalWrite(PB2, HIGH);
}

void handleRight()
{
  Serial.println("Right");
  digitalWrite(motor1Pin1, LOW);
  digitalWrite(motor1Pin2, HIGH);
  digitalWrite(motor2Pin1, HIGH);
  digitalWrite(motor2Pin2, LOW);

  digitalWrite(PB0, HIGH);
  digitalWrite(PB1, LOW);
  digitalWrite(PB2, HIGH);
}

void handleStop()
{
  Serial.println("Stop");
  digitalWrite(motor1Pin1, LOW);
  digitalWrite(motor1Pin2, LOW);
  digitalWrite(motor2Pin1, LOW);
  digitalWrite(motor2Pin2, LOW);

  digitalWrite(PB0, LOW);
  digitalWrite(PB1, HIGH);
  digitalWrite(PB2, HIGH);
}

void processControl(String command)
{
  command.trim();

  if (command == "FORWARD")
  {
    handleForward();
  }
  else if (command == "STOP")
  {
    handleStop();
  }
  else if (command == "REVERSE")
  {
    handleReverse();
  }
  else if (command == "LEFT")
  {
    handleLeft();
  }
  else if (command == "RIGHT")
  {
    handleRight();
  }
}

void setup()
{
  Serial.begin(115200);

  // Set up motor control pins
  pinMode(motor1Pin1, OUTPUT);
  pinMode(motor1Pin2, OUTPUT);
  pinMode(motor2Pin1, OUTPUT);
  pinMode(motor2Pin2, OUTPUT);

  // Initialize PWM for motor speed control
  ledcAttach(enable1Pin, freq, 8);
  ledcAttach(enable2Pin, freq, 8);

  // Initialize PWM with 0 duty cycle
  ledcWrite(enable1Pin, 0);
  ledcWrite(enable2Pin, 0);

  // Initialize avr communication pins to 0
  digitalWrite(PB0, LOW);
  digitalWrite(PB1, LOW);
  digitalWrite(PB2, LOW);

  // Initialize the camera
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
  config.frame_size = FRAMESIZE_QVGA;
  config.pixel_format = PIXFORMAT_JPEG; // for streaming
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  // for larger pre-allocated frame buffer.
  if(psramFound())
  {
    config.jpeg_quality = 30;
    config.fb_count = 2;
    config.grab_mode = CAMERA_GRAB_LATEST;
  }
  else
  {
    // Limit the frame size when PSRAM is not available
    config.frame_size = FRAMESIZE_SVGA;
    config.fb_location = CAMERA_FB_IN_DRAM;
  }

  // Initialize the camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK)
  {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  s->set_vflip(s, 1); // flip it back
  s->set_brightness(s, 1); // up the brightness just a bit
  s->set_saturation(s, 0); // lower the saturation
  // s->set_hmirror(s, 1);
  s->set_dcw(s, 1);

  // Connect to Wi-Fi
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  digitalWrite(PB0, HIGH);
  digitalWrite(PB1, LOW);
  digitalWrite(PB2, LOW);
}

void loop()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    // Capture a frame from the camera
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb)
    {
      Serial.println("Camera capture failed");
      return;
    }

    // Send the frame to the Flask server
    HTTPClient http;
    http.begin(upload_frame_url);
    http.addHeader("Content-Type", "image/jpeg");
    int httpCode = http.POST(fb->buf, fb->len);
    if (httpCode == HTTP_CODE_OK)
    {
      //Serial.println("Frame uploaded successfully");
    }

    else
    {
      Serial.printf("Error uploading frame. HTTP code: %d\n", httpCode);
    }
    http.end();

    // Return the frame buffer to the camera
    esp_camera_fb_return(fb);

    // Fetch the current command and speed
    http.begin(serveraddr + "/control");
    httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK)
    {
      String payload = http.getString();
      //Serial.println("received control: " + payload);

      StaticJsonDocument<200> doc;  // Adjust the size as needed
      DeserializationError error = deserializeJson(doc, payload);

      if (!error)
      {
        String command = doc["command"];
        //Serial.println("Extracted command: " + command);
        processControl(command);
      }
      else
      {
        Serial.println("error in request");
      }
    }

    else
    {
      Serial.println("Error in speed request. Code: " + String(httpCode));
    }

    http.end();

    http.begin(serveraddr + "/speed");
    httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK)
    {
      String payload = http.getString();
      //Serial.println("Received speed payload: " + payload);

      // Parse the JSON payload
      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, payload);

      if (!error)
      {
        int speed = doc["speed"];
        Serial.println("Extracted speed: " + String(speed));

        // Map speed to PWM duty cycle (0-100% to 0-255)
        dutyCycle = map(speed, 0, 100, 155, 255);
        //Serial.println("Calculated duty cycle: " + String(dutyCycle));

        // Set motor speed using PWM
        ledcWrite(enable1Pin, dutyCycle);  // Update PWM for enable1Pin
        ledcWrite(enable2Pin, dutyCycle);  // Update PWM for enable2Pin
        //Serial.println("Motor speed set to " + String(speed) + "%");
      }
    }

    else
    {
      Serial.println("Error in speed request. Code: " + String(httpCode));
    }
    http.end();
  }

  //delay(100);  // Adjust the delay as needed
}
