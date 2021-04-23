/******************************************************/
//       THIS IS A GENERATED FILE - DO NOT EDIT       //
/******************************************************/

#include "Particle.h"
#line 1 "c:/Users/Killi/Documents/IoT/l14_moisture-KalebGlodowski/L14_04_PlantWater/src/L14_04_PlantWater.ino"
/*
 * Project L14_04_PlantWater
 * Description: plant watering system
 * Author: Kaleb Glodowski
 * Date: 20-APR-2021
 */

#include <Adafruit_MQTT.h>
#include "Adafruit_MQTT/Adafruit_MQTT.h" 
#include "Adafruit_MQTT/Adafruit_MQTT_SPARK.h" 
#include "Adafruit_MQTT/Adafruit_MQTT.h" 
#include <neopixel.h>
#include <colors.h>
#include <Adafruit_BME280.h>
#include <Adafruit_SSD1306.h>
#include <Air_Quality_Sensor.h>
#include "credentials.h"

void setup();
void loop();
void MQTT_connect();
void _waterPump();
void _waterPumpOverride();
void _soilSensor();
void _BMESensor();
void MQTT_Subscribe();
void MQTT_Publish();
void _dustSensor();
void _AQSensor();
void _dateTime();
void _OLED_AllData();
#line 19 "c:/Users/Killi/Documents/IoT/l14_moisture-KalebGlodowski/L14_04_PlantWater/src/L14_04_PlantWater.ino"
#define OLED_RESET D4

/************ Global State (you don't need to change this!) ***   ***************/ 
TCPClient TheClient; 

/************Declare Constants*************/

const int SCREEN_ADDRESS = 0x3C;
const int BME_ADDRESS = 0x76;
const int MOISTUREPIN = A0;
const int WATERPIN = D7;
const int AQPIN = A2;
const int DUSTPIN = A1;

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details. 
Adafruit_MQTT_SPARK mqtt(&TheClient,AIO_SERVER,AIO_SERVERPORT,AIO_USERNAME,AIO_KEY); 

/****************************** Feeds ***************************************/ 
// Setup Feeds to publish or subscribe 
// Notice MQTT paths for AIO follow the form: <username>/feeds/<feedname> 
Adafruit_MQTT_Subscribe waterPumpFeed = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/waterPump");
Adafruit_MQTT_Subscribe waterPumpOverrideFeed = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/waterPumpOverride");  
Adafruit_MQTT_Publish temperatureFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/roomTemperature");
Adafruit_MQTT_Publish moistureFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/moistureLevels");
Adafruit_MQTT_Publish pressureFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/pressureCheck");
Adafruit_MQTT_Publish humidityFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/humidityCheck");
Adafruit_MQTT_Publish dustFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/dustCheck");
Adafruit_MQTT_Publish AQ_qualFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/AQQualCheck");
Adafruit_MQTT_Publish AQ_quantFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/AQQuantCheck");

/************Defining other objects**************/

Adafruit_SSD1306 oled(OLED_RESET);
Adafruit_BME280 bme; // I2C
AirQualitySensor airqualitysensor(AQPIN);

/************Declare Variables*************/
unsigned long last;
float value;
unsigned int lastTime;
int BMEStatus;
int tempC;
int pressPA;
int humidRH;
float tempF;
float pressHG;
unsigned int BME_Timer;
unsigned int moisture_Timer;
float moistureReading;
bool pumpOn;
unsigned int pumpTimer;
bool pumpCanBeOn;
unsigned long duration;
unsigned long int lowpulseoccupancy;
unsigned int dustTimer;
float ratio;
float concentration;
int currentAQ;
unsigned int AQTimer;
int AQValue;
String qualitativeAQ;
String DateTime, TimeOnly;
char currentDateTime[25], currentTime[9];
unsigned int OLED_Timer1;
unsigned int OLED_Timer2;
unsigned int OLED_TimeStart;
bool hasDisplayed;
bool pumpCanBeOn_Override;

SYSTEM_MODE(AUTOMATIC);

