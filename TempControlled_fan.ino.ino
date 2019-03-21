//increase MQTT_MAX_PACKET_SIZE  in pubsubclient to allow for larger data packets to be sent.

#include "FS.h"
#include <OneWire.h>
#include <ArduinoJson.h> //Use version 5.x
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DallasTemperature.h>
#include <sigma_delta.h>
#include <credentials.h> // File defining all credentials e.g: mySSID, mySSIDPASSWORD
//If the MQTT port is defined as 8883 then load WiFiClientSecure to support MQTT + TLS 
//#include <WiFiClientSecure.h>


const char* configurationFile = "/config.cfg";

const char* ssid = mySSID;
const char* pass = mySSIDPASSWORD;

char mqtt_server[40] = myMQTTSERVER;
int mqtt_server_port = myMQTTPORT;
char mqtt_clientName[40] = "CVFAN3.6";
char mqtt_username[40] = myMQTTUSER;
char mqtt_password[40] = myMQTTPASSWORD;


//If the MQTT port is defined as 8883 then load WiFiClientSecure to support MQTT + TLS 
WiFiClient wifiClient;
//WiFiClientSecure wifiClient;
PubSubClient client(mqtt_server, mqtt_server_port , wifiClient);

char configTopic[] = "living/cvfan/config";
char logTopic[] = "log/living/cvfan";

char configfanstarttemp[] = "living/cvfan/config/fanstarttemp";
char configfanstoptemp[] = "living/cvfan/config/fanstoptemp";
char configfanfullspeedtemp[] = "living/cvfan/config/fanfullspeedtemp";
char configminfanspeed[] = "living/cvfan/config/minfanspeed";
char configmaxfanspeed[] = "living/cvfan/config/maxfanspeed";
char configforcefan[] = "living/cvfan/config/forcefan";
char configforcefanspeed[] = "living/cvfan/config/forcefanspeed";
char configdebug[] = "living/cvfan/config/debug";
char configpwmfrequency[] = "living/cvfan/config/pwmfrequency";

struct Configuration
{
  byte fanStartTemp;
  byte fanStopTemp;
  byte fanFullSpeedTemp;
  int minFanSpeed;
  int maxFanSpeed;
  bool forceFan;
  int forceFanSpeed;
  bool debug;
  int pwmRange;
  int pwmFrequency;
};

Configuration configuration;

OneWire oneWireInput(D4);
OneWire oneWireOutput(D5);
OneWire oneWireAmbient(D6);
DallasTemperature sensorInput(&oneWireInput);
DallasTemperature sensorOutput(&oneWireOutput);
DallasTemperature sensorAmbient(&oneWireAmbient);


struct Sensor
{
  char* topic;
  float oldValue = 0;
  float value = 0;
};

Sensor currentInputTemp, currentOutputTemp, currentAmbientTemp, currentDelta, calculatedPWM, uptime, pingpong;

const byte fanpin = D7;

bool loadConfig();
bool saveConfig();
void setup_wifi();
void sendMQTTMessage(char* topic, char* message, bool persistent);

unsigned long lastSendConfigurationMillis = 0;


void setup() {
  Serial.begin(115200);
  Serial.println("serial started");
  pinMode(fanpin, OUTPUT);
  sigmaDeltaSetup(0, 1000);
  sigmaDeltaAttachPin(fanpin);
  sigmaDeltaWrite(0,254);
  pinMode(D4, OUTPUT);
  pinMode(D5, OUTPUT);
  pinMode(D6, OUTPUT);
  //digitalWrite(fanpin, HIGH);
  digitalWrite(D4, HIGH);
  digitalWrite(D4, HIGH);
  digitalWrite(D4, HIGH);

  DeviceAddress deviceAddress;
  sensorInput.begin();
  sensorInput.getAddress(deviceAddress, 0);
  sensorInput.setResolution(deviceAddress, 12);

  sensorOutput.begin();
  sensorOutput.getAddress(deviceAddress, 0);
  sensorOutput.setResolution(deviceAddress, 12);

  sensorAmbient.begin();
  sensorAmbient.getAddress(deviceAddress, 0);
  sensorAmbient.setResolution(deviceAddress, 12);

  setup_wifi();

  SPIFFS.begin();

  loadConfig();

  currentInputTemp.topic = "living/cvfan/input";
  currentOutputTemp.topic = "living/cvfan/output";
  currentDelta.topic = "living/cvfan/delta";
  currentAmbientTemp.topic = "living/cvfan/ambient";
  calculatedPWM.topic = "living/cvfan/fanspeed";
  uptime.topic = "living/cvfan/uptime";
  pingpong.topic = "living/cvfan/pingpong";

  if (!isValidConfig(configuration))
  {
    Serial.println("Invalid configuration, formatting and recreating default config" );
    SPIFFS.format();
    configuration.fanStartTemp = 26;
    configuration.fanStopTemp = 25;
    configuration.fanFullSpeedTemp = 36;
    configuration.minFanSpeed = 40;
    configuration.maxFanSpeed = 65;
    configuration.forceFan = false;
    configuration.forceFanSpeed = 65;
    configuration.debug = true;
    configuration.pwmRange = 100;
    configuration.pwmFrequency = 14000;
    saveConfig();
  }
  sendMQTTMessage("pushbullet/living/cvfan", "Living CV fan booted v3.6", false);
  saveConfig();
  applyConfig();
  sendConfiguration();

  //Disable software WDT so the board reboots when it crashes due to a lost internet connection
  ESP.wdtDisable();
}

