
/*
   Smart Plant Watering & Weather station powered by Solar panel - Version: 2.00 - July 2018
   Author: Esteban Pizzini - esteban.pizzini@gmail.com
   Purpose: Measure temperature & humidity and water plants automatically based on weather conditions.
   Powered by Solar Panel so sleep features are being used to save power consumption.

   Release notes:
   1.00 - Initial version
   1.01 - Implemented WifiManager library for configuring WIFI networks - Source: https://github.com/tzapu/WiFiManager 
   1.02 - Clean up code & fixed DHT11 read issue.
   1.03 - Fixed IP address display issue on serial output. Cleaned up output messages.
   1.04 - Added interface with OpenWeatherMap.org site to get current/forecast weather information for its current location
   2.00 - Powered by Solar panel (1 x 10W panel 12v w/ Direct sun light ~4 hours per day)
          Using ThinkSpeaks.com to upload sensors information (IoT analytics) and visualize information from the web 
          example: this is the channel I am using for testing https://thingspeak.com/channels/504661  
   2.01 - Added Ultrasonic sensor to measure water level in the water can
          Modified LighSensor logic
*/
#include <SimpleDHT.h> // DHT11 sensor library
#include <ESP8266WiFi.h> // Wifi library
#include <WiFiManager.h> // Wifi Manager (helps to easily configure Wifi settings
#include <ESP8266HTTPClient.h> // Library for making HTTP requests
#include <ArduinoJson.h> // Library to read JSON 
//#include <ESP8266WebServer.h> // to be used later


// Required for LIGHT_SLEEP_T delay mode
extern "C" {
#include "user_interface.h"
}

/* Watering/Sleep interval configuration - reference
   86400000 =  24hours
   43200000 =  12 hours
   1800000 = 30 minutes 
*/
const long defaultPumpInterval = 86400000;
const long defaultSleepInterval = 1800000;

/* Configuration for third party sites used by this code
 * ThingSpeak.com - used for IoT analytis, posting captured values (i.e. temperature, humidity, etc) and creating charts
 * OpenWeatherMap.org - Use openweathermap.org to get weather info from actual location.
*/
/* ThingSpeaks */
int ThingSpeaks_WAIT = 15500; // if you are using free ThingSpeaks version you need to wait about 15 seconds between updates.
char ThingSpeaks_URL[100] = "http://api.thingspeak.com/update?api_key=";
char ThingSpeaks_KEY[100] = "xxxxxxxxxx"; // Thingspeak key

/* OpenWeatherMap */
// This information will adjust watering frequency/duration considering actual weather and forecast for the next hours.
char openWeatherAPIid[10] = "0000000";             // Location - Uses openweathermap.org to get weather info from actual location - http://api.openweathermap.org/ - ID
char openWeatherAPIappid[50] = "xxxxxxxxxx";          // Your APP ID - Uses openweathermap.org to get weather info from actual location - http://api.openweathermap.org/ - APP ID

/* Sensors PINS (refer to schematic)
    DH11 --> D2
    PhotoCell --> A0 (Analog)
    Relay --> D3
    Ultrasonic sensor (trigger) -> D6
    Ultrasonic sensor (echo) -> D7
*/

/* water can level (reference only) - this is a reference using a 10lts water can. This may vary based on your water can
 *  35cm - empty
 *  30cm - 1 litre
 *  25cm - 2 litres
 *  21cm - 3 litres
 *  15cm - 5 litres
 *  11cm - 6 litres
 */
#define trigPin D6                        // Ultrasonic sensor - Trigger
#define echoPin D7                        // Ultrasonic sensor - Echo
//float WaterLevelEmpty = 35;                 // NEED to adjust based on your water jerry can - Empty
float WaterLevelFull = 11;                  // NEED to adjust based on your water jerry can - Full
float WaterLevelLitres = 6;                 // NEED to adjust based on your water jerry can - Capacity in litres
float WaterLevelcurrent = 0;
int pinDHT11 = D2;                        // DHT digital Input (Digital)
int pinPhotoCell = A0;                    // PhotoCell Input (Analog)
int pinWaterPump = D3;                    // WaterPump relay Output (Digital
long PumpInterval = defaultPumpInterval;  // default interval to water  (24 hours)
long DefaultPumpDuration = 20000;         // default water duration (seconds)
long PumpDuration = 0;                    // pumpDuration
long WeatherCheckInterval = 1800000;      // default weather check interval (30 minutes)
long SleepInterval = defaultSleepInterval; // default sleep time (1 hour)
unsigned long PumpPrevMillis = 0;         // store last time for Water Pump
unsigned long WeatherCheckPrevMillis = 0; // Weather prev milliseconds
unsigned long SleepPrevMillis = 0;        // Last time sleep mode
unsigned long currentMillis = 0;          // actual milliseconds
int WaterPlantTimes = 0;                  // count times plant was watered
String Light;                             // Light string
SimpleDHT11 dht11;                        // Temperature & Humidity object

