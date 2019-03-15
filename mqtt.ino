void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    if (configuration.debug) Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(mqtt_clientName, mqtt_username, mqtt_password, logTopic, 1, true, "Living CV disconnected from MQTT server")) {
      if (configuration.debug) Serial.println("connected");
      sendMQTTMessage(logTopic, "Living CV fan connected to MQTT server", false);
      client.setCallback(callback);
      subscribe();

    } else {
      if (configuration.debug) Serial.print("failed, rc=");
      if (configuration.debug) Serial.print(client.state());
      if (configuration.debug) Serial.println(" try again in 5 seconds");
      // Wait 1 seconds before retrying
      delay(3000);
      ESP.wdtFeed();
    }
  }
}

void sendSensor(Sensor &sensor)
{
  if (sensor.oldValue != sensor.value)
  {
    char buf[10];
    sendMQTTMessage(sensor.topic, ftoa(buf, sensor.value, 2), true);
    sensor.oldValue = sensor.value;
    delay2(100);
  }
}

void sendMQTTMessage(char* topic, char* message, bool persistent) {

  if (client.connected()) {
    if (configuration.debug) Serial.print(topic);
    if (configuration.debug) Serial.print(" => ");
    if (configuration.debug) Serial.println(message);
    client.publish(topic, message, persistent);
  } else {
    reconnect();
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  char payloadcpy[length + 1];
  for (int i = 0; i < length; i++) {
    payloadcpy[i] = payload[i];
  }
  payloadcpy[length + 1] = '\0';

  if (configuration.debug) Serial.print("Message arrived [");
  if (configuration.debug) Serial.print(topic);
  if (configuration.debug) Serial.print("] :");
  if (configuration.debug) Serial.println(payloadcpy);

  Configuration tempConfig = configuration;

  if (strcmp(topic, configfanstarttemp) == 0) tempConfig.fanStartTemp = atoi((char*)payloadcpy);
  if (strcmp(topic, configfanstoptemp) == 0) tempConfig.fanStopTemp = atoi((char*)payloadcpy);
  if (strcmp(topic, configfanfullspeedtemp) == 0) tempConfig.fanFullSpeedTemp = atoi((char*)payloadcpy);
  if (strcmp(topic, configminfanspeed) == 0) tempConfig.minFanSpeed = atoi((char*)payloadcpy);
  if (strcmp(topic, configmaxfanspeed) == 0) tempConfig.maxFanSpeed = atoi((char*)payloadcpy);
  if (strcmp(topic, configforcefanspeed) == 0) tempConfig.forceFanSpeed = atoi((char*)payloadcpy);
  if (strcmp(topic, configpwmfrequency) == 0) tempConfig.pwmFrequency = atoi((char*)payloadcpy);

  if (strcmp(topic, configforcefan) == 0)
  {
    if (atoi((char*)payloadcpy) == 0) tempConfig.forceFan = false;
    if (atoi((char*)payloadcpy) == 1) tempConfig.forceFan = true;
  }

  if (strcmp(topic, configdebug) == 0)
  {
    if (atoi((char*)payloadcpy) == 0) tempConfig.debug = false;
    if (atoi((char*)payloadcpy) == 1) tempConfig.debug = true;
  }

  if (isValidConfig(tempConfig) && !areConfigsEqual(configuration, tempConfig))
  {
    configuration = tempConfig;
    saveConfig();
    applyConfig();
    sendConfiguration();
  }

}


void subscribe()
{
  client.subscribe(configfanstarttemp);
  client.subscribe(configfanstoptemp);
  client.subscribe(configfanfullspeedtemp);
  client.subscribe(configminfanspeed);
  client.subscribe(configmaxfanspeed);
  client.subscribe(configforcefan);
  client.subscribe(configforcefanspeed);
  client.subscribe(configdebug);
  client.subscribe(configpwmfrequency);
}