void loop() {
  if(client.connected())
  {
  client.loop();
  }
  
  if (configuration.debug) Serial.println("loop;Measuring temps");
  measureTemps();
  if (configuration.debug) Serial.println("loop;Setting fanspeed");
  setFanSpeed();
  if (configuration.debug) Serial.println("loop;Updating millis");
  uptime.value = millis();
  if (configuration.debug) Serial.println("loop;Sending WirelessData");
  if (pingpong.value == 0) {
    pingpong.value = 1;
  } else {
    pingpong.value = 0;
  }
  SendWirelessData();
}

void measureTemps()
{
  sensorInput.requestTemperatures();
  sensorOutput.requestTemperatures();
  sensorAmbient.requestTemperatures();
  digitalWrite(D4, HIGH);
  digitalWrite(D5, HIGH);
  digitalWrite(D6, HIGH);
  delay2(750);

  float tempValue;

  tempValue = sensorInput.getTempCByIndex(0);
  if (tempValue != -100 && tempValue != -127) currentInputTemp.value = tempValue;
  delay2(0);

  tempValue = sensorOutput.getTempCByIndex(0);
  if (tempValue != -100 && tempValue != -127) currentOutputTemp.value = tempValue;
  delay2(0);

  tempValue = sensorAmbient.getTempCByIndex(0);
  if (tempValue != -100 && tempValue != -127) currentAmbientTemp.value = tempValue;
  delay2(0);

  currentDelta.value = currentInputTemp.value - currentOutputTemp.value;

  if (configuration.debug) Serial.print("measureTemps;currentInputTemp;");
  if (configuration.debug) Serial.println(currentInputTemp.value);
  if (configuration.debug) Serial.print("measureTemps;currentOutputTemp;");
  if (configuration.debug) Serial.println(currentOutputTemp.value);
  if (configuration.debug) Serial.print("measureTemps;currentDelta;");
  if (configuration.debug) Serial.println(currentDelta.value);
  if (configuration.debug) Serial.print("measureTemps;currentAmbientTemp;");
  if (configuration.debug) Serial.println(currentAmbientTemp.value);
  digitalWrite(D4, HIGH);
  digitalWrite(D5, HIGH);
  digitalWrite(D6, HIGH);
}

void setFanSpeed()
{
  //Forced fan overrides other logic
  if (configuration.forceFan)
  {
    calculatedPWM.value = configuration.forceFanSpeed;
    if (calculatedPWM.value > configuration.maxFanSpeed) calculatedPWM.value = configuration.maxFanSpeed;
    if (calculatedPWM.value < configuration.minFanSpeed) calculatedPWM.value = configuration.minFanSpeed;
  } else {
    if (currentInputTemp.value < configuration.fanStopTemp)calculatedPWM.value = 0;
    if (currentInputTemp.value > configuration.fanStartTemp)
    {
      calculatedPWM.value = map(currentInputTemp.value * 100, configuration.fanStartTemp * 100, configuration.fanFullSpeedTemp * 100 , configuration.minFanSpeed * 100 , configuration.maxFanSpeed * 100) / 100;
      if (calculatedPWM.value > configuration.maxFanSpeed) calculatedPWM.value = configuration.maxFanSpeed;
      if (calculatedPWM.value < configuration.minFanSpeed) calculatedPWM.value = configuration.minFanSpeed;
    }
  }

  if (calculatedPWM.value > configuration.pwmRange) calculatedPWM.value = configuration.pwmRange;

  if (configuration.debug) Serial.print("setFanSpeed;");
  if (configuration.debug) Serial.println(calculatedPWM.value);
  
  //analogWrite(fanpin, calculatedPWM.value);
  sigmaDeltaWrite(0, map(calculatedPWM.value, 0, 100, 0, 254));
}

void SendWirelessData()
{
  sendSensor(currentInputTemp);
  sendSensor(currentOutputTemp);
  sendSensor(currentAmbientTemp);
  sendSensor(currentDelta);
  sendSensor(calculatedPWM);
  sendSensor(uptime);
  sendSensor(pingpong);
}




