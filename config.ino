bool loadConfig() {
  File configFile = SPIFFS.open(configurationFile, "r");
  if (!configFile) {
    saveConfig();
    if (configuration.debug) Serial.println("Failed to open config file");
    return false;
  }

  size_t size = configFile.size();
  if (size > 1024) {
    if (configuration.debug) Serial.println("Config file size is too large");
    return false;
  }

  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  // We don't use String here because ArduinoJson library requires the input
  // buffer to be mutable. If you don't use ArduinoJson, you may as well
  // use configFile.readString instead.
  configFile.readBytes(buf.get(), size);

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf.get());

  if (!json.success()) {
    if (configuration.debug) Serial.println("Failed to parse config file");
    return false;
  }

  //load config to struct
  if (configuration.debug) Serial.println("loading config");

  configuration.fanStartTemp = json["fanStartTemp"];
  configuration.fanStopTemp = json["fanStopTemp"];
  configuration.fanFullSpeedTemp = json["fanFullSpeedTemp"];
  configuration.minFanSpeed = json["minFanSpeed"];
  configuration.maxFanSpeed = json["maxFanSpeed"];
  configuration.forceFan = json["forceFan"].as<bool>();
  configuration.forceFanSpeed  = json["forceFanSpeed"];
  configuration.debug  = json["debug"].as<bool>();
  configuration.pwmRange  = json["pwmRange"];
  configuration.pwmFrequency  = json["pwmFrequency"];

  if (configuration.debug) Serial.println("Configuration file loaded");

  printJsonConfig(json);

  return true;
}

bool saveConfig() {
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();

  //get config from struct
  json["fanStartTemp"] = configuration.fanStartTemp;
  json["fanStopTemp"] = configuration.fanStopTemp;
  json["fanFullSpeedTemp"] = configuration.fanFullSpeedTemp;
  json["minFanSpeed"] = configuration.minFanSpeed;
  json["maxFanSpeed"] = configuration.maxFanSpeed;
  json["forceFan"] = configuration.forceFan;
  json["forceFanSpeed"] = configuration.forceFanSpeed;
  json["debug"] = configuration.debug;
  json["pwmRange"] = configuration.pwmRange;
  json["pwmFrequency"] = configuration.pwmFrequency;

  File configFile = SPIFFS.open(configurationFile, "w");
  if (!configFile) {
    if (configuration.debug) Serial.println("Failed to open config file for writing");
    return false;
  }

  json.printTo(configFile);
  if (configuration.debug) json.printTo(Serial);
  if (configuration.debug) Serial.println("");
  if (configuration.debug) Serial.println("Configuration file saved");

  printJsonConfig(json);

  return true;
}

void printJsonConfig(JsonObject& json)
{
  char mqttMessage[200];

  json.printTo(mqttMessage);
  sendMQTTMessage(configTopic, mqttMessage, false);
}


//bool saveConfig(char* charJson)
//{
//
//  StaticJsonBuffer<200> jsonBuffer;
//  JsonObject& json = jsonBuffer.parseObject(charJson);
//
//  if (!json.success()) {
//    if (configuration.debug) Serial.println("Failed to parse config file");
//    return false;
//  }
//
//  if (!(json.containsKey("fanStartTemp") &&
//        json.containsKey("fanStopTemp") &&
//        json.containsKey("fanFullSpeedTemp") &&
//        json.containsKey("minFanSpeed") &&
//        json.containsKey("maxFanSpeed") &&
//        json.containsKey("forceFan") &&
//        json.containsKey("forceFanSpeed") &&
//        json.containsKey("debug")))
//  {
//    if (configuration.debug) Serial.println("Config json is missing some keys");
//    return false;
//  }
//
//  configuration.fanStartTemp = json["fanStartTemp"];
//  configuration.fanStopTemp = json["fanStopTemp"];
//  configuration.fanFullSpeedTemp = json["fanFullSpeedTemp"];
//  configuration.minFanSpeed = json["minFanSpeed"];
//  configuration.maxFanSpeed = json["maxFanSpeed"];
//  configuration.forceFan = json["forceFan"].as<bool>();
//  configuration.forceFanSpeed  = json["forceFanSpeed"];
//  configuration.debug  = json["debug"].as<bool>();
//
//  return saveConfig();
//}
