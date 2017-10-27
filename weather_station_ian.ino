#include <SimpleTimer.h>

#include <math.h>
//Include LCD library
#include <LiquidCrystal.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include "DHT.h"

#define DHTPIN D7
#define DHTTYPE DHT22

#define LDRPIN D8

DHT dht(DHTPIN, DHTTYPE);
HTTPClient http;

const char *ssid = "";
const char *password = "";
String httpPostUrl = "http://iot-open-server.herokuapp.com";
String deviceId = "";
String apiToken = "";
const bool POST = true;

float humidity = 0;
float temperature = 0;

const int sensorPin = A0; //Defines the pin that the anemometer output is connected to
int sensorValue = 0; //Variable stores the value direct from the analog pin
float sensorVoltage = 0; //Variable that stores the voltage (in Volts) from the anemometer being sent to the analog pin
float windSpeed = 0; // Wind speed in meters per second (m/s)

float voltageConversionConstant = 0.0032; //This constant maps the value provided from the analog read function, which ranges from 0 to 1023, to actual voltage, which ranges from 0V to 5V

float voltageMin = 0.4; // Mininum output voltage from anemometer in mV.
float windSpeedMin = 0; // Wind speed in meters/sec corresponding to minimum voltage

float voltageMax = 2.0; // Maximum output voltage from anemometer in mV.
float windSpeedMax = 32; // Wind speed in meters/sec corresponding to maximum voltage

int ldrValue = 0;

int humidityThreshold = 65;
int humidityThresholdTimer = 1000 * 5 * 1; // 5 seconds
bool triggered = false;
int timer = 0;

const bool ENABLE_ACTUATOR = true;
//const String actuatorPostUrl = "https://eu-wap.tplinkcloud.com?token=";
const String actuatorPostUrl = "";
const String actuatorDeviceId = "";
String lastTrigger = "-1";

int displayTimer = 0;
int currentDisplay = 0;

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(D6, D5, D4, D3, D2, D1);

void setup() {
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  Serial.begin(115200);
  dht.begin();
  if (POST) {
    connectToWifiNetwork();
  }
}

void loop() {
  // set the cursor to column 0, line 1
  //  lcd.setCursor(0, 1);

  StaticJsonBuffer<400> jsonBuffer;
  JsonObject& jsonRoot = jsonBuffer.createObject();

  jsonRoot["token"] = apiToken;

  JsonArray& data = jsonRoot.createNestedArray("data");

  // DHT sensor
  float _humidity = dht.readHumidity();

  if (isnan(_humidity) != 1) {
    JsonObject& humidityObject = jsonBuffer.createObject();
    humidityObject["key"] = "humidity";
    humidityObject["value"] = _humidity;

    data.add(humidityObject);

    humidity = _humidity;
  }

  float _temperature = dht.readTemperature();
  if (isnan(_temperature) != 1) {
    JsonObject& temperatureObject = jsonBuffer.createObject();
    temperatureObject["key"] = "temperature";
    temperatureObject["value"] = _temperature;

    data.add(temperatureObject);

    temperature = _temperature;
  }

  sensorValue = analogRead(A0); //Get a value between 0 and 1023 from the analog pin connected to the anemometer

  sensorVoltage = sensorValue * voltageConversionConstant; //Convert sensor value to actual voltage

  //Convert voltage value to wind speed using range of max and min voltages and wind speed for the anemometer

  if (sensorVoltage <= voltageMin) {
    windSpeed = 0; //Check if voltage is below minimum value. If so, set wind speed to zero.
  } else {
    windSpeed = ((sensorVoltage - voltageMin) * windSpeedMax / (voltageMax - voltageMin) * 2.23694);
  }
  //For voltages above minimum value, use the linear relationship to calculate wind speed.

  if (isnan(windSpeed) != 1) {
    JsonObject& windSpeedObject = jsonBuffer.createObject();
    windSpeedObject["key"] = "windforce";
    windSpeedObject["value"] = windSpeed;

    data.add(windSpeedObject);
  }

  ldrValue = digitalRead(LDRPIN);

  JsonObject& ldrObject = jsonBuffer.createObject();
  ldrObject["key"] = "ldr";
  ldrObject["value"] = ldrValue;

  data.add(ldrObject);

  String dataToSend;
  jsonRoot.printTo(dataToSend);

  //  Serial.println(dataToSend);

  postData(dataToSend);

  Serial.print("LDR: ");
  Serial.print(ldrValue);
  Serial.print(" | Humidity: ");
  Serial.print(humidity);
  Serial.print(" | Temperature: ");
  Serial.print(temperature);
  Serial.print(" | Wind speed: ");
  Serial.println(windSpeed);

  checkActuator();

  handleDisplay();

  delay(2000);
}

