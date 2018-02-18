
/*
   Smart Plant Watering - Version: 1.01 - February 2018
   Author: Esteban Pizzini - esteban.pizzini@gmail.com
   Purpose: Measure temperature & humidity and water plants automatically based on weather conditions.

   Release notes:
   1.00 - Initial version
   1.01 - Implemented WifiManager library for configuring WIFI networks - Source: https://github.com/tzapu/WiFiManager  
          
*/
#include <SimpleDHT.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

// Required for LIGHT_SLEEP_T delay mode
//extern "C" {
//#include "user_interface.h"
//}

/* Watering configuration - reference
   86400000 = each 24hours
   43200000 = each 12 hours
*/
const long defaultPumpInterval = 86400000;

ESP8266WebServer server(8356); //Create web server instance - using port 8356

/* Sensors PINS
    DH11 --> D2
    PhotoCell --> A0
    Relay --> D3
*/
int pinDHT11 = D2;                        // DHT digital Input (Digital)
int pinPhotoCell = A0;                    // PhotoCell Input (Analog)
int pinWaterPump = D3;                    // WaterPump relay Output (Digital
long PumpInterval = defaultPumpInterval;  // default interval to water  (24 hours)
long PumpDuration = 20000;                // default water duration (seconds)
//long WeatherCheckInterval = 10800000;     // default weather check interval (3 hours)
unsigned long PumpPrevMillis = 0;         // store last time for Water Pump
unsigned long WeatherCheckPrevMillis = 0; // Weather prev millis

unsigned long currentMillis = 0;          // actual millis
int WaterPlantTimes = 0;                  // count times plant was watered
float photoCellReading = 0;               // Light reading (Enable Light Sleep modeanalog value)
String Light;                             // Light string
SimpleDHT11 dht11;                        // Temperature & Humidity object
const int sleepTimeSec = 10;              // Time to sleep (in seconds)


/* Initial Setup */
void setup() {

  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);   // initialize digital pin LED_BUILTIN as an output.
  pinMode(pinWaterPump, OUTPUT);  // initialize Relay PIN as an output.
  
  WiFiManager wifiManager; // Enable wifiManager
  wifiManager.setDebugOutput(false);

  //WiFi.mode(WIFI_STA); // for light Sleep mode
  //wifi_set_sleep_type(LIGHT_SLEEP_T); // Enable Light Sleep mode

  //Initiate web server
  server.on("/", handleRoot);
  server.on("/html", handleRootHTML);
  server.on("/waternow", handleWaterNow);
  server.on("/inline", []() {
    server.send(200, "text/plain", "Smart Water Plant");
  });
  server.begin();
  Serial.println("HTTP server initiated - Ok!");

  // Config WIFI
  //WifiManager configuration - it will try to connect to known network or prompt for information to connect.
  //WifiManager library used -- https://github.com/tzapu/WiFiManager
  
  wifiManager.setBreakAfterConfig(true);  //exit after config instead of connecting

  //reset settings - for testing
  //wifiManager.resetSettings();
  
  //If no previous WIFI settings are available, it will be enabled as AP with default SSID "SmartWaterPlant" and password "water"
  if (!wifiManager.autoConnect("SmartWaterPlant", "water")) {
    Serial.println("failed to connect...Reset in progress");
    delay(3000);
    ESP.reset();
    delay(5000);
  }

}

/* Main Loop */
void loop() {


  //handle web requests
  digitalWrite(LED_BUILTIN, HIGH);
  server.handleClient(); // handle web clients
  delay(2000);
  digitalWrite(LED_BUILTIN, LOW);

  // Water Logic
  currentMillis = millis();

  // PumpWater Logic
  if (currentMillis - PumpPrevMillis >= PumpInterval) {
    PumpPrevMillis = currentMillis;
    PumpInterval = defaultPumpInterval;
    pumpWater(1); // pump water on
  }
  else if (currentMillis - PumpPrevMillis >= PumpDuration) {
    pumpWater(0); // pump water on
  }

  // Adjust Watering based on temperature & humidity sensors (DHT11)
  byte temperature = 0;
  byte humidity = 0;
  int err = SimpleDHTErrSuccess;
  if ((err = dht11.read(pinDHT11, &temperature, &humidity, NULL)) != SimpleDHTErrSuccess) {
    Serial.print("Read DHT11 failed, err="); Serial.println(err); delay(1000);
  }

  if (temperature > 24 & temperature < 31 && humidity < 60) {
    PumpDuration = 25000;
  }
  else if (temperature > 30 && humidity < 60) {
    PumpDuration = 30000;
  }
  else {
    PumpDuration = 20000;
  }
}