byte temperature = 0;
byte humidity = 0;
int LightValue = 0;

int WeatherTemp=0;                    // OpenWeatherAPI - Temperature
int WeatherPressure=0;                // OpenWeatherAPI - Pressure
int WeatherActual=0;                  // OpenWeatherAPI - Actual Weather
char WeatherDescription [255];        // OpenWeatherAPI - Weather Main (Rain, sunny, etc)
int WeatherForecast[3];               // OpenWeatherAPI - next two days

/* Initialization */
  void setup() {
    Serial.begin(115200);
    delay(5000);
    for (int s=1;s<1000;s++) {
      Serial.print(".");
    }
    Serial.println();
    Serial.println("Smart Plant Watering by Esteban Pizzini (esteban.pizzini@gmail.com) - Initializing...");

    pinMode(LED_BUILTIN, OUTPUT);   // initialize digital pin LED_BUILTIN as an output.
    pinMode(pinWaterPump, OUTPUT);  // initialize Relay PIN as an output.
    pinMode(trigPin, OUTPUT);       // initialize Ultrosonic sensor - trigger pin
    pinMode(echoPin, INPUT);        // initialize Ultrosonic sensor - echo pin
    digitalWrite(LED_BUILTIN, HIGH); // Led on - initial setup started
    
    WiFiManager wifiManager; // Enable wifiManager
    wifiManager.setDebugOutput(false);
  
    WiFi.mode(WIFI_STA); // for light Sleep mode
    wifi_set_sleep_type(LIGHT_SLEEP_T); // Enable Light Sleep mode
  
    // Wifi configuration - uses WifiManger library
    //WifiManager configuration - it will try to connect to known network or prompt for information to connect.
    //WifiManager library used -- https://github.com/tzapu/WiFiManager
    
    wifiManager.setBreakAfterConfig(true);  //exit after config instead of connecting
  
    Serial.println("Initializing custom parameters..");

    /* Weather forecast - default values */
    WeatherForecast[0]=0;
    WeatherForecast[1]=0;
    WeatherForecast[2]=0;
    
    // id/name, placeholder/prompt, default, length

    //reset settings - only for testing
    //wifiManager.resetSettings(); // uncomment this line only for testing and reset wifi settings.

    Serial.println("initializing WiFi..");

    //If no previous WIFI settings are available, it will be enabled as AP with default SSID "SmartWaterPlant" and password "water"
    if (!wifiManager.autoConnect("1 SmartWaterPlant", "water")) {
      Serial.println("Wifi - connection failed...Reset in progress");
      delay(3000);
        ESP.reset();
      delay(5000);
    }
  
    Serial.println("Listeining on IP:");
    Serial.println(WiFi.localIP());
    Serial.println("Initializing sensors...");
    // DHT11 sensor (temperature, humidity)
    int err = SimpleDHTErrSuccess;
    if ((err = dht11.read(pinDHT11, &temperature, &humidity, NULL)) != SimpleDHTErrSuccess) {
       Serial.print("DHT11 sensor - failed, err="); Serial.println(err); delay(1000);
    }
    else {
       Serial.println("DHT11 sensor - Ok");
    }
    //photoCell Sensor (Light)
    LightValue = checkPhotoCellSensor();
    (LightValue > 0) ? Serial.println("photoCell sensor - Ok") :Serial.println("photoCell sensor - Failed");

    //ultrasonic Sensor (Water level)
    WaterLevelcurrent = checkWaterLevel();
    (WaterLevelcurrent > 0) ? Serial.println("WaterLevel sensor - Ok") :Serial.println("WaterLevel sensor - Failed (no water)");
    
    Serial.println("...checking OpenWeatherMap for current weather & forecast");
    checkCurrentWeather();
    checkForecastWeather();

    Serial.println("...sending information to ThingSpeaks");
    updateThingSpeaks();
    Serial.println("...initialization completed!");

    // Water pump (test)
    /*if (checkWaterLevel() > 0.5) {
      pumpWater(1); // pump water on
      delay(5000);
      pumpWater(0);
    }
    */

    // By default - Sleep after initial setup
    Serial.println("...sleeping.. wifi off...");
    WiFi.forceSleepBegin();
    delay(1);
    digitalWrite(LED_BUILTIN, LOW); // turn led off - initial setup ended
    Serial.println("Sleep...see you in ");
    Serial.print(SleepInterval/3600000);
    Serial.print(" hours");

  }