void checkActuator() {
  if (ldrValue == HIGH && isnan(humidity) != 1) {
    if (humidity < humidityThreshold) {
      Serial.println("Checking timer");
      if(timer + humidityThresholdTimer < millis() && triggered == false) {
        Serial.println("Done with timer");
        http.begin(httpPostUrl + "/actuator/" + deviceId);
        http.addHeader("Content-Type", "application/json");
        http.POST("{\"actuator\":\"0\"}");
        String resposne = http.getString();
        Serial.println(resposne);
        timer = millis();
        triggered = true;
      }
    } else {
      Serial.println("Reset timer");
      timer = millis();
    }
  } else {
      timer = millis();
  }

  if (WiFi.status() == WL_CONNECTED) {
    http.begin(httpPostUrl + "/actuator/" + deviceId);
    http.addHeader("Content-Type", "application/json");
    http.GET();
    String payload = http.getString();

    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(payload);
    String trigger = root["trigger"];
    if(trigger != lastTrigger) {
      toggleActuator(trigger);
      lastTrigger = trigger;
    }

    http.end();
  } else {
    Serial.println("Wifi connection failed, retrying.");
    connectToWifiNetwork();
  }
}

void handleDisplay() {
  int currentTime = millis();
  if(currentTime > displayTimer + 5000) {
    displayTimer = currentTime;
    currentDisplay++;
    if(currentDisplay > 3) {
      currentDisplay = 0;
    }
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  switch(currentDisplay) {
    case 0:
      lcd.print("Welcome to Ian's");
      lcd.setCursor(0, 1);
      lcd.print("Weather Station!");
      break;
    case 1:
      lcd.print("Temperature:");
      lcd.setCursor(0, 1);
      lcd.print(temperature);
      lcd.print((char)223);
      lcd.print("C");
      break;
    case 2:
      lcd.print("Humidity:");
      lcd.setCursor(0, 1);
      lcd.print(humidity);
      lcd.print("%");
      break;
    case 3:
      lcd.print("Wind speed:");
      lcd.setCursor(0, 1);
      lcd.print(windSpeed);
      lcd.print("m/s");
      break;
  }
}

void toggleActuator(String state) {
  if (!ENABLE_ACTUATOR) {
    return;
  }
  if (WiFi.status() == WL_CONNECTED) {
    StaticJsonBuffer<400> jsonBuffer;
    JsonObject& jsonRoot = jsonBuffer.createObject();

    jsonRoot["method"] = "passthrough";
    jsonRoot["state"] = state;

    JsonObject& paramsObject = jsonBuffer.createObject();
    paramsObject["deviceId"] = actuatorDeviceId;
    paramsObject["requestData"] = "{\"system\":{\"set_relay_state\":{\"state\":" + state + "}}}";

    jsonRoot["params"] = paramsObject;

    String stringToPost;
    jsonRoot.printTo(stringToPost);

    Serial.println("Setting actuator state to " + state + "....");
    http.begin(actuatorPostUrl + "&state=" + state);
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.POST(stringToPost);
    http.end();
    triggered = false;
  } else {
    Serial.println("Wifi connection failed, retrying.");
    connectToWifiNetwork();
  }
}

void connectToWifiNetwork() {
  Serial.println ( "Trying to establish WiFi connection" );
  WiFi.begin ( ssid, password );
  Serial.println ( "" );

  // Wait for connection
  while ( WiFi.status() != WL_CONNECTED ) {
    delay ( 500 );
    Serial.print ( "." );
  }

  Serial.println ( "" );
  Serial.print ( "Connected to " );
  Serial.println ( ssid );
}

void postData(String stringToPost) {
  if (!POST) {
    return;
  }
  if (WiFi.status() == WL_CONNECTED) {
    http.begin(httpPostUrl + "/data");
    http.addHeader("Content-Type", "application/json");
    http.POST(stringToPost);
    http.end();
  } else {
    Serial.println("Wifi connection failed, retrying.");
    connectToWifiNetwork();
  }
}

