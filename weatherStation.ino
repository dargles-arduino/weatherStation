/**
 * Program: weatherStation
 * Purpose:
 *   I have a sneaking suspicion that I've got so many competing libraries 
 *   in this program that this is causing instability at start-up. To check 
 *   this out, version 5 starts completely from scratch, rebuilding to see 
 *   if I can identify a problem point, especially in terms of included 
 *   libraries. 
 *   I think this should work with ESP8266s in general; I'm using a bare 
 *   ESP12F to try and get sleep current down as low as possible.
 *   Now updated to use our new BT HomeHub
 * @author: David Argles, d.argles@gmx.com
 */

/* Program identification */ 
#define PROG    "weatherWebClient"
#define VER     "5.03"
#define BUILD   "03may2021 @17:42h"

/* Necessary includes */
#include "flashscreen.h"
#include "rtcMemory.h"
/* These includes are for the BME280 sensor */
#include <Wire.h>
#include <BMx280I2C.h>
/* These are for the WiFi & webclient */
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>

/* Global "defines" - some may have to look like variables because of type */
#define LOCAL_SSID  "whatever"
#define LOCAL_PWD   "key"
#define ADC_0       A0
#define CUTOFF      492     // 6V0 = 738, so target 4V -> 492
#define I2C_ADDRESS 0x76    // Defines the expected I2C address for the sensor
#define CHANNEL     "test"  // use "bat" for normal use, anything else for testing

/* ----- Initialisation ------------------------------------------------- */

/* Global stuff that must happen outside setup() */
rtcMemory         store;  // Creates an RTC memory object
BMx280I2C         bmx280(I2C_ADDRESS);  // Creates a BMx280I2C object using I2C
ESP8266WiFiMulti  WiFiMulti; // Creates a WiFiMulti object
int error         = 0;    // Reports any errors that occur during run