/* Main Loop */
void loop() {
  digitalWrite(LED_BUILTIN, HIGH);
  delay(5000);
  digitalWrite(LED_BUILTIN, LOW); // turn led OFF
  
  // Water Logic
  currentMillis = millis();
 
  // Wake up & send information using WIFI - This is when the device will send information to the internet
  // Adjust SleepInterval based on your battery capacity and also considering how often you want to get data updated.
  if (currentMillis - SleepPrevMillis >= SleepInterval) {
    Serial.println("wake up device");
    SleepPrevMillis = currentMillis;

 

    WiFi.forceSleepWake();  // Wake up device!
    delay(1);
    updateThingSpeaks();
    WiFi.forceSleepBegin(); // Go to sleep again!
    delay(1);
    Serial.println("Sleep...see you in ");
    Serial.print(SleepInterval/3600000);
    Serial.print(" hours");
  }

  // Check forecast & adjust Watering interval & update other values
  if (currentMillis - WeatherCheckPrevMillis >= WeatherCheckInterval) {
      WeatherCheckPrevMillis = currentMillis;
      WaterLevelcurrent = checkWaterLevel(); // update water level
      checkCurrentWeather();
      checkForecastWeather();
      // If forecast says we will have clouds or rainy weather, adjust sleep timing
      // Read temperature/humidity from DHT11 sensor and adjust Watering based on that
        int err = SimpleDHTErrSuccess;
        if ((err = dht11.read(pinDHT11, &temperature, &humidity, NULL)) != SimpleDHTErrSuccess) {
          Serial.print("ERROR: Read DHT11 failed, err="); Serial.println(err); delay(1000);
        }
      
        if (temperature > 24 & temperature < 31 && humidity < 60 && (WeatherActual>=800 || WeatherForecast[0] >= 800)) {
          PumpDuration = DefaultPumpDuration+12000;
        }
        else if (temperature > 30 && humidity < 60 && (WeatherActual>=800 || WeatherForecast[0]>=800)) {
          PumpDuration = DefaultPumpDuration+15000;
        }
        else {
          PumpDuration = DefaultPumpDuration;
        }

   /* Verify photoCell Sensor & adjust Sleep level - Power saving
     *  If we have sunlight (70-100) we update information more often 
     *  If we don't have sunlight we update less often to save energy
     */
    LightValue = checkPhotoCellSensor();
    if (WeatherForecast[0] < 800 && WeatherActual < 800) {
      SleepInterval = defaultSleepInterval * 5; // every 2.5 hours
    }
    else if (LightValue < 30 || WeatherForecast[0] < 800) {
      SleepInterval = defaultSleepInterval * 4; // every 2 hours
    }
    else if (LightValue < 50) {
      SleepInterval = defaultSleepInterval * 3; // every 1.5 hours
    }
    else if (LightValue < 80) {
      SleepInterval = defaultSleepInterval * 2; // every hour
    }
    else {
      SleepInterval = defaultSleepInterval; // every 30 min
    }


        
  }

  // Check if it is time for plant watering - PumpWater Logic
  if (currentMillis - PumpPrevMillis >= PumpInterval) {
    PumpPrevMillis = currentMillis;
    PumpInterval = defaultPumpInterval;
    if (checkWaterLevel() > 0.5) {
      pumpWater(1); // pump water on if we have enough water.
    }
    else {
      Serial.println("Water pump failed - water level too low");
    }
    
  }
  else if (currentMillis - PumpPrevMillis >= PumpDuration) {
    pumpWater(0); // pump water off
  }
}

/* functions */

void pumpWater(int mode) {
  if (mode == 1) {
    //Serial.println("Water = On");
    WaterPlantTimes++; // Increase water plant counter
    digitalWrite(pinWaterPump, HIGH);
    digitalWrite(LED_BUILTIN, HIGH);
  }
  else {
    //Serial.println("Water = Off");
    digitalWrite(pinWaterPump, LOW);
    digitalWrite(LED_BUILTIN, LOW);
  }
}