void setup() {
  Particle.connect(); //connecting to particle cloud
  Time.zone ( -4) ; //EST = -4 MST = -7, MDT = -6
  Particle.syncTime () ; // Sync time with Particle Cloud

  pinMode (MOISTUREPIN, INPUT);
  pinMode (WATERPIN, OUTPUT);
  pinMode (DUSTPIN, INPUT);

  Serial.begin(9600);
  delay(100); //wait for Serial Monitor to startup

  if (airqualitysensor.init()) {
    Serial.println("Sensor ready.");
  }
  else {
    Serial.println("Sensor ERROR!");
  }

  oled.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS); //NEED THIS!
  oled.display();
  delay(3000);
  oled.clearDisplay();
  oled.display();
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  oled.setCursor(0,0);

  BMEStatus = bme.begin(BME_ADDRESS); //REQUIRED LINE TO BEGIN BME WITH HEX ADDRESS OF BME
  if (BMEStatus == false) {
    Serial.printf("BME280 not detected. Check BME address and wiring.\n");
  }  

  Serial.printf("Connecting to Internet \n");
  WiFi.connect();
  while(WiFi.connecting()) {
    Serial.printf(".");
    delay(100);
  }
  Serial.printf("\n Connected!!!!!! \n");
  // Setup MQTT subscription for onoff feed.
  mqtt.subscribe(&waterPumpFeed);
  mqtt.subscribe(&waterPumpOverrideFeed);

  pumpCanBeOn = true;
  currentAQ = -1;
}

void loop() {
  _waterPump();       //water pump that automatically waters the plant OR takes manually input from dashboard
  _OLED_AllData();    //main bulk of the code and writes to OLED
  MQTT_connect();     //validate connected to MQTT
  MQTT_Subscribe();   //this is our 'wait for incoming subscription packets' busy subloop
  MQTT_Publish();     //publishes all data to the dashboard
}

void MQTT_connect() {
  // Function to connect and reconnect as necessary to the MQTT server.
  // Should be called in the loop function and it will take care if connecting.
  int8_t ret;
 
  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }
 
  Serial.print("Connecting to MQTT... ");
 
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println("Retrying MQTT connection in 5 seconds...");
       mqtt.disconnect();
       delay(5000);  // wait 5 seconds
  }
  Serial.println("MQTT Connected!");

  //pinging MQTT to keep connection open
  if ((millis()-last)>120000) {
    Serial.printf("Pinging MQTT \n");
    if(! mqtt.ping()) {
      Serial.printf("Disconnecting \n");
      mqtt.disconnect();
      }
      last = millis();
  }
}

void _waterPump() {
  if (millis() > (pumpTimer + 600000)) {     //allows the pump to be turned on only once every 10 minutes
    pumpCanBeOn = true;
    pumpTimer = millis();
  }    
  if (pumpCanBeOn == true && moistureReading > 2500) { 
    Serial.println("Water pump turning on for 1/2 second.");
    digitalWrite(WATERPIN, HIGH);
    delay(500);
    digitalWrite(WATERPIN, LOW);
    pumpCanBeOn = false;
  }
}

void _waterPumpOverride() {
  if (pumpCanBeOn_Override == true) {
    Serial.println("Water pump manual override pressed. Plant may not need water.");
    Serial.println("Water pump turning on for 1/2 second.");
    digitalWrite(WATERPIN, HIGH);
    delay(500);
    digitalWrite(WATERPIN, LOW);
    pumpCanBeOn_Override = false;
    pumpCanBeOn = false;  //ensures pump will not double-tap
  }
}

void _soilSensor() {
  if(millis() > moisture_Timer + 5000) {
    moistureReading = analogRead(MOISTUREPIN);
    Serial.printf("Moisture Reads: %f\n", moistureReading);       
    moisture_Timer = millis();
  }  
}

void _BMESensor() {
  if (millis() > (BME_Timer + 5000)) { //every 5s
    tempC = bme.readTemperature(); //reading temp in celsius
    tempF = ((tempC * 9/5) + 32);  //converting to fahrenheit
    Serial.printf("Temperature is: %0.2f.\n", tempF);
    pressPA =  bme.readPressure(); //reading pressure in pascals
    pressHG = pressPA/3333.3333; //converting pascals to inches of mercury (inHG)
    Serial.printf("Pressure is: %0.2f.\n", pressHG);
    humidRH = bme.readHumidity();
    Serial.printf("Humidity is: %i.\n", humidRH);
    BME_Timer = millis();      
  }  
}

void MQTT_Subscribe() {
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(1000))) {
    if (subscription == &waterPumpFeed) {
      value = atof((char *)waterPumpFeed.lastread);
      pumpCanBeOn = true; //overrides 30 second cooldown on pump
      _waterPump();
    }
    if (subscription == &waterPumpOverrideFeed) {
      value = atof((char *)waterPumpOverrideFeed.lastread);
      pumpCanBeOn_Override = true;
      _waterPumpOverride();
    }
  }  
}

void MQTT_Publish() {
  if((millis()-lastTime > 60000)) { //publish every 60 seconds
    if(mqtt.Update()) {
      temperatureFeed.publish(tempF);
      Serial.printf("Publishing %f \n",tempF);
      moistureFeed.publish(moistureReading);
      Serial.printf("Publishing %f \n",moistureReading);
      pressureFeed.publish(pressHG);
      Serial.printf("Publishing %f \n",pressHG);  
      humidityFeed.publish(humidRH);
      Serial.printf("Publishing %i \n",humidRH);
      dustFeed.publish(concentration);
      Serial.printf("Publishing %f \n",concentration);
      AQ_qualFeed.publish(currentAQ);
      Serial.printf("Publishing %i \n",currentAQ);
      AQ_quantFeed.publish(AQValue);
      Serial.printf("Publishing %i \n",AQValue);                              
      } 
    lastTime = millis();
  }
}

