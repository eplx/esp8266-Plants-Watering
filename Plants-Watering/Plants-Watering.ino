#include <SimpleDHT.h>
#include <ESP8266WiFi.h> 
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>


//WIFI credentials
const char* ssid = "SSID";
const char* pwd = "PASSWORD";
const long defaultPumpInterval = 86400000;

ESP8266WebServer server(8356); //Create web server instance

/* for DHT11, 
//      VCC: 5V or 3V
//      GND: GND
//      DATA: 2
*/
int pinDHT11 = D1;    // DHT digital Input (Digital)
int pinPhotoCell = A0;// PhotoCell Input (Analog)
int pinWaterPump = D2;// WaterPump relay Output (Digital

// Water pump configuration
// 86400000 = each 24hours
// 43200000 = each 12 hours

long PumpInterval = defaultPumpInterval; // default interval to water  (24 hours)
long PumpDuration = 20000;               // default water duration (seconds)
long WeatherCheckInterval = 10800000;    // default weather check interval (3 hours)
unsigned long PumpPrevMillis = 0;       // store last time for Water Pump
unsigned long WeatherCheckPrevMillis = 0;    // Weather prev millis
unsigned long currentMillis = 0;        // actual millis


int WaterPlantTimes = 0;                // count times plant was watered
float photoCellReading;                 // Light reading (analog value)
String Light;                           // Light string
SimpleDHT11 dht11;                      // Temperature & Humidity object

/*global variables */
int WeatherTemp=0;                    // OpenWeatherAPI - Temperature
int WeatherPressure=0;                // OpenWeatherAPI - Pressure
int WeatherActual=0;                  // OpenWeatherAPI - Actual Weather
char WeatherDescription [255];         // OpenWeatherAPI - Weather Main (Rain, sunny, etc)
int WeatherForecast[8];                // OpenWeatherAPI - next day every 3 hours

/* Setup */
void setup() {
  Serial.begin(115200);
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(pinWaterPump, OUTPUT);
  
  //connecting to wifi
  Serial.println();
  Serial.println();
  Serial.print("Connecting to wifi ");
  Serial.println(ssid);
  WiFi.begin(ssid, pwd);
  
  //attempt to connect to wifi
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print("."); //progress until Wifi connected
  }
    Serial.println("");

    //while connected prin    delay(1000);t this
    Serial.println("Wifi connected");

    //get the ip address and print it
    Serial.print("This is your ip address: ");
    Serial.print("http://");
    Serial.print(WiFi.localIP());
    Serial.println("/");

    server.on("/", handleRoot);
    server.on("/html", handleRootHTML);
    server.on("/water458now", handleWaterNow);
    server.on("/inline", [](){
      server.send(200, "text/plain", "this works as well");
    });

    server.begin();
    Serial.println("HTTP server started");

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
        Serial.print("Read DHT11 failed, err="); Serial.println(err);delay(1000);
      return; 
      }

      if (temperature > 24 & temperature <31 && humidity < 60) {
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
    Serial.print("Read DHT11 failed, err="); Serial.println(err);delay(1000);
    return; 
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
  int minutes = (int) (((PumpInterval- (currentMillis - PumpPrevMillis)) / (1000*60)) %60);
  int hours = (int) (((PumpInterval- (currentMillis - PumpPrevMillis)) / (1000*60*60)) %24);
  
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
    Serial.print("Read DHT11 failed, err="); Serial.println(err);delay(1000);
    return;
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
  int minutes = (int) (((PumpInterval- (currentMillis - PumpPrevMillis)) / (1000*60)) %60);
  int hours = (int) (((PumpInterval- (currentMillis - PumpPrevMillis)) / (1000*60*60)) %24);
  // Calculate when was last water plant
  int lastMinutes = (int) (((currentMillis - PumpPrevMillis) / (1000*60))%60);
  int lastHours = (int) (((currentMillis - PumpPrevMillis) / (1000*60*60)) % 24);
  int lastDays = (int) (((PumpInterval- (currentMillis - PumpPrevMillis)) / (1000*60*60*24)));
  
  // print output (HTML)
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
    <h1>Current Weather</h1>\
    <p>Temp: %02d C - Status: %s </p>\
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
    PumpDuration/1000,
    WeatherTemp,
    WeatherDescription

  );
  
  digitalWrite(LED_BUILTIN, HIGH);
  server.send(200, "text/html", message);
  delay(1000);
  digitalWrite(LED_BUILTIN, LOW);
}

/* handle water now webpage */
void handleWaterNow() {
String message;
  if (WaterPlantTimes < 4 ) {
      // print in Serial
      Serial.println("Watering now...");
      // print output (HTML)
      message = "Watering now... ";
      // Water Logic
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