/* checkOpenWeather - use openWeatherAPI to check current weather*/
void checkCurrentWeather () {
    /* check current weather from openweathermap */
    char url[256] = "http://api.openweathermap.org/data/2.5/weather?id=";   //openweathermap.org URL for checking actual weather
    HTTPClient http;  //Declare an object of class HTTPClient
 
    strcat(url,openWeatherAPIid);
    strcat(url,"&APPID=");
    strcat(url,openWeatherAPIappid);
    strcat(url,"&units=metric");

    Serial.println(url);
    
    http.begin(url);

    int httpCode = http.GET();                                                                  //Send the request
 
    if (httpCode == 200) { //Check the returning code
        String payload = http.getString();   //Get the request response payload
        //Serial.println(payload);
        char JSONMessage[1000];
        payload.toCharArray(JSONMessage, 1000);         // Read JSON - Variable for weather information (json format)
        StaticJsonBuffer<1000> JSONBuffer;              // create JsonBuffer object
        JsonObject&  parsed= JSONBuffer.parseObject(JSONMessage);
      
        if (!parsed.success()) {
         Serial.println("Parsing failed");
        }
        else {
         const char * sensorType = parsed["coord"]["lat"]; //Get sensor type value
         const char * currentWeather = parsed["weather"][0]["description"];
         WeatherTemp = (int) parsed["main"]["temp"];
         WeatherActual = (int) parsed["weather"][0]["id"];
         WeatherPressure = (int) parsed["main"]["pressure"];
         strcpy(WeatherDescription, currentWeather );
         Serial.print("OpenWeatherMap info - current Weather:");
         Serial.println(WeatherActual);
         Serial.print("OpenWeatherMap info- current Temp:");
         Serial.println(WeatherTemp);
         Serial.print("OpenWeatherMap info- current Pressure:");
         Serial.println(WeatherPressure);
        }
    }
    else
    {
      Serial.print("CheckCurrentWeather (OpenWeatherMap.org)- failed! - err:");Serial.println(httpCode);
    }
     http.end();   //Close HTTP client connection
}

/* checkForecastWeather - use openWeatherAPI to check current weather*/
void checkForecastWeather () {
  char url[256] = "http://api.openweathermap.org/data/2.5/forecast/daily?id=";
  /* check current weather from openweathermap */
  HTTPClient http;  //Declare an object of class HTTPClient
 
  strcat(url,openWeatherAPIid);
  strcat(url,"&APPID=");
  strcat(url,openWeatherAPIappid);
  strcat(url,"&units=metric&cnt=2");
  http.begin(url);
  int httpCode = http.GET();                                                                  //Send the request
    if (httpCode == 200) { //Check the returning code
        String payload = http.getString();   //Get the request response payload
        //Serial.println(payload);
        //Serial.println(payload.length());
        // Read JSON - Variable for weather information (json format)
        char JSONMessage[1000];
        payload.toCharArray(JSONMessage, 1000);
 
        StaticJsonBuffer<1000> JSONBuffer; // create JsonBuffer object
        JsonObject&  parsed= JSONBuffer.parseObject(JSONMessage);
      
        if (!parsed.success()) {
         Serial.println("Parsing failed");
        }
        else {
        //int test = (int) parsed["list"][0]["weather"][0]["id"];
        //Serial.println(test);
        WeatherForecast[0]= (int)parsed["list"][1]["weather"][0]["id"]; 
        Serial.print("OpenWeatherMap info - tomorrow forecast:");
        Serial.println(WeatherForecast[0]);
        }
    }
    else
    {
      Serial.print("checkForecastWeather (OpenWeatherMap.org)- failed! - err:");Serial.println(httpCode);
    }
   http.end();
}