void setup() {
  // initialise objects
  flashscreen flash;
  
  // declare variables
  long int  baudrate  = 115200;   // Baudrate for serial output
  int       serialNo;             // Maintains a count of runs through deep sleep
  int       prevError = 0;        // error code for the previous run
  boolean   batteryOK = false;    // Checks whether our battery has sufficient charge
  uint64_t  deepSleepTime = 5e6;  //3600e6; // Deep sleep delay (millionths of sec)
  bool BMElive = false;           // records whether BME280 initialised properly
  int   adc  = 0;
  float temp = 0;
  float pres = 0;
  float hum  = 0;
  String  readings = "";          // For the parameter string in the http upload
  boolean successful = false;     // If we fail, let's record the value and try next time
  String urlRequest;              // String for contacting server
  pinMode(LED_BUILTIN, OUTPUT);   // Only use LED_BUILTIN in case of error; but initialise it now

  // Start up the serial output port
  Serial.begin(baudrate);
  // Serial.setDebugOutput(true);

  // Send program details to serial output
  flash.message(PROG, VER, BUILD);

  // Initialise the RTC memory
  store.readData();
  // Remember the serial no for this set of readings
  serialNo = store.count() + 1;
  store.incrementCount();
  Serial.print("Starting run ");
  Serial.println(serialNo);

  // Check the ADC to see what the battery voltage is
  //Serial.println("Giving the ADC a chance to get going...");
  //delay(1000);
  Serial.println("Checking battery...");
  //batteryOK = checkBattery();
  batteryOK = false;
  adc = analogRead(ADC_0);
  Serial.print("ADC reading: ");
  Serial.println(adc);
  if(adc<=CUTOFF) error += 1;
  else batteryOK = true;
  if(batteryOK){
    Serial.println("Battery OK");

    // Get the BME sensor going
    // Initialise the BME sensor
    Serial.println("Initialising sensor...");
    
    /* Now initialise the BME280 */
    Wire.begin();
    /* begin() checks the Interface, reads the sensor ID (to differentiate between 
     BMP280 and BME280) and reads compensation parameters.*/
    int attempts = 5;
    while(!bmx280.begin() && (attempts>0))
    {
      Serial.print("Attempt ");
      Serial.print(6-attempts--);
      Serial.println(": begin() failed. Trying again."); //check your BMx280 Interface and I2C Address.");
      delay(1000);
    }
    if(attempts<1) 
    {
      // Sensor initialisation failed
      Serial.println("begin() failed. Check your BMx280 Interface and I2C Address.");
      temp = -1;
      pres = -1;
      hum  = -1;
      error += 2; // error #2 means we failed to connect to the weather sensor 
    }
    else
    {
      // Sensor initialised; set up sensor parameters
      BMElive = true;
      if (bmx280.isBME280()) Serial.println("sensor is a BME280");
      else Serial.println("sensor is a BMP280");
      //reset sensor to default parameters.
      bmx280.resetToDefaults();
      //by default sensing is disabled and must be enabled by setting a non-zero
      //oversampling setting.
      //set an oversampling setting for pressure and temperature measurements. 
      bmx280.writeOversamplingPressure(BMx280MI::OSRS_P_x16);
      bmx280.writeOversamplingTemperature(BMx280MI::OSRS_T_x16);
      //if sensor is a BME280, set an oversampling setting for humidity measurements.
      if (bmx280.isBME280()) bmx280.writeOversamplingHumidity(BMx280MI::OSRS_H_x16);
    }

    // If we're up and running get the sensor readings (ADC has already been recorded)
    if(BMElive)
    {
      //start a measurement
      if (!bmx280.measure()){
        Serial.println("could not start measurement, is a measurement already running?");
        error += 8; // Error #8 means although the sensor initialised, we couldn't get a reading
      }
      else
      {
        //wait for the measurement to finish
        delay(1000);
        if(bmx280.hasValue())
        {
          temp = bmx280.getTemperature();
          pres = bmx280.getPressure()/100;
          if(bmx280.isBME280()) hum = bmx280.getHumidity();
        }
      }
    }

    // Set up the parameter string for the web exchange
    int prevError = store.error();
    Serial.print("Previous error # was: ");
    Serial.println(prevError);
    readings = "?"+String(CHANNEL)+"="+String(adc)+"&serialNo="+String(serialNo)+"&temp="+String(temp)+"&pres="+String(pres)+"&hum="+String(hum)+"&error="+String(store.error());
    Serial.print("Param String is: ");
    Serial.println(readings);
    Serial.print("Current error is: ");
    Serial.println(error);

    // Now start up the wifi and attempt to submit the data
    wifiConnect();
    // Check for WiFi connection 
    successful = false;
    if ((WiFiMulti.run() == WL_CONNECTED)) 
    {
      WiFiClient client;
      HTTPClient http; // Must be declared after WiFiClient for correct destruction order, because used by http.begin(client,...)
      //trace("\n[HTTP]", "");

      // Set up request url with reading parameter(s)
      urlRequest = "http://argles.org.uk/homelog.php";
      urlRequest += readings;
      // trace("empty = ", String(adcStore.empty));   
      // Now make the request
      http.begin(client, urlRequest);
      // trace("Requesting: ", urlRequest);
      // start connection and send HTTP header
      int httpCode = http.GET();
      if (httpCode > 0) 
      {
        // HTTP header has been sent and Server response header has been handled
        // trace("Return code: ", String(httpCode));
        // file found at server
        if (httpCode == HTTP_CODE_OK) 
        {
          // Serial.println(http.getString());  // Gets the actual page returned
          //trace("Request successful", "");
          //trace("Connection closed", "");
          successful = true;
        }
      }
      // HTTP request failed 
      else{
        error += 16;  // error #16 means the upload to the server failed
        Serial.printf("Request failed, error: %s\n", http.errorToString(httpCode).c_str());
      }
      http.end();
    }
    // If no WiFi...
    //else trace("WiFi failed, code: ", String(WiFiMulti.run()));
  }

  if(error!=0){
    Serial.print("Error #");
    Serial.print(error);
    Serial.println(" occured");
  }
  // Save the error code
  store.setError(error);
  Serial.print("Error set to: ");
  Serial.println(store.error());
  // write the data back to rtc memory
  store.writeData();   
  Serial.println("Going to sleep...");
  // Whether successful or not, we're going to sleep for an hour before trying again!
  ESP.deepSleep(deepSleepTime);  
}

void wifiConnect()
{
  Serial.println("Connecting to Wifi...");
  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(LOCAL_SSID, LOCAL_PWD);
  /* NOTE: the example sketch has this wrong. We need to wait -after- doing
           the WiFiMulti.addAP to give it time to register! */
  Serial.print("[SETUP]\nWAIT ");
  for (uint8_t t = 4; t > 0; t--) {
    Serial.printf("%d...", t);
    Serial.flush();
    delay(1500);
  }
  Serial.println();

  int attempts = 5;
  
  while(WiFiMulti.run() != WL_CONNECTED && (attempts-- >0))
  {
    Serial.println("WiFi not connected");
    delay(1000);
  }
  
  if (WiFiMulti.run() == WL_CONNECTED) 
  {
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
  else{
    error += 4; // error #4 means we couldn't connect to WiFi
    Serial.println("WiFi connection failed!");
  }
  return;
}

/*boolean checkBattery(){
  adc = analogRead(ADC_0);
  if(adc<=CUTOFF) store.setError(1);
  return (adc>CUTOFF);
}*/ 

void loop() {
  // If we get here, something's gone wrong! Let's flash a warning
  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  delay(1000);
}
