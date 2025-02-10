#include <WiFi.h>
#include <HTTPClient.h>

// Replace with your network credentials
//const char* ssid = "NOKIA-4D01";
//const char* password = "9LqFFSXEZb";
const char* ssid = "mystery";
const char* password = "electrotech";

const char* serveraddr = "192.168.1.110:4444";


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
const int resolution = 8;
int dutyCycle = 0;

String valueString = String(0);

void handleRoot() {
}

void handleForward() {
  Serial.println("Forward");
  digitalWrite(motor1Pin1, LOW);
  digitalWrite(motor1Pin2, HIGH); 
  digitalWrite(motor2Pin1, LOW);
  digitalWrite(motor2Pin2, HIGH);
}

void handleLeft() {
  Serial.println("Left");
  digitalWrite(motor1Pin1, LOW); 
  digitalWrite(motor1Pin2, LOW); 
  digitalWrite(motor2Pin1, LOW);
  digitalWrite(motor2Pin2, HIGH);
}

void handleStop() {
  Serial.println("Stop");
  digitalWrite(motor1Pin1, LOW); 
  digitalWrite(motor1Pin2, LOW); 
  digitalWrite(motor2Pin1, LOW);
  digitalWrite(motor2Pin2, LOW);   
}

void handleRight() {
  Serial.println("Right");
  digitalWrite(motor1Pin1, LOW); 
  digitalWrite(motor1Pin2, HIGH); 
  digitalWrite(motor2Pin1, LOW);
  digitalWrite(motor2Pin2, LOW);    
}

void handleReverse() {
  Serial.println("Reverse");
  digitalWrite(motor1Pin1, HIGH);
  digitalWrite(motor1Pin2, LOW); 
  digitalWrite(motor2Pin1, HIGH);
  digitalWrite(motor2Pin2, LOW);          
}

void handleSpeed() {
  if (server.hasArg("value")) {
    valueString = server.arg("value");
    int value = valueString.toInt();
    if (value == 0) {
      ledcWrite(enable1Pin, 0);
      ledcWrite(enable2Pin, 0);
      digitalWrite(motor1Pin1, LOW); 
      digitalWrite(motor1Pin2, LOW); 
      digitalWrite(motor2Pin1, LOW);
      digitalWrite(motor2Pin2, LOW);   
    } else { 
      dutyCycle = map(value, 25, 100, 200, 255);
      ledcWrite(enable1Pin, dutyCycle);
      ledcWrite(enable2Pin, dutyCycle);
      Serial.println("Motor speed set to " + String(value));
    }
  }
}

void processControl(std::String command)
{
  if (command == )
}

void setup() {
  Serial.begin(115200);

  // Set the Motor pins as outputs
  pinMode(motor1Pin1, OUTPUT);
  pinMode(motor1Pin2, OUTPUT);
  pinMode(motor2Pin1, OUTPUT);
  pinMode(motor2Pin2, OUTPUT);

  // Configure PWM Pins
  ledcAttach(enable1Pin, freq, resolution);
  ledcAttach(enable2Pin, freq, resolution);
    
  // Initialize PWM with 0 duty cycle
  ledcWrite(enable1Pin, 0);
  ledcWrite(enable2Pin, 0);
  
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
  client.print("Hello");
    
  // Define routes
  //server.on("/", handleRoot);
  //server.on("/forward", handleForward);
  //server.on("/left", handleLeft);
  //server.on("/stop", handleStop);
  //server.on("/right", handleRight);
  //server.on("/reverse", handleReverse);
  //server.on("/speed", handleSpeed);

  // Start the server
  server.begin();
}

void loop() {
  while (WiFi.status == WL_CONNECTED)
  {
    HTTPClient client;
    http.begin(serveraddr);

    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK)
    {
      std::String payload = http.getString();
      Serial.println("received control: ", + payload);

      processControl();
    }
    else 
    {
        Serial.println("error in request");

    }

    delay(500);

  }
}