void updateThingSpeaks() {
 /* This function will update sensors data into www.ThingSpeaks.com 
  *  Field 1 -> Temperature
  *  Field 2 -> Humidity
  *  Field 3 -> Light
  *  Field 4 -> Hours for next plant watering
*/
 char temp[50]; // used to format URL
 char url[255]; // URL to be used to call ThingSpeaks API
 HTTPClient http2;  //Declare an object of class HTTPClient
 
 currentMillis = millis(); // get current milliseconds since started
 // Update Field 1 - temperature
 sprintf(temp,"&field1=%d", temperature);
 strcpy(url,ThingSpeaks_URL);
 strcat(url,ThingSpeaks_KEY);
 strcat(url,temp);
 //Serial.print(url);
 http2.begin(url);  //Specify request destination
 int httpCode = http2.GET();
 Serial.print("Temperature");  
 (httpCode=200) ? Serial.println("..Completed!") : Serial.println("..Failed!");
 http2.end();   //Close connection
 delay(ThingSpeaks_WAIT);
 
// Update field 2 - humidity
 sprintf(temp,"&field2=%d", humidity);
 strcpy(url,ThingSpeaks_URL);
 strcat(url,ThingSpeaks_KEY);
 strcat(url,temp);
 //Serial.print(url);
 http2.begin(url);  //Specify request destination
 httpCode = http2.GET();                                                                  //Send the request
 Serial.print("Humidity");
 (httpCode=200) ? Serial.println("..Completed!") : Serial.println("..Failed!");
 http2.end();   //Close connection
 delay(ThingSpeaks_WAIT);

// Update field 3 - light
 sprintf(temp,"&field3=%d", LightValue);
 strcpy(url,ThingSpeaks_URL);
 strcat(url,ThingSpeaks_KEY);
 strcat(url,temp);
   //Serial.print(url);
 http2.begin(url);  //Specify request destination
 httpCode = http2.GET();
 Serial.print("Light");
 (httpCode=200) ? Serial.println("..Completed!") : Serial.println("..Failed!");
 http2.end();   //Close connection
 delay(ThingSpeaks_WAIT);

 // Update field 4 - Next Plant watering
 int minutes = (int) (((PumpInterval - (currentMillis - PumpPrevMillis)) / (1000 * 60)) % 60);
 int hours = (int) (((PumpInterval - (currentMillis - PumpPrevMillis)) / (1000 * 60 * 60)) % 24);
 // Calculate when was last water plant
 int lastMinutes = (int) (((currentMillis - PumpPrevMillis) / (1000 * 60)) % 60);
 int lastHours = (int) (((currentMillis - PumpPrevMillis) / (1000 * 60 * 60)) % 24);
 int lastDays = (int) (((PumpInterval - (currentMillis - PumpPrevMillis)) / (1000 * 60 * 60 * 24)));
 
 sprintf(temp,"&field4=%d", hours+(minutes/60));
 strcpy(url,ThingSpeaks_URL);
 strcat(url,ThingSpeaks_KEY);
 strcat(url,temp);
 //Serial.print(url);
 http2.begin(url); 
 httpCode = http2.GET();
 Serial.print("Next plant watering");
 (httpCode=200) ? Serial.println("..Completed!") : Serial.println("..Failed!");
 http2.end();
 delay(ThingSpeaks_WAIT);

// Update field 6 - water level
 //Serial.println(WaterLevelcurrent);
 dtostrf(WaterLevelcurrent, 2, 2, temp);
 //Serial.println(temp);
 strcpy(url,ThingSpeaks_URL);
 strcat(url,ThingSpeaks_KEY);
 strcat(url,"&field6=");
 strcat(url,temp);
 //Serial.println(url);
 http2.begin(url);  //Specify request destination
 httpCode = http2.GET();
 Serial.print("Water level");
 (httpCode=200) ? Serial.println("..Completed!") : Serial.println("..Failed!");
 http2.end();   //Close connection
 delay(ThingSpeaks_WAIT);

}

/* Check PhotoCellSensor and return Light % */
int checkPhotoCellSensor (){
        float photoCellReading = 0;               // Light reading (Enable Light Sleep modeanalog value)
        int LightValue = 0;
        photoCellReading = analogRead(pinPhotoCell);
        //Serial.println("photoCell reading:");
        //Serial.print(photoCellReading);

        LightValue = photoCellReading * 100 / 1100;

        /*
        if (photoCellReading < 100) {
            LightValue = 0;
          } else if (photoCellReading < 200) {
            LightValue = 5;
          } else if (photoCellReading < 300) {
            LightValue = 10;
          } else if (photoCellReading < 400) {
            LightValue = 20;
          } else if (photoCellReading < 500) {
            LightValue = 30;
          } else if (photoCellReading < 600) {
            LightValue = 40;
          } else if (photoCellReading < 700) {
            LightValue = 50;
          } else if (photoCellReading < 800) {
            LightValue = 60;
          } else if (photoCellReading < 900) {
            LightValue = 70;
          } else if (photoCellReading < 1000) {
            LightValue = 80;
          } else if (photoCellReading < 1100) {
            LightValue = 90;
          } else {
            LightValue = 100;
          }
          */
        return LightValue;
}

float checkWaterLevel() {
  float duration, distance;
  float litres;
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  duration = pulseIn(echoPin, HIGH);
  distance = (duration/2) / 29.1;
  //Serial.print(distance); Serial.println(" cm");
  litres =  WaterLevelLitres * WaterLevelFull / distance;
  //Serial.print(litres); Serial.println(" lts");
  if (litres < 0) 
    { 
      litres = 0; 
    }
  return litres;
}