void _dustSensor() {
    duration = pulseIn(DUSTPIN, LOW);
    lowpulseoccupancy = lowpulseoccupancy+duration;
    if (millis() > dustTimer + 30000) {
      ratio = lowpulseoccupancy/(30000*10.0);  // Integer percentage 0=>100, 30s
      concentration = 1.1*pow(ratio,3)-3.8*pow(ratio,2)+520*ratio+0.62; // using spec sheet curve
      if (concentration < 1) {
        concentration = 0; //sets concentration to 0 when reading nothing
      }
      Serial.printf("Dust Sensor LowPulseOccupancy: %lu.\n", lowpulseoccupancy); //%lu is how to print an unsigned long int
      Serial.printf("Dust Sensor Ratio: %f.\n", ratio);
      Serial.printf("Dust Sensor Concentration: %f.\n", concentration);  
      lowpulseoccupancy = 0;
      dustTimer = millis();      
    }    
}

void _AQSensor() {
    if (millis() > AQTimer + 5000) {
      currentAQ = airqualitysensor.slope(); //3 is fresh, 2 is medium quality, 1 is low quality, 0 is super low quality
      AQValue = airqualitysensor.getValue();
      Serial.printf("Air Quality qualitative reading: %i.\n", currentAQ);
      Serial.printf("Air Quality quantitative value: %i.\n", AQValue);
      AQTimer = millis();

      if (currentAQ==0) {
        Serial.println("Very high pollution! Force signal active");
        qualitativeAQ = "Very high pollution"; 
      }
      if (currentAQ==1) {
        Serial.println("High pollution!");
        qualitativeAQ = "High Pollution";
      }
      if (currentAQ==2) {
        Serial.println("Low pollution!");
        qualitativeAQ = "Low pollution";        
      }
      if (currentAQ==3) {
        Serial.println("Fresh air");
        qualitativeAQ = "Fresh air";         
      }
    }
}

void _dateTime() {
  DateTime = Time.timeStr();            //current data and time from particle time class
  TimeOnly = DateTime.substring(11,19); //Extract the time from the datetime string
  //Convert String to char arrays - this is needed for formatted print
  DateTime.toCharArray(currentDateTime, 25);
  TimeOnly.toCharArray(currentTime, 9);
  //Print using formatted print
  Serial.printf("Date and time is %s.\n", currentDateTime);
  Serial.printf("Time is %s.\n", currentTime);
}

void _OLED_AllData() {
  _BMESensor();
  _soilSensor();
  _dustSensor();
  _AQSensor();  
  _dateTime();
  //Clear OLED
  oled.clearDisplay();
  oled.display();
  //OLED Settings
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  oled.setCursor(0,0);
  OLED_Timer1 = 0;
  OLED_TimeStart = millis();
  hasDisplayed = false;
  while (OLED_Timer1 < 5000) { //stays in this loop until OLED_Timer1 hits 5 seconds
    if (hasDisplayed == false) {
    //Date/Time
      oled.printf("Date and time is %s.\n", currentDateTime);

    //BME
      oled.printf("Temperature: %0.2f.\n", tempF);
      oled.printf("Pressure: %0.2f.\n", pressHG);
      oled.printf("Humidity: %i.\n", humidRH); 
      
      oled.display();
      hasDisplayed = true;
    }

    OLED_Timer1 = (millis() - OLED_TimeStart);
    delay(50);
  }
  //Clear OLED
  oled.clearDisplay();
  oled.display();
  //OLED Settings
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  oled.setCursor(0,0);
  OLED_Timer2 = 0;
  OLED_TimeStart = millis();
  hasDisplayed = false;
  while (OLED_Timer2 < 5000) { //stays in this loop until OLED_Timer1 hits 5 seconds
    if (hasDisplayed == false) {
    //Date/Time
      oled.printf("Date and time is %s.\n", currentDateTime);  

    //Soil Sensor
      oled.printf("Moisture: %0.2f.\n", moistureReading);

    //Dust Sensor
      oled.printf("Dust: %0.2f.\n", concentration);

    //AQ Sensor
      oled.printf("AQ Value: %i.\n", AQValue);
      oled.printf("AQ: %s.\n", qualitativeAQ.c_str());

      oled.display();
      hasDisplayed = true;
    }
    OLED_Timer2 = millis();
    delay(50);  
  }
}