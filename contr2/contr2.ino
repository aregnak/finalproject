#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "esp_camera.h"

//#define CAMERA_MODEL_ESP32S3_EYE // Has PSRAM

// #include "camera_pins.h"

// Replace with your network credentials
const char* ssid = "NOKIA-4D01";
const char* password = "9LqFFSXEZb";
// const char* ssid = "mystery";
// const char* password = "electrotech";

// const char* serveraddr = "http://192.168.18.14:4444/control";
// const char* speedaddr = "http://192.168.18.14:4444/speed";

//const char* upload_frame_url = "http://192.168.1.110:4444/upload_frame";


const char* serveraddr = "http://192.168.18.14:4440/control";
const char* speedaddr = "http://192.168.18.14:4440/speed";

// Motor 1
int motor1Pin1 = 19; 
int motor1Pin2 = 20; 
int enable1Pin = 21;

// Motor 2
int motor2Pin1 = 47; 
int motor2Pin2 = 48; 
int enable2Pin = 45;

// Setting PWM properties
const int freq = 30000;
//const int resolution = 8;
int dutyCycle = 0;

void handleForward() {
  Serial.println("Forward !!!!");
  digitalWrite(2, HIGH);
  digitalWrite(motor1Pin1, LOW);
  digitalWrite(motor1Pin2, HIGH); 
  digitalWrite(motor2Pin1, LOW);
  digitalWrite(motor2Pin2, HIGH);
}

void handleLeft() {
  Serial.println("Left ^^^^^");
  digitalWrite(2, LOW);
  digitalWrite(motor1Pin1, LOW); 
  digitalWrite(motor1Pin2, LOW); 
  digitalWrite(motor2Pin1, LOW);
  digitalWrite(motor2Pin2, HIGH);
}

void handleStop() {
  Serial.println("Stop &&&&&&");
  digitalWrite(motor1Pin1, LOW); 
  digitalWrite(motor1Pin2, LOW); 
  digitalWrite(motor2Pin1, LOW);
  digitalWrite(motor2Pin2, LOW);   
}

void handleRight() {
  Serial.println("Right ######");
  digitalWrite(motor1Pin1, LOW); 
  digitalWrite(motor1Pin2, HIGH); 
  digitalWrite(motor2Pin1, LOW);
  digitalWrite(motor2Pin2, LOW);    
}

void handleReverse() {
  Serial.println("Reverse @@@@@@");
  digitalWrite(motor1Pin1, HIGH);
  digitalWrite(motor1Pin2, LOW); 
  digitalWrite(motor2Pin1, HIGH);
  digitalWrite(motor2Pin2, LOW);          
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

void setup() {
  Serial.begin(115200);

  // Set the Motor pins as outputs
  pinMode(motor1Pin1, OUTPUT);
  pinMode(motor1Pin2, OUTPUT);
  pinMode(motor2Pin1, OUTPUT);
  pinMode(motor2Pin2, OUTPUT);

  // Configure PWM Pins
  ledcAttach(enable1Pin, freq, 8);
  ledcAttach(enable2Pin, freq, 8);
    
  // Initialize PWM with 0 duty cycle
  ledcWrite(enable1Pin, 0);
  ledcWrite(enable2Pin, 0);
  
  pinMode(2, OUTPUT);

  // Connect to Wi-Fi
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());


  Serial.println("connected");
}

void loop() 
{
  // even if the webserver is not active while the esp32 is,
  // the esp can connect to the webserver
  // if (WiFi.status() == WL_CONNECTED)
  // {
  //    camera_fb_t *fb = esp_camera_fb_get();
  //   if (!fb) {
  //     Serial.println("Camera capture failed");
  //     return;
  //   }

  //   // Send the frame to the Flask server
     HTTPClient http;
  //   http.begin(upload_frame_url);
  //   http.addHeader("Content-Type", "image/jpeg");
  //   int httpCode = http.POST(fb->buf, fb->len);
  //   if (httpCode == HTTP_CODE_OK) {
  //     Serial.println("Frame uploaded successfully");
  //   } else {
  //     Serial.println("Error uploading frame");
  //   }
  //   http.end();

  //   // Return the frame buffer to the camera
  //   esp_camera_fb_return(fb);
    
  //   //HTTPClient http;
    http.begin(serveraddr);

    int httpCode = http.GET();
  
    if (httpCode == HTTP_CODE_OK)
    {
      String payload = http.getString();
      Serial.println("received control: " + payload);

      StaticJsonDocument<200> doc;  // Adjust the size as needed
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) 
      {
        String command = doc["command"];
        Serial.println("Extracted command: " + command);
        processControl(command);
      }
      else 
      {
        Serial.println("error in request");
      }
    }

    http.end();

    // Fetch the current speed
    http.begin(speedaddr);
    httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.println("Received speed payload: " + payload);

      // Parse the JSON payload
      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        int speed = doc["speed"];
        Serial.println("Extracted speed: " + String(speed));

        // Map speed to PWM duty cycle (0-100% to 0-255)
        dutyCycle = map(speed, 0, 100, 160, 255);
        Serial.println("Calculated duty cycle: " + String(dutyCycle));

        // Set motor speed using PWM
        ledcWrite(enable1Pin, dutyCycle);  // Update PWM for enable1Pin
        ledcWrite(enable2Pin, dutyCycle);  // Update PWM for enable2Pin
        Serial.println("Motor speed set to " + String(speed) + "%");
      }
    }

    else 
    {
      Serial.println("Error in speed request. Code: " + String(httpCode));
    }

    http.end();
    delay(100);
  }

  // else
  // {
  //   Serial.println("WiFi disconnected.");
  // }
//}