char *ftoa(char *a, double f, int precision)
{
  long p[] = {0, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000};

  char *ret = a;
  long heiltal = (long)f;
  itoa(heiltal, a, 10);
  while (*a != '\0') a++;
  *a++ = '.';
  long desimal = abs((long)((f - heiltal) * p[precision]));
  itoa(desimal, a, 10);
  return ret;
}

//Do some maintenance while sleeping
void delay2(unsigned long duration)
{
  unsigned long startMillis = millis();
  do
  {
    yield();
    client.loop();
    ESP.wdtFeed();
  }while ((millis() - startMillis) < duration);
}

bool isValidConfig(Configuration c)
{
  //These values cannot be 0
  if (c.fanStartTemp <= 0 || c.fanStopTemp <= 0 || c.fanFullSpeedTemp <= 0 || c.minFanSpeed <= 0 || c.maxFanSpeed <= 0 || c.forceFanSpeed <= 0 || c.pwmRange <= 0 || c.pwmFrequency <= 0)
  {
    if (configuration.debug) Serial.print("ConfigValidation;");
    if (configuration.debug) Serial.println("Invalid configuration detected reason: fanStartTemp <= 0 || fanStopTemp <= 0 || fanFullSpeedTemp <= 0 || minFanSpeed <= 0 || maxFanSpeed <= 0 || forceFanSpeed <= 0 || pwmRange <= 0 || pwmFrequency <= 0");
    return false;
  }

  //start temperature must be smaller then stop temp must be smaller then full speed temp
  if (!(c.fanStartTemp < c.fanStopTemp < c.fanFullSpeedTemp))
  {
    if (configuration.debug) Serial.print("ConfigValidation;");
    if (configuration.debug) Serial.println("Invalid configuration detected reason: fanStartTemp < fanStopTemp < fanFullSpeedTemp");
    return false;
  }

  //minimum speed must be higher then max speed
  if (!(c.minFanSpeed < c.maxFanSpeed))
  {
    if (configuration.debug) Serial.print("ConfigValidation;");
    if (configuration.debug) Serial.println("Invalid configuration detected reason: minFanSpeed < maxFanSpeed");
    return false;
  }

  //pwm frequency must be smaller then 20.000hz
  if (!(c.pwmFrequency <= 20000))
  {
    if (configuration.debug) Serial.print("ConfigValidation;");
    if (configuration.debug) Serial.println("Invalid configuration detected reason: pwmFrequency <= 20000");
    return false;
  }
  if (configuration.debug) Serial.println("ConfigValidation;Config Valid");
  return true;
}

void applyConfig()
{
  analogWriteRange(configuration.pwmRange);
  analogWriteFreq(configuration.pwmFrequency);
}

bool areConfigsEqual(Configuration c1, Configuration c2)
{
  if (c1.fanStartTemp != c2.fanStartTemp) return false;
  if (c1.fanStopTemp != c2.fanStopTemp)return false;
  if (c1.fanFullSpeedTemp != c2.fanFullSpeedTemp) return false;
  if (c1.minFanSpeed != c2.minFanSpeed) return false;
  if (c1.maxFanSpeed != c2.maxFanSpeed) return false;
  if (c1.forceFan != c2.forceFan) return false;
  if (c1.forceFanSpeed != c2.forceFanSpeed)  return false;
  if (c1.debug != c2.debug) return false;
  if (c1.pwmRange != c2.pwmRange) return false;
  if (c1.pwmFrequency != c2.pwmFrequency) return false;
  return true;
}

void sendConfiguration()
{
  //Only send configuration if the last one was send more then one second ago (To mittigate configuration trigger loop)
  if (millis() - lastSendConfigurationMillis > 1000)
  {
    char buf[10];
    sendMQTTMessage(configfanstarttemp, ftoa(buf, configuration.fanStartTemp, 2), false);
    sendMQTTMessage(configfanstoptemp, ftoa(buf, configuration.fanStopTemp, 2), false);
    sendMQTTMessage(configfanfullspeedtemp, ftoa(buf, configuration.fanFullSpeedTemp, 2), false);
    sendMQTTMessage(configminfanspeed, ftoa(buf, configuration.minFanSpeed, 2), false);
    sendMQTTMessage(configmaxfanspeed, ftoa(buf, configuration.maxFanSpeed, 2), false);
    sendMQTTMessage(configforcefan, ftoa(buf, configuration.forceFan, 2), false);
    sendMQTTMessage(configforcefanspeed, ftoa(buf, configuration.forceFanSpeed, 2), false);
    sendMQTTMessage(configdebug, ftoa(buf, configuration.debug, 2), false);
    sendMQTTMessage(configpwmfrequency, ftoa(buf, configuration.pwmFrequency, 2), false);
    lastSendConfigurationMillis = millis();
  }
}