/* handle root webpage */
void handleRoot() {

  // print in Serial
  Serial.println("=================================");
  // read without samples.
  byte temperature = 0;
  byte humidity = 0;
  int err = SimpleDHTErrSuccess;
  if ((err = dht11.read(pinDHT11, &temperature, &humidity, NULL)) != SimpleDHTErrSuccess) {
    Serial.print("Read DHT11 failed, err="); Serial.println(err); delay(1000);

  }

  // read PhotoCell (Light)Temperature
  photoCellReading = analogRead(pinPhotoCell);
  // We'll have a few threshholds, qualitatively determined
  if (photoCellReading < 10) {
    Light = " Dark";
  } else if (photoCellReading < 200) {
    Light = " Dim";
  } else if (photoCellReading < 500) {
    Light = " Light";
  } else if (photoCellReading < 800) {
    Light = " Bright";
  } else {
    Light = " Very bright";
  }

  // print values
  Serial.print("Sample OK: ");
  Serial.print((int)temperature); Serial.print(" *C, ");
  Serial.print((int)humidity); Serial.println(" H");
  Serial.print((int)photoCellReading);

  // Get minutes for next WaterPump
  int minutes = (int) (((PumpInterval - (currentMillis - PumpPrevMillis)) / (1000 * 60)) % 60);
  int hours = (int) (((PumpInterval - (currentMillis - PumpPrevMillis)) / (1000 * 60 * 60)) % 24);

  // print output (HTML)
  String message = "Temp: ";
  message += (int)temperature;
  message += "C - Hum: ";
  message += (int)humidity;
  message += "%";
  message += " - Light:";
  message += Light;
  message += " Next pump in: ";
  message += hours;
  message += " hs ";
  message += minutes;
  message += " min";
  digitalWrite(LED_BUILTIN, HIGH);
  server.send(200, "text/plain", message);
  delay(1000);
  digitalWrite(LED_BUILTIN, LOW);
}

/* handle /html webpage */
void handleRootHTML() {
  byte temperature = 0; // sensor temperature
  byte humidity = 0;  // sensor humidity

  int err = SimpleDHTErrSuccess;
  if ((err = dht11.read(pinDHT11, &temperature, &humidity, NULL)) != SimpleDHTErrSuccess) {
    Serial.print("Read DHT11 failed, err="); Serial.println(err); delay(1000);
  }

  // read PhotoCell (Light)
  int LightValue = 0;
  photoCellReading = analogRead(pinPhotoCell);
  if (photoCellReading < 10) {
    LightValue = 0;
  } else if (photoCellReading < 200) {
    LightValue = 30;
  } else if (photoCellReading < 500) {
    LightValue = 60;
  } else if (photoCellReading < 800) {
    LightValue = 80;
  } else {
    LightValue = 100;
  }

  // Get minutes for next WaterPump
  int minutes = (int) (((PumpInterval - (currentMillis - PumpPrevMillis)) / (1000 * 60)) % 60);
  int hours = (int) (((PumpInterval - (currentMillis - PumpPrevMillis)) / (1000 * 60 * 60)) % 24);
  // Calculate when was last water plant
  int lastMinutes = (int) (((currentMillis - PumpPrevMillis) / (1000 * 60)) % 60);
  int lastHours = (int) (((currentMillis - PumpPrevMillis) / (1000 * 60 * 60)) % 24);
  int lastDays = (int) (((PumpInterval - (currentMillis - PumpPrevMillis)) / (1000 * 60 * 60 * 24)));

  char message [1500];
  snprintf ( message, 1500,
"<html>\
  <head>\
    <title>Plant Water Pump</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <h1>Temperature (sensor)</h1>\
    <p>%02d C</p>\
    <h1>Humidity (sensor)</h1>\
    <p>%02d %</p>\
    <h1>Light</h1>\
    <p>%02d % (0 Dark..100 Very bright)</p>\
    <h1>Next plant watering in</h1>\
    <p>%02dh:%02dm</p>\
    <h1>Time since last plant watering</h1>\
    <p>%02dd:%02dh:%02dm</p>\
    <h1>Times watered since last reset</h1>\
    <p>%02d times</p>\
    <h1>Watering duration</h1>\
    <p>%d seconds</p>\
  </body>\
</html>",
    temperature,
    humidity,
    LightValue,
    hours,
    minutes,
    lastDays,
    lastHours,
    lastMinutes,
    WaterPlantTimes,
    PumpDuration/1000
  );

  digitalWrite(LED_BUILTIN, HIGH);
  server.send(200, "text/html", message);
  delay(1000);
  digitalWrite(LED_BUILTIN, LOW);

}

/* handle water now request - Allow only up to 3 attemps */
void handleWaterNow() {

  String message;
  if (WaterPlantTimes < 4 ) {
    Serial.println("Watering now...");
    message = "Watering now... ";
    currentMillis = millis();
    PumpPrevMillis = currentMillis;
    PumpInterval = defaultPumpInterval;
    pumpWater(1); //  water on
  }
  else
  {
    message = "Manual watering not allowed...sorry! ";
  }
  digitalWrite(LED_BUILTIN, HIGH);
  server.send(200, "text/plain", message);
  delay(1000);
  digitalWrite(LED_BUILTIN, LOW);
}

void pumpWater(int mode) {

  if (mode == 1) {
    Serial.println("Water = On");
    WaterPlantTimes++; // Increase water plant counter
    digitalWrite(pinWaterPump, HIGH);
    digitalWrite(LED_BUILTIN, HIGH);
  }
  else {
    Serial.println("Water = Off");
    digitalWrite(pinWaterPump, LOW);
    digitalWrite(LED_BUILTIN, LOW);
  }
}

