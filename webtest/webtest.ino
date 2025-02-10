#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
// Replace with your network credentials
//const char* ssid = "NOKIA-4D01";
//const char* password = "9LqFFSXEZb";
const char* ssid = "mystery";
const char* password = "electrotech";

const char* serveraddr = "http://192.168.1.110:4444/control";

#define LED 2

String lastState = "";

void ledon()
{
  digitalWrite(LED, HIGH);
  Serial.println("led on");
}

void ledoff()
{
  digitalWrite(LED, LOW);
  Serial.println("led off");
}

void processled(String command)
{
  Serial.println("raw payload: " + command);

  command.trim();

  Serial.println("trimmed payload: " + command);
  if (command == "ON")
  {
    ledon();
  }
  else if (command == "OFF")
  {
    ledoff();
  }
  else
  {
    Serial.println("unknown command my guy");
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(LED, OUTPUT);

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
}

void loop() {
  if (WiFi.status() == WL_CONNECTED)
  {
    HTTPClient http;
    http.begin(serveraddr);

    int httpCode = http.GET();
  
    if (httpCode == HTTP_CODE_OK)
    {
      String payload = http.getString();
      Serial.println("received control: " + payload);

      StaticJsonDocument<200> doc;  // Adjust the size as needed
      DeserializationError error = deserializeJson(doc, payload);

      if (error)
      {
        Serial.println("failed to parse JSON " + String(error.c_str()));
      }
      else 
      {
        String command = doc["command"].as<String>();
        String newState = String(command);
        
        if (newState != lastState)
        {
          processled(command);
          lastState = newState;
        }
      }

    }
    else 
    {
        Serial.println("error in request");
    }

   // delay(500);
  }
  
  else
  {
    Serial.println("WiFi disconnected.");
  }
}
  