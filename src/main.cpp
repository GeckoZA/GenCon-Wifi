#include <Arduino.h>
#include "config.h"
#include "icons.h"
#include "images.h"
#include <WiFi.h>
#include <FS.h>
#include <SPIFFS.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include "RTClib.h"
#include <SPI.h>
#include <Servo.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Filters.h>

#define ledPin 2

WiFiManager wm;
WiFiManagerParameter *custom_mqtt_server;
WiFiManagerParameter *custom_mqtt_username;
WiFiManagerParameter *custom_mqtt_password;

WiFiManagerParameter *mqtt_port_convert;
WiFiManagerParameter *utc_convert;
WiFiManagerParameter *start_time_convert;
WiFiManagerParameter *stop_time_convert;
WiFiManagerParameter *crank_time_convert;

WiFiManagerParameter *mqtt_checkbox;
WiFiManagerParameter *wifi_time_checkbox;

WiFiClient espClient;
PubSubClient client(espClient);

WiFiUDP ntpUDP;
NTPClient NTPtimeClient(ntpUDP);

RTC_DS3231 rtc;
Servo chokeServo;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

RunningStatistics inputStats;

//--- Setup Functions
bool loadConfigFile()
{
// clean FS, for testing
#ifdef ERASE_WIFI
  SPIFFS.format();
#endif
  // SPIFFS.format();
  //  read configuration from FS json
  debugln("mounting FS...");

  if (SPIFFS.begin(false) || SPIFFS.begin(true))
  {
    debugln("mounted file system");
    if (SPIFFS.exists(JSON_CONFIG_FILE))
    {
      // file exists, reading and loading
      debugln("reading config file");
      File configFile = SPIFFS.open(JSON_CONFIG_FILE, "r");
      if (configFile)
      {
        debugln("opened config file");
        StaticJsonDocument<512> json;
        DeserializationError error = deserializeJson(json, configFile);
        serializeJsonPretty(json, Serial);
        if (!error)
        {
          debugln("\nparsed json");
          // Web Config files
          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_username, json["mqtt_username"]);
          strcpy(mqtt_password, json["mqtt_password"]);
          mqtt_port = json["mqtt_port"].as<int>();
          utc_offset = json["utc_offset"].as<int>();
          start_time = json["start_time"].as<int>();
          stop_time = json["stop_time"].as<int>();
          crank_time = json["crank_time"].as<int>();
          servoMin = json["servo_min"].as<uint8_t>();
          servoMax = json["servo_max"].as<uint8_t>();
          // Screen Config Files
          Wifi_Active = json["wifi_active"].as<bool>();
          MQTT_Active = json["mqtt_active"].as<bool>();
          NTP_time_active = json["NTP_time"].as<bool>();
          presance_detect_Active = json["presance_detect"].as<bool>();
          initial_setup = json["initial_setup"].as<bool>();
          Monitor_Active = json["monitor_active"].as<bool>();

          totalRunTime = json["total_run_time"].as<long>();

          return true;
        }
        else
        {
          debugln("failed to load json config");
        }
      }
    }
  }
  else
  {
    debugln("failed to mount FS");
  }
  // end read
  return false;
}

void saveConfigFile()
{
  //---Get New Values from variables and save to SPIFFS---/
  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 30);
  display.print("Saving Config");
  display.display();

  debugln(F("Saving config"));
  StaticJsonDocument<512> json;
  json["mqtt_server"] = mqtt_server;
  json["mqtt_username"] = mqtt_username;
  json["mqtt_password"] = mqtt_password;
  json["mqtt_port"] = mqtt_port;

  json["utc_offset"] = utc_offset;
  json["start_time"] = start_time;
  json["stop_time"] = stop_time;
  json["crank_time"] = crank_time;
  json["servo_min"] = servoMin;
  json["servo_max"] = servoMax;

  json["mqtt_active"] = MQTT_Active;
  json["NTP_time"] = NTP_time_active;
  json["presance_detect"] = presance_detect_Active;
  json["initial_setup"] = initial_setup;
  json["monitor_active"] = Monitor_Active;

  json["total_run_time"] = totalRunTime;

  File configFile = SPIFFS.open(JSON_CONFIG_FILE, "w");
  if (!configFile)
  {
    debugln("failed to open config file for writing");
  }
  serializeJsonPretty(json, Serial);
  if (serializeJson(json, configFile) == 0)
  {
    debugln(F("Failed to write to file"));
  }
  configFile.close();

  if (webSave == true)
  {
    delay(1000);
    esp_restart();
  }
  delay(1000);
}

void WebSave()
{
  //---Get New Values from WiFiManager and save to variables---/
  strncpy(mqtt_server, custom_mqtt_server->getValue(), sizeof(mqtt_server));
  strncpy(mqtt_username, custom_mqtt_username->getValue(), sizeof(mqtt_username));
  strncpy(mqtt_password, custom_mqtt_password->getValue(), sizeof(mqtt_password));
  // strncpy(mqtt_port, custom_mqtt_port->getValue(), sizeof(mqtt_port));
  mqtt_port = atoi(mqtt_port_convert->getValue());
  utc_offset = atoi(utc_convert->getValue());
  start_time = atoi(start_time_convert->getValue());
  stop_time = atoi(stop_time_convert->getValue());
  crank_time = atoi(crank_time_convert->getValue());

  // Handle the bool value
  MQTT_Active = (strncmp(mqtt_checkbox->getValue(), "t", 1) == 0);
  debug("MQTT Active: ");
  if (MQTT_Active)
  {
    Serial.println("true");
  }
  else
  {
    Serial.println("false");
  }

  NTP_time_active = (strncmp(wifi_time_checkbox->getValue(), "t", 1) == 0);
  debug("wifi_time: ");
  if (NTP_time_active)
  {
    Serial.println("true");
  }
  else
  {
    Serial.println("false");
  }

  debug("MQTT Server IP: ");
  debugln(mqtt_server);
  debug("MQTT Username: ");
  debugln(mqtt_username);
  debug("MQTT Password: ");
  debugln(mqtt_password);
  debug("MQTT Port: ");
  debugln(mqtt_port);

  debug("UTC Offset: ");
  debugln(utc_offset);
  debug("Start Time : ");
  debugln(start_time);
  debug("Stop Time : ");
  debugln(stop_time);
  debug("Generator Crank Time : ");
  debugln(crank_time);
  webSave = true;
  saveConfigFile();
}

void setupWifiManager()
{
  wm.setDebugOutput(true);
  //---Setup for Menu---//
  std::vector<const char *> menu = {"wifi", "setup", "param", "info", "sep", "restart", "exit"};
  wm.setMenu(menu);
  //---Set Dark Mode---//
  wm.setClass("invert");
//---Wipe WiFi settings settings---//
#ifdef ERASE_WIFI
  wm.resetSettings();
#endif
  // wm.resetSettings();
  //---Set config save notify callback---//
  wm.setSaveConfigCallback(WebSave);
  wm.setSaveParamsCallback(WebSave);
  //---Setting Non Blocking & Auto close configportal after 120 seconds---//
  // wm.setConfigPortalTimeout(120);
  wm.setConfigPortalBlocking(false);
  //--- additional Configs params ------//
  // Text box (String)
  custom_mqtt_server = new WiFiManagerParameter("server", "MQTT Server IP", mqtt_server, 40);      // 40 == max length
  custom_mqtt_username = new WiFiManagerParameter("username", "MQTT Username", mqtt_username, 20); // 20 == max length
  custom_mqtt_password = new WiFiManagerParameter("password", "MQTT Password", mqtt_password, 20); // 20 == max length
  // custom_mqtt_port = new WiFiManagerParameter("port", "MQTT Port", mqtt_port, 6);                                         // 6 == max length

  char mqtt_portConvertValue[3];
  sprintf(mqtt_portConvertValue, "%d", mqtt_port);                                             // Need to convert to string to display a default value.
  mqtt_port_convert = new WiFiManagerParameter("port", "MQTT Port", mqtt_portConvertValue, 6); // 6 == max length

  char utcConvertValue[3];
  sprintf(utcConvertValue, "%d", utc_offset);                                                             // Need to convert to string to display a default value.
  utc_convert = new WiFiManagerParameter("utc_offset", "Enter Time Zone UTC Offset", utcConvertValue, 3); // 2 == max length

  char startConverValue[4];
  sprintf(startConverValue, "%d", start_time);                                                                      // Need to convert to string to display a default value.
  start_time_convert = new WiFiManagerParameter("start_time", "Hours for starting Auto Mode", startConverValue, 3); // 3 == max length

  char stopConvertValue[4];
  sprintf(stopConvertValue, "%d", stop_time);                                                                     // Need to convert to string to display a default value.
  stop_time_convert = new WiFiManagerParameter("stop_time", "Hours for stopping Auto Mode", stopConvertValue, 3); // 3 == max length

  char crankConvertValue[3];
  sprintf(crankConvertValue, "%d", crank_time);                                                                         // Need to convert to string to display a default value.
  crank_time_convert = new WiFiManagerParameter("crank_time", "Generator Crank Time in Seconds", crankConvertValue, 3); // 2 == max length

  const char *mqtt_active_Html;
  if (MQTT_Active)
  {
    mqtt_active_Html = "type=\"checkbox\" checked";
  }
  else
  {
    mqtt_active_Html = "type=\"checkbox\"";
  }
  mqtt_checkbox = new WiFiManagerParameter("mqtt_active", "MQTT Server", "t", 2, mqtt_active_Html); // The "t" isn't really important, but if the

  const char *wifi_time_Html;
  if (NTP_time_active)
  {
    wifi_time_Html = "type=\"checkbox\" checked";
  }
  else
  {
    wifi_time_Html = "type=\"checkbox\"";
  }
  wifi_time_checkbox = new WiFiManagerParameter("wifi_time", "Use Internet Time", "t", 2, wifi_time_Html);

  //---Add all your parameters here---//
  wm.addParameter(mqtt_checkbox);
  if (MQTT_Active == true)
  {
    wm.addParameter(custom_mqtt_server);
    wm.addParameter(custom_mqtt_username);
    wm.addParameter(custom_mqtt_password);
    wm.addParameter(mqtt_port_convert);
  }
  wm.addParameter(wifi_time_checkbox);
  if (NTP_time_active == true)
  {
    wm.addParameter(utc_convert);
  }

  wm.addParameter(start_time_convert);
  wm.addParameter(stop_time_convert);
  wm.addParameter(crank_time_convert);
}

void pinSetup()
{
  pinMode(RELAY_ON, OUTPUT);
  pinMode(RELAY_START, OUTPUT);
  pinMode(RELAY_MAINS, OUTPUT);
  pinMode(RELAY_FAN, OUTPUT);

  pinMode(OVERIDE_BTN, INPUT);
  pinMode(CHOKE_BTN, INPUT);
  pinMode(START_BTN, INPUT);
  pinMode(SELECT_BTN, INPUT);
  pinMode(ENTER_BTN, INPUT);
  pinMode(MAINS_BTN, INPUT);
  pinMode(GEN_ON_BTN, INPUT);

  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BLUE_LED, OUTPUT);

  pinMode(AC_12V_SENSE, INPUT);
  pinMode(BATT_SENSE, INPUT);
  pinMode(ZMPT, INPUT);

  pinMode(ledPin, OUTPUT);

  debugln("Pin Modes set");
}

void OLEDSetup()
{
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
  {
    Serial.println(F("SSD1306 allocation failed"));
    // screen is not available ************************************************************************
  }
  display.clearDisplay();
  display.drawBitmap(0, 0, Logo_image, 128, 63, 1);

  display.display();
  delay(500);
  display.clearDisplay();
}

void RTCsetup()
{
  if (!rtc.begin())
  {
    Serial.println("Couldn't find RTC");
    // Serial.flush();
    // while (1)
    // delay(10);
  }

  if (rtc.lostPower())
  {
    Serial.println("RTC lost power, let's set the time!");
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }

  // When time needs to be re-set on a previously configured device, the
  // following line sets the RTC to the date & time this sketch was compiled
  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  // This line sets the RTC with an explicit date & time, for example to set
  // January 21, 2014 at 3am you would call:
  // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
}

// Loop Networking Functions

void mqtt_int_connect()
{
  if (!client.connected())
  {
    debug("Connecting to MQTT broker ...");
    if (client.connect("GenCon", mqtt_username, mqtt_password))
    {
      debugln("MQTT Connection OK");
      debugln("connected");
      // Subscribe
      client.subscribe("GenCon/Cmnd/mode");
      client.subscribe("GenCon/Cmnd/start-stop");
      client.subscribe("GenCon/Cmnd/UTC-Offset");
      client.subscribe("GenCon/Cmnd/mains");
      client.subscribe("GenCon/Cmnd/start-time");
      client.subscribe("GenCon/Cmnd/stop-time");
      client.subscribe("GenCon/Cmnd/presance-detect");
      client.subscribe("GenCon/Cmnd/crank");
      init_mqtt_conn = true;
    }
  }
}

void mqtt_reconnect()
{
  if ((unsigned long)(millis() - mqtt_retry_previousMillis) >= mqtt_retry_Period)
  {
    mqtt_retry_previousMillis = millis();
    if (!client.connected())
    {
      debug("Connecting to MQTT broker ...");
      if (client.connect("GenCon", mqtt_username, mqtt_password))
      {
        debugln("MQTT Connection OK");
        debugln("connected");
        // Subscribe Topics
        client.subscribe("GenCon/Cmnd/mode");
        client.subscribe("GenCon/Cmnd/start-stop");
        client.subscribe("GenCon/Cmnd/UTC-Offset");
        client.subscribe("GenCon/Cmnd/mains");
        client.subscribe("GenCon/Cmnd/start-time");
        client.subscribe("GenCon/Cmnd/stop-time");
        client.subscribe("GenCon/Cmnd/presance-detect");
        client.subscribe("GenCon/Cmnd/crank");
      }
      else
      {
        debug("MQTT Error : ");
        debug(client.state());
        debugln(" Waiting 5 secondes before retrying");
      }
    }
  }
}

void mqttPublish()
{
  if ((unsigned long)(millis() - mqtt_Send_previous) >= mqtt_Send_Period)
  {
    mqtt_Send_previous = millis();
    debugln("Sending MQTT Data");

    client.publish(UTC_offset_topic, String(utc_offset).c_str(), true);
    client.publish(start_time_topic, String(start_time).c_str(), true);
    client.publish(stop_time_topic, String(stop_time).c_str(), true);
    client.publish(presance_detect_topic, String(presance_detect).c_str(), true);
    client.publish(running_state_topic, String(started).c_str(), true);
    client.publish(battery_volt_topic, String(BATT_VOLT).c_str(), true);
    client.publish(temp_topic, String(TEMPERATURE).c_str(), true);
    client.publish(gen_volt_topic, String(GEN_VOLT).c_str(), true);
    client.publish(control_mode_topic, String(MODE).c_str(), true);
    client.publish(crank_time_topic, String(crank_time).c_str(), true);
    client.publish(runtime_topic, String(runTime).c_str(), true);
    client.publish(totalruntime_topic, String(totalRunTime).c_str(), true);
  }
}

void callback(char *topic, byte *message, uint8_t length)
{
  debug("Message arrived on topic: ");
  debug(topic);
  debug(". Message: ");
  String messageMode;
  String messageStartStop;
  String messageMains;
  String messageUTC;
  String messageStartTime;
  String messageStopTime;
  String messageCrankTime;
  String messagepresance;

  for (int i = 0; i < length; i++)
  {
    debug((char)message[i]);
    messageMode += (char)message[i];
  }
  for (int i = 0; i < length; i++)
  {
    debug((char)message[i]);
    messageStartStop += (char)message[i];
  }
  for (int i = 0; i < length; i++)
  {
    debug((char)message[i]);
    messageMains += (char)message[i];
  }
  for (int i = 0; i < length; i++)
  {
    debug((char)message[i]);
    messagepresance += (char)message[i];
  }
  for (int i = 0; i < length; i++)
  {
    debug((char)message[i]);
    messageUTC += (char)message[i];
  }
  for (int i = 0; i < length; i++)
  {
    debug((char)message[i]);
    messageStartTime += (char)message[i];
  }
  for (int i = 0; i < length; i++)
  {
    debug((char)message[i]);
    messageStopTime += (char)message[i];
  }
  for (int i = 0; i < length; i++)
  {
    debug((char)message[i]);
    messageCrankTime += (char)message[i];
  }

  debugln();

  if (String(topic) == "GenCon/Cmnd/mode")
  {
    debug("Changing output to ");
    if (messageMode == "manual")
    {
      debugln("Manual Mode");
      //MODE = 0;
      currentState = generatorState::IDLE_MANUAL;
    }
    if (messageMode == "auto")
    {
      debugln("Auto Mode");
      //MODE = 1;
      currentState = generatorState::IDLE_AUTO;
    }    
    if (messageMode == "monitor")
    {
      debugln("monitor Mode");
      //MODE = 2;
      currentState = generatorState::MONITORING_RUN;
    }
  }

  if (String(topic) == "GenCon/Cmnd/start-stop")
  {
    debug("Changing output to ");
    if (messageStartStop == "on")
    {
      if (started == false)
      {
        debugln("Start on");
        currentState = generatorState::STARTING;
        MODE = 0;
      }
    }
    else if (messageStartStop == "off")
    {
      if (started == true)
      {
        debugln("Shutting Off Generator");
        // ShutDown_Sequance();
        currentState = generatorState::SHUTDOWN;
        MODE = 0;
      }
    }
  }

  if (String(topic) == "GenCon/Cmnd/mains")
  {
    debug("Changing output to ");
    if (messageMains == "on")
    {
      if (started == true)
      {
        debugln("Turning On Mains");
        digitalWrite(RELAY_MAINS, HIGH);
      }
    }
    else if (messageMains == "off")
    {
      if (started == true)
      {
        debugln("Shutting Off Generator");
        digitalWrite(RELAY_MAINS, LOW);
      }
    }
  }

  if (String(topic) == "GenCon/Cmnd/presance-detect")
  {
    debug("Changing output to ");
    if (messagepresance == "on")
    {
      debugln("Presance Detected");
      presance_detect = true;
    }
    else if (messagepresance == "off")
    {
      debugln("No Presance");
      presance_detect = false;
    }
  }

  if (String(topic) == "GenCon/Cmnd/UTC-Offset")
  {
    debug("Changing output to ");
    utc_offset = (messageUTC.toInt());
    saveConfigFile();
  }

  if (String(topic) == "GenCon/Cmnd/start-time")
  {
    debug("Changing output to ");
    start_time = (messageStartTime.toInt());
    saveConfigFile();
  }

  if (String(topic) == "GenCon/Cmnd/stop-time")
  {
    debug("Changing output to ");
    stop_time = (messageStopTime.toInt());
    saveConfigFile();
  }
  if (String(topic) == "GenCon/Cmnd/crank")
  {
    debug("Changing output to ");
    crank_time = (messageCrankTime.toInt());
    saveConfigFile();
  }
}

void RTC_NTP_Sync()
{
  NTPtimeClient.update();
  delay(100);

  int NTP_Day = NTPtimeClient.getDay();
  rtc.adjust(DateTime(RTC_Year, RTC_Month, RTC_Day, NTP_Hrs, NTP_Min, NTP_Sec));
}

void wifi_reconnect()
{
  wm.process();
  //---WiFi Status Values---//
  // idle Status      = 0
  // No SSID Avail    = 1
  // Scan Compledted  = 2
  // Connected        = 3
  // Connect Failed   = 4
  // Connection Lost  = 5
  // Disconnected     = 6
  if (Wifi_Active == 1 && WiFi.status() != 3) // Disconnected from a previously saved Wifi Network
  {
    if ((unsigned long)(millis() - previousWifiReconnect) >= wifiReconnectPeriod)
    {
      previousWifiReconnect = millis();
      debugln("Atempting to reconnect");
      if (wm.getConfigPortalActive() == false)
      {
        wm.startConfigPortal();
      }
      WiFi.reconnect();
    }
  }

  //------------------------------------------------------------------------------------------------------------------
  if (Wifi_Active == 1 && WiFi.status() == 3) // Connected to Previous Saved Wifi Network
  {
    if (wm.getConfigPortalActive() == true)
    {
      wm.stopConfigPortal();
    }
    if (wm.getWebPortalActive() == false)
    {
      wm.startWebPortal();
    }
    //--- NTP Time Update---//
    if (NTP_time_active == true)
    {
      // RTC_NTP_Sync();
      // debugln("Sync NTP Time");
    }

    if (MQTT_Active == true)
    {
      client.loop();
      if (!client.connected())
      {
        if (init_mqtt_conn = true)
        {
          mqtt_reconnect();
        }
        else
        {
          mqtt_int_connect();
        }
      }
      mqttPublish();
    }

    if ((unsigned long)(millis() - debugpreviousMillis) >= debugPeriod)
    {
      debugpreviousMillis = millis();
      debugln("Connected");
      debugln(WiFi.localIP());
    }
  }
}

// Loop Control Functions

void setLED(uint8_t red, uint8_t green, uint8_t blue)
{
  analogWrite(RED_LED, red);
  analogWrite(GREEN_LED, green);
  analogWrite(BLUE_LED, blue);
}

void overideSwitch()
{
  enum class controllerState : uint8_t
  {
    AUTO_MODE,
    MANUAL_MODE,
  };
  static controllerState contCurrentState = controllerState::MANUAL_MODE;

  switch (contCurrentState)
  {
  case controllerState::MANUAL_MODE:
    if (OVERIDE_PRESS == 1)
    {
      delay(button_delay);
      MODE = 1;
      contCurrentState = controllerState::AUTO_MODE;
      currentState = generatorState::IDLE_AUTO;
    }
    break;
  case controllerState::AUTO_MODE:
    if (OVERIDE_PRESS == 1)
    {
      delay(button_delay);
      MODE = 0;
      contCurrentState = controllerState::MANUAL_MODE;
      currentState = generatorState::IDLE_MANUAL;
    }
  default:
    break;
  }
  // int OverideSwitchNew = digitalRead(OVERIDE_BTN);
  /*if (OverideSwitchOld == 0 && OverideSwitchNew == 1)
  {
    if (overideState == 0)
    {
      debugln("Manual Mode Set");
      overideState = 1;
      MODE = 0;
    }
    else
    {
      debugln("Auto Mode Set");
      overideState = 0;
      MODE = 1;
    }
  }
  OverideSwitchOld = OverideSwitchNew;*/
  // delay(20);
}

void Generator_Monitoring()
{
  static float Volts_TRMS = 0; // estimated actual current in amps
  static float slope_intercept = 1.38;

  uint16_t RawValue = analogRead(ZMPT); // read the analog in value:
  inputStats.input(RawValue);           // log to Stats function

  Volts_TRMS = inputStats.sigma() * slope_intercept;
  GEN_VOLT = Volts_TRMS;

  if (Volts_TRMS >= 100 && Volts_TRMS <= 280)
  {
    debug("Generator Volatge: ");
    GEN_VOLT = Volts_TRMS;
    debugln(Volts_TRMS);
  }
  else
  {
    GEN_VOLT = 0;
  }
}

void Runtime_Count(){
  runTime = (runtimeEnd - runtimeStart) / 60000;
  totalRunTime = totalRunTime + runTime;
  saveConfigFile();

  if (totalRunTime >= 600)
  {
    currentState = generatorState::EMERGENCY;
    // fill up generator ***********************
  }  
}

//--- Loop Display Functions
void Display_Home_Screen()
{
  DateTime now = rtc.now();

  display.clearDisplay();
  display.drawLine(0, 15, 128, 15, SSD1306_WHITE); // top Horizontal Line
  display.drawLine(0, 40, 128, 40, SSD1306_WHITE); // bottom Horizontal Line

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  if (WiFi.status() != WL_CONNECTED)
  {
    display.drawBitmap(0, 0, No_Wifi3_icon, 13, 14, 1);
  }
  else
  {
    display.drawBitmap(0, 0, Wifi_icon, 13, 11, 1);
  }
  if (MQTT_Active == 1)
  {
    display.drawBitmap(15, 0, mqtt_icon, 13, 13, 1);
  }
  if (NTP_time_active == 1)
  {
    display.drawBitmap(30, 0, Clock_icon, 13, 13, 1);
  }
  if (presance_detect_Active == 1)
  {
    display.drawBitmap(45, 0, Presance_icon, 13, 13, 1);
  }

  display.setCursor(60, 2);
  display.print(TEMPERATURE);
  display.print("C");

  // display.setCursor(75, 0);
  if (MODE == 0)
  {
    display.setCursor(92, 2);
    display.print("Manual");
  }
  if (MODE == 1)
  {
    display.setCursor(95, 2);
    display.print("Auto");
  }
  if (MODE == 2)
  {
    display.setCursor(85, 2);
    display.print("Monitor");
  }
  if (MODE == 3)
  {
    display.setCursor(85, 2);
    display.print("ERROR");
  }
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(15, 21);
  display.print(now.hour(), DEC);
  display.print(":");
  display.setCursor(52, 21);
  display.print(now.minute(), DEC);
  display.print(":");
  display.setCursor(90, 21);
  display.print(now.second(), DEC);

  if (emergency == false)
  {
    display.drawLine(42, 44, 42, 64, SSD1306_WHITE);
    display.drawLine(85, 44, 85, 64, SSD1306_WHITE);
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(15, 45);
    display.print("AC");
    display.setCursor(50, 45);
    display.print("Gen V");
    display.setCursor(95, 45);
    display.print("Batt");

    display.setCursor(15, 55);
    if (AC_12_VOLT > 6)
    {
      display.print("ON");
    }
    else
    {
      display.print("OFF");
    }
    display.setCursor(57, 55);

    display.print(GEN_VOLT, 0);

    display.setCursor(95, 55);
    display.print(BATT_VOLT, 1);
  }

  if (presance_detect == true)
  {
    display.fillCircle(2, 55, 2, WHITE);
  }

  if (emergency == true)
  {
    display.setTextSize(2);
    display.setCursor(15, 45);
    display.print("EMERGENCY");
  }

  display.display();
}

void Display_Main_Menu()
{
  static uint8_t SUB_OPTION = 0;
  bool SELECT_STATE = SELECT_PRESS;
  bool ENTER_STATE = ENTER_PRESS;
  bool BACK_STATE = MAINS_PRESS;

  if (SELECT_STATE == 1)
  {
    delay(button_delay);
    ++SUB_OPTION;
    if (SUB_OPTION > 2)
    {
      SUB_OPTION = 0;
    }
    debugln(SUB_OPTION);
  }
  if (BACK_STATE == 1)
  {
    delay(button_delay);
    currentState = generatorState::IDLE_MANUAL;
    debugln("back Selected");
  }

  display.clearDisplay();

  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(40, 0);
  display.print("Menu");

  display.setTextSize(1);
  display.setCursor(5, 19);
  display.print("Set Time");
  display.setCursor(5, 31);
  display.print("Generator Config");
  display.setCursor(5, 43);
  display.print("Networking");

  if (SUB_OPTION == 0)
  {
    display.drawRoundRect(0, 17, 120, 12, 2, WHITE);
    if (ENTER_STATE == 1)
    {
      debugln("Set Time Menu");
      MENU = 2;
    }
  }
  if (SUB_OPTION == 1)
  {
    display.drawRoundRect(0, 29, 120, 12, 2, WHITE);
    if (ENTER_STATE == 1)
    {
      debugln("Generator Config");
      MENU = 3;
    }
  }
  if (SUB_OPTION == 2)
  {
    display.drawRoundRect(0, 41, 120, 12, 2, WHITE);
    if (ENTER_STATE == 1)
    {
      debugln("Connection Config");
      MENU = 4;
    }
  }

  display.display();
}

void SET_WIFI_TIME()
{
  NTPtimeClient.update();
  Serial.println(NTPtimeClient.getFormattedTime());
  Serial.println(NTPtimeClient.getDay());

  NTP_Hrs = NTPtimeClient.getHours() + utc_offset;
  if (NTP_Hrs >= 24)
  {
    NTP_Hrs = NTP_Hrs - 24;
  }
  NTP_Min = NTPtimeClient.getMinutes();
  NTP_Sec = NTPtimeClient.getSeconds();

  rtc.adjust(DateTime(RTC_Year, RTC_Month, RTC_Day, NTP_Hrs, NTP_Min, NTP_Sec));

  display.clearDisplay();

  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(28, 2);
  display.print("Saving");
  display.setCursor(10, 24);
  display.print("Internet");
  display.setCursor(34, 46);
  display.print("Time");
  debug("Saving NTP Time");
  display.display();
  delay(1000);
}

void DISPLAY_SET_TIME_MANUALLY()
{
  DateTime now = rtc.now();
  static int SUB_OPTION = 0;

  static uint8_t hrs = (now.hour());
  static uint8_t min = (now.minute());
  debug(hrs);
  debug(":");
  debugln(min);
  debug(now.hour());
  debug(":");
  debugln(now.minute());
  debug("Sub Option: ");
  debugln(SUB_OPTION);

  bool SELECT_STATE = SELECT_PRESS;
  bool ENTER_STATE = ENTER_PRESS;
  bool ADD_STATE = OVERIDE_PRESS;
  bool SUBTRACT_STATE = GEN_ON_PRESS;
  bool BACK_STATE = MAINS_PRESS;

  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(40, 4);
  display.print("Set Time");
  display.drawLine(0, 14, 128, 14, WHITE);

  display.setTextSize(1);
  display.setCursor(50, 50);
  display.print("Save");

  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(15, 22);
  display.print(hrs);
  display.print(":");
  display.setCursor(52, 22);
  display.print(min);
  display.print(":");
  display.setCursor(90, 22);
  display.print(now.second(), DEC);

  if (SELECT_STATE == 1) //--- Select the option
  {
    delay(button_delay);
    ++SUB_OPTION;
    if (SUB_OPTION > 2)
    {
      SUB_OPTION = 0;
    }
  }

  if (BACK_STATE == 1) //--- Back to Main Menu
  {
    delay(button_delay);
    MENU = 1;
    MODE = 4;
    debugln("back Selected");
  }

  if (SUB_OPTION == 0) // Hours
  {
    display.drawRoundRect(10, 19, 40, 21, 2, WHITE);
    if (ADD_STATE == 1)
    {
      delay(button_delay);
      ++hrs;
      if (hrs > 24)
      {
        hrs = 0;
      }
    }
    if (SUBTRACT_STATE == 1)
    {
      delay(button_delay);
      --hrs;
      if (hrs < 0)
      {
        hrs = 24;
      }
    }
  }

  if (SUB_OPTION == 1) // Min
  {
    display.drawRoundRect(47, 20, 40, 20, 2, WHITE);
    if (ADD_STATE == 1)
    {
      delay(button_delay);
      ++min;
      if (min >= 60)
      {
        min = 0;
      }
    }
    if (SUBTRACT_STATE == 1)
    {
      delay(button_delay);
      --min;
      if (min < 0)
      {
        min = 59;
      }
    }
  }
  if (SUB_OPTION == 2) // Save
  {
    display.drawRoundRect(48, 47, 40, 15, 2, WHITE);
    if (ENTER_STATE == 1)
    {
      delay(button_delay);
      rtc.adjust(DateTime(RTC_Year, RTC_Month, RTC_Day, hrs, min, (now.second(), DEC)));
      debugln("Saving Manual time");
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(10, 30);
      display.print("Saving Time...");
      display.display();
      delay(1000);
      MODE = 4;
      MENU = 1;
    }
  }
  display.display();
}

void DISPLAY_SET_TIME_MENU()
{
  display.clearDisplay();
  bool SELECT_STATE = SELECT_PRESS;
  bool ENTER_STATE = ENTER_PRESS;
  bool ADD_STATE = OVERIDE_PRESS;
  bool SUBTRACT_STATE = GEN_ON_PRESS;
  bool BACK_STATE = MAINS_PRESS;

  static uint8_t SUB_OPTION = 0;

  if (NTP_time_active == false) //--- No Wifi Time Only Display Manual screen
  {
    DISPLAY_SET_TIME_MANUALLY();
  }
  else
  {
    if (SELECT_STATE == 1)
    {
      delay(button_delay);
      ++SUB_OPTION;
      if (SUB_OPTION > 2)
      {
        SUB_OPTION = 0;
      }
      debugln(SUB_OPTION);
    }
    if (BACK_STATE == 1)
    {
      delay(button_delay);
      MENU = 1;
      MODE = 4;
      debugln("back Selected");
    }

    display.clearDisplay();
    // Heading
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(15, 1);
    display.print("Time Set Options");

    // Options
    display.setCursor(4, 15);
    display.print("Time Zone: ");
    if (utc_offset > 0)
    {
      display.print("+");
    }
    display.print(utc_offset);
    display.setCursor(4, 27);
    display.print("Auto Internet Time");
    display.setCursor(4, 39);
    display.print("Set Time Manually");

    if (SUB_OPTION == 0)
    {
      display.drawRoundRect(2, 13, 124, 12, 2, WHITE);
      if (ADD_STATE == 1)
      {
        delay(button_delay);
        ++utc_offset;
        if (utc_offset > 12)
        {
          utc_offset = -12;
        }
      }
      if (SUBTRACT_STATE == 1)
      {
        delay(button_delay);
        --utc_offset;
        if (utc_offset < -12)
        {
          utc_offset = 12;
        }
      }
      if (ENTER_STATE == 1)
      {
        delay(button_delay);
        saveConfigFile();
      }
    }

    if (SUB_OPTION == 1)
    {
      display.drawRoundRect(2, 25, 124, 12, 2, WHITE);
      if (ENTER_STATE == 1)
      {
        SET_WIFI_TIME();
      }
    }

    if (SUB_OPTION == 2)
    {
      display.drawRoundRect(2, 37, 124, 12, 2, WHITE);
      if (ENTER_STATE == 1)
      {
        DISPLAY_SET_TIME_MANUALLY();
      }
    }
    display.display();
  }
}

void Display_CHOKE_SET()
{
  static uint8_t submenu_option = 0;
  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(30, 2);
  display.print("Choke Setup");                      //---Heading
  display.drawBitmap(47, 22, Choke_icon, 32, 32, 1); // Icon

  display.setTextSize(2);
  display.setCursor(3, 20);
  display.print("Min");
  display.setCursor(5, 43);
  display.print(servoMin);

  display.setCursor(90, 20);
  display.print("Max");
  display.setCursor(98, 43);
  display.print(servoMax);
  chokeServo.attach(SERVO_PIN);

  if (submenu_option == 0)
  {
    display.drawRoundRect(2, 40, 35, 20, 3, WHITE);
    if (ENTER_PRESS == 1)
    {
      delay(button_delay);
      submenu_option = 1;
    }
    if (OVERIDE_PRESS == 1)
    {
      delay(button_delay);
      servoMin++;
      chokeServo.write(servoMin);
    }
    if (GEN_ON_PRESS == 1)
    {
      delay(button_delay);
      servoMin--;
      chokeServo.write(servoMin);
    }
    if (servoMin > 180)
    {
      servoMin = 180;
    }
    if (servoMin < 0)
    {
      servoMin = 0;
    }
  }
  if (submenu_option == 1)
  {
    display.drawRoundRect(92, 40, 35, 20, 3, WHITE);
    if (ENTER_PRESS == 1)
    {
      delay(button_delay);
      submenu_option = 0;
      MENU = 3;
      chokeServo.detach();
      saveConfigFile();
    }
    if (OVERIDE_PRESS == 1)
    {
      delay(button_delay);
      servoMax++;
      chokeServo.write(servoMax);
    }
    if (GEN_ON_PRESS == 1)
    {
      delay(button_delay);
      servoMax--;
      chokeServo.write(servoMax);
    }
    if (servoMax > 180)
    {
      servoMax = 180;
    }
    if (servoMax < 0)
    {
      servoMax = 0;
    }
  }

  display.display();
}

void DISPLAY_GENERATOR_CONFIG()
{
  static uint8_t select_option = 0;

  bool select_state = SELECT_PRESS;
  bool enter_state = ENTER_PRESS;
  bool add_state = OVERIDE_PRESS;
  bool subtract_state = GEN_ON_PRESS;
  bool BACK_State = MAINS_PRESS;

  if (BACK_State == 1) //--- Going back to main Menu
  {
    delay(button_delay);
    MENU = 1;
    MODE = 4;
    debugln("back Selected");
  }

  if (select_state == 1)
  {
    delay(button_delay);
    select_option++;
    debugln(select_option);
    if (select_option > 4)
    {
      select_option = 0;
    }
  }

  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 2);
  display.print("Configuration");

  display.setCursor(95, 2);
  display.print("Save");

  display.setCursor(2, 16);
  display.print("Start Time:");
  display.setCursor(2, 28);
  display.print("Stop Time:");
  display.setCursor(2, 40);
  display.print("Crank Time:");
  display.setCursor(2, 52);
  display.print("Choke Setup:");

  display.setCursor(85, 16);
  display.print(start_time);
  if (select_option == 0)
  {
    display.drawRoundRect(82, 14, 40, 11, 2, WHITE);
    if (add_state == 1)
    {
      delay(button_delay);
      start_time++;
      if (start_time > 24)
      {
        start_time = 1;
      }
    }
    if (subtract_state == 1)
    {
      delay(button_delay);
      start_time--;
      if (start_time < 0)
      {
        start_time = 23;
      }
    }
  }

  display.print(":00");

  display.setCursor(85, 28);
  display.print(stop_time);
  if (select_option == 1)
  {
    display.drawRoundRect(82, 26, 40, 11, 2, WHITE);
    if (add_state == 1)
    {
      delay(button_delay);
      stop_time++;
      if (stop_time > 24)
      {
        start_time = 1;
      }
    }
    if (subtract_state == 1)
    {
      delay(button_delay);
      stop_time--;
      if (stop_time < 0)
      {
        start_time = 23;
      }
    }
  }
  display.print(":00");

  display.setCursor(85, 40);
  display.print(crank_time);
  if (select_option == 2)
  {
    display.drawRoundRect(82, 38, 40, 11, 2, WHITE);
    if (add_state == 1)
    {
      delay(button_delay);
      crank_time++;
      if (crank_time > 60)
      {
        crank_time = 0;
      }
    }
    if (subtract_state == 1)
    {
      delay(button_delay);
      crank_time--;
      if (crank_time < 0)
      {
        crank_time = 60;
      }
    }
  }
  display.print("sec");

  if (select_option == 4) // Save
  {
    display.drawRoundRect(82, 0, 31, 11, 2, WHITE);
    if (enter_state == 1)
    {
      delay(button_delay);
      saveConfigFile();
      select_option = 0;
      MENU = 1;
    }
  }

  if (select_option == 3)
  {
    display.fillCircle(85, 55, 3, WHITE);
    if (enter_state == 1)
    {
      delay(button_delay);
      select_option = 0;
      MENU = 5;
    }
  }

  display.display();
}

void DISPLAY_NETWORKING()
{
  static uint8_t select_option = 0;
  // delay(button_delay);

  debug("NTP Time: ");
  debugln(NTP_time_active);
  debug("MQTT: ");
  debugln(MQTT_Active);
  debug("Presance: ");
  debugln(presance_detect_Active);

  bool select_state = SELECT_PRESS;
  bool enter_state = ENTER_PRESS;
  bool add_state = OVERIDE_PRESS;
  bool subtract_state = GEN_ON_PRESS;
  bool BACK_State = MAINS_PRESS;

  if (BACK_State == 1) //--- Back to main menu
  {
    delay(button_delay);
    MENU = 1;
    MODE = 4;
    debugln("back Selected");
  }
  if (select_state == 1) //--- Select option
  {
    delay(button_delay);
    select_option++;
    if (select_option > 3)
    {
      select_option = 0;
    }
    debugln(select_option);
  }

  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(100, 40);
  display.print("Save");

  display.setCursor(0, 2);
  display.print(WiFi.localIP());
  display.setCursor(1, 14);
  display.print("RSSI:");
  display.print(WiFi.RSSI());

  display.setCursor(2, 28);
  display.print("WiFi Time:");
  display.drawRect(80, 27, 10, 10, WHITE);
  //--- NTP Select option
  if (select_option == 0)
  {
    display.fillCircle(72, 31, 3, WHITE);
    if (enter_state == 1)
    {
      delay(button_delay);
      NTP_time_active = !NTP_time_active;
    }
  }
  if (NTP_time_active == true)
  {
    display.fillRect(82, 29, 8, 8, WHITE);
  }
  //--- MQTT Select Option
  display.setCursor(2, 40);
  display.print("MQTT Comms:");
  display.drawRect(80, 39, 10, 10, WHITE);
  if (select_option == 1)
  {
    display.fillCircle(72, 43, 3, WHITE);
    if (enter_state == 1)
    {
      delay(button_delay);
      MQTT_Active = !MQTT_Active;
    }
  }
  if (MQTT_Active == true)
  {
    display.fillRect(82, 41, 8, 8, WHITE);
  }
  //--- Presance Select Option
  display.setCursor(2, 52);
  display.print("Presance:");
  display.drawRect(80, 51, 10, 10, WHITE);
  if (select_option == 2)
  {
    display.fillCircle(72, 55, 3, WHITE);
    if (enter_state == 1)
    {
      delay(button_delay);
      presance_detect_Active = !presance_detect_Active;
    }
  }
  if (presance_detect_Active == true)
  {
    display.fillRect(82, 53, 8, 8, WHITE);
  }

  //--- Save Selection ---//
  if (select_option == 3)
  {
    display.drawRoundRect(98, 38, 29, 14, 2, WHITE);
    if (enter_state == 1)
    {
      delay(button_delay);
      saveConfigFile();
      select_option = 0;
      MENU = 1;
    }
  }

  display.display();
}

void Display_refuel(){
  bool select_state = SELECT_PRESS;
  bool enter_state = ENTER_PRESS;
  bool BACK_State = MAINS_PRESS;

  static uint8_t select_option = 0;

  if (BACK_State == 1) //--- Back to main menu
  {
    delay(button_delay);
    currentState = generatorState::IDLE_AUTO;
    debugln("back Selected");
  }
  if (select_state == 1) //--- Select option
  {
    delay(button_delay);
    select_option++;
    if (select_option > 1)
    {
      select_option = 0;
    }
    debugln(select_option);
  }

  display.clearDisplay();

  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Refueled ?");

  display.setCursor(10, 40);
  display.print("100%");

  display.setCursor(80, 40);
  display.print("50%");

  if (select_option == 0) // 100% filled up
  {
    display.drawRoundRect(7, 35, 53, 26, 2, WHITE);
    if (enter_state == 1)
    {
      delay(button_delay);
      totalRunTime = 0;
      saveConfigFile();
    }
    
  }
  
  if (select_option == 1) // 50% Filled up
  {
    display.drawRoundRect(75, 35, 50, 26, 2, WHITE);
    if (enter_state == 1)
    {
      delay(button_delay);
      totalRunTime = 300;
      saveConfigFile();
    }
  }
  

  
  

  display.display();
}

void INITIAL_GEN_SETUP()
{
  while (initial_setup == false)
  {
    /* Initial Setup Code on the screen */
    static uint8_t Select_Option = 0;
    static uint8_t submenu_option = 0;

    bool select_state = SELECT_PRESS;
    bool enter_state = ENTER_PRESS;
    bool add_state = OVERIDE_PRESS;
    bool subtract_state = GEN_ON_PRESS;
    bool BACK_State = MAINS_PRESS;

    display.clearDisplay();

    if (BACK_State == 1)
    {
      initial_setup = true;
    }

    if (select_state == 1) //--- Select option
    {
      delay(button_delay);
      Select_Option++;
      if (Select_Option > 3)
      {
        Select_Option = 0;
      }
      debugln(Select_Option);
    }

    if (Select_Option == 0) // Display Welcome Screen ********ADD a nice image
    {
      display.setTextSize(2);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(20, 2);
      display.print("Welcome");

      display.setTextSize(1);
      display.setCursor(25, 50);
      display.print("Press Entre");

      if (ENTER_PRESS == 1)
      {
        delay(button_delay);
        Select_Option = 1;
      }
    }

    if (Select_Option == 1) //--- Choke Servo Setup
    {
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(30, 2);
      display.print("Choke Setup");                      //---Heading
      display.drawBitmap(47, 22, Choke_icon, 32, 32, 1); // Icon

      display.setTextSize(2);
      display.setCursor(3, 20);
      display.print("Min"); //---Heading
      display.setCursor(5, 43);
      display.print(servoMin); //---Heading

      display.setCursor(90, 20);
      display.print("Max"); //---Heading
      display.setCursor(98, 43);
      display.print(servoMax); //---Heading
      chokeServo.attach(SERVO_PIN);

      if (submenu_option == 0)
      {
        display.drawRoundRect(2, 40, 35, 20, 3, WHITE);
        if (ENTER_PRESS == 1)
        {
          delay(button_delay);
          submenu_option = 1;
        }
        if (OVERIDE_PRESS == 1)
        {
          delay(button_delay);
          servoMin++;
          chokeServo.write(servoMin);
        }
        if (GEN_ON_PRESS == 1)
        {
          delay(button_delay);
          servoMin--;
          chokeServo.write(servoMin);
        }
        if (servoMin > 180)
        {
          servoMin = 180;
        }
        if (servoMin < 0)
        {
          servoMin = 0;
        }
      }

      if (submenu_option == 1)
      {
        display.drawRoundRect(92, 40, 35, 20, 3, WHITE);
        if (ENTER_PRESS == 1)
        {
          delay(button_delay);
          submenu_option = 0;
          Select_Option = 2;
          chokeServo.detach();
        }
        if (OVERIDE_PRESS == 1)
        {
          delay(button_delay);
          servoMax++;
          chokeServo.write(servoMax);
        }
        if (GEN_ON_PRESS == 1)
        {
          delay(button_delay);
          servoMax--;
          chokeServo.write(servoMax);
        }
        if (servoMax > 180)
        {
          servoMax = 180;
        }
        if (servoMax < 0)
        {
          servoMax = 0;
        }
      }
    }

    if (Select_Option == 2) // Engine crank time for starting
    {
      // Engine starting Crank time in seconds
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(15, 2);
      display.print("Engine Crank Time");                     //---Heading
      display.drawBitmap(2, 13, Start_Crank_icon, 50, 50, 1); // Icon
      display.setTextSize(2);
      display.setCursor(80, 25);
      display.print(crank_time);
      display.setTextSize(1);
      display.setCursor(78, 47);
      display.print("Sec");

      if (OVERIDE_PRESS == 1)
      {
        delay(button_delay);
        crank_time++;
      }
      if (GEN_ON_PRESS == 1)
      {
        delay(button_delay);
        crank_time--;
      }

      if (crank_time > 30)
      {
        crank_time = 1;
      }
      if (crank_time <= 0)
      {
        crank_time = 30;
      }

      if (ENTER_PRESS == 1)
      {
        delay(button_delay);
        Select_Option = 3;
      }
    }

    if (Select_Option == 3) // Auto Mode start and Stop Time
    {
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(5, 2);
      display.print("Auto Start/End Time");                      //---Heading
      display.drawBitmap(47, 22, Time_Schedule_icon, 32, 32, 1); // Icon

      display.setCursor(4, 20);
      display.print("Start");
      display.setCursor(90, 20);
      display.print("Stop");

      display.setTextSize(2);
      display.setCursor(4, 34);
      display.print(start_time);
      display.setCursor(90, 34);
      display.print(stop_time);

      if (submenu_option == 0)
      {
        display.drawRoundRect(1, 30, 30, 22, 2, WHITE);
        if (OVERIDE_PRESS == 1)
        {
          delay(button_delay);
          start_time++;
        }
        if (GEN_ON_PRESS == 1)
        {
          delay(button_delay);
          start_time--;
        }
        if (start_time > 24)
        {
          start_time = 0;
        }
        if (start_time < 0)
        {
          start_time = 24;
        }
        if (ENTER_PRESS == 1)
        {
          delay(button_delay);
          submenu_option = 1;
        }
      }

      if (submenu_option == 1)
      {
        display.drawRoundRect(87, 30, 30, 22, 2, WHITE);
        if (OVERIDE_PRESS == 1)
        {
          delay(button_delay);
          stop_time++;
        }
        if (GEN_ON_PRESS == 1)
        {
          delay(button_delay);
          stop_time--;
        }
        if (stop_time > 24)
        {
          stop_time = 0;
        }
        if (stop_time < 0)
        {
          stop_time = 24;
        }
        if (ENTER_PRESS == 1)
        {
          delay(button_delay);
          submenu_option = 0;
          Select_Option = 4;
        }
      }
    }

    if (Select_Option == 4) // GEnerator Voltage Monitoring
    {
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(5, 2);
      display.print("Generator Monitoring");              //---Heading
      display.drawBitmap(7, 20, Monitor_Icon, 40, 41, 1); // Icon

      display.setTextSize(2);
      display.setCursor(50, 35);
      display.print("Yes");
      display.setCursor(98, 35);
      display.print("No");

      if (OVERIDE_PRESS == 1)
      {
        delay(button_delay);
        submenu_option++;
        if (submenu_option > 1)
        {
          submenu_option = 0;
        }
      }

      if (submenu_option == 0)
      {
        display.drawRoundRect(47, 32, 40, 24, 3, WHITE);
        if (ENTER_PRESS == 1)
        {
          delay(button_delay);
          Monitor_Active = true;
          submenu_option = 0;
          Select_Option = 5;
        }
      }
      if (submenu_option == 1)
      {
        display.drawRoundRect(95, 32, 28, 24, 3, WHITE);
        if (ENTER_PRESS == 1)
        {
          delay(button_delay);
          Monitor_Active = false;
          submenu_option = 0;
          Select_Option = 5;
        }
      }
    }

    if (Select_Option == 5) //--- Saving Setup
    {
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(30, 2);
      display.print("Save Setup");                           //---Heading
      display.drawBitmap(2, 13, Setup_Save_icon, 50, 50, 1); // Icon

      display.setTextSize(2);
      display.setCursor(65, 25);
      display.print("Save"); //---Heading
      display.drawRoundRect(60, 21, 55, 24, 3, WHITE);

      if (ENTER_PRESS == 1)
      {
        delay(button_delay);
        initial_setup = true;
        saveConfigFile();
      }
    }

    display.display();
  }
}

void setup()
{
  static uint8_t windowLength = 5; // how long to average the signal, for statistist

  Serial.begin(115200);

  loadConfigFile();         // Load SPIFFS Config file
  OLEDSetup();              // Setup Oled Display
  RTCsetup();               // RTC module Setup
  pinSetup();               // Pin Mode Setup
  analogReadResolution(10); // set DAC to 10bit

  //---Wifi Settings---//
  // WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  delay(10);

  setupWifiManager();

  if (wm.autoConnect(Access_Point, Password))
  {
    Serial.println("connected...yeey :)");
    Wifi_Active = true;
    debug("WiFi Active = ");
    debugln(Wifi_Active);
    wm.startWebPortal();
    WiFi.getAutoReconnect();
  }
  else
  {
    Serial.println("Configportal running");
    if (wm.getWiFiSSID() != "" && wm.getWiFiPass() != "")
    {
      Wifi_Active = true;
    }
  }
  // wm.setWiFiAutoReconnect(true);
  WiFi.setAutoConnect(true);
  WiFi.persistent(true);
  //------------------------------------------------------------------------------------------------//

  client.setServer(mqtt_server, mqtt_port); // Configure MQTT connexion
  client.setCallback(callback);             // MQTT Subscribe

  inputStats.setWindowSecs(windowLength);

  chokeServo.attach(SERVO_PIN); // Servo attach pin to object
  chokeServo.write(servoMin);   // Set Choke to Closed
  delay(1000);
  chokeServo.detach();

  NTPtimeClient.begin(); // NTP Client Start up
  INITIAL_GEN_SETUP();
}

void loop()
{
  wifi_reconnect();
  bool Active_Auto_Time;

  Generator_Monitoring();

  DateTime now = rtc.now();

  BATT_VOLT = (analogRead(BATT_SENSE) * 3.3 / (1023) / (0.2272));
  AC_12_VOLT = (analogRead(AC_12V_SENSE) * 3.3 / (1023)) / (0.2272);
  TEMPERATURE = rtc.getTemperature();

  if ((now.hour()) >= start_time && (now.hour()) <= stop_time) //--- Check time within set limits for auto start routine
  {
    Active_Auto_Time = 1;
  }
  else
  {
    Active_Auto_Time = 0;
  }

  switch (currentState)
  {
  //-----------MANUAL MODE -----------------//
  case generatorState::IDLE_MANUAL:
    Display_Home_Screen();
    overideSwitch();
    setLED(0, 180, 180); //-- Cyan for Manual
    MODE = 0;

    if (ENTER_PRESS == 1 && SELECT_PRESS == 1)
    {
      delay(button_delay);
      currentState = generatorState::MENU;
      debugln("State: MENU");
      // DEBUG_CURRENT_STATE(currentState);
    }
    if (START_PRESS == 1)
    {
      delay(button_delay);
      if (started == false)
      {
        currentState = generatorState::STARTING;
        MODE = 0;
        debugln("State: STARTING");
      }
      if (started == true)
      {
        currentState = generatorState::SHUTDOWN;
        debugln("State: SHUTDOWN");
        // ShutDown_Sequance();
      }
    }
    if (MAINS_PRESS == 1)
    {
      delay(button_delay);
      MAINS_TOGGLE_RELAY;
    }
    if (GEN_ON_PRESS == 1)
    {
      delay(button_delay);
      GEN_TOGGLE_RELAY;
    }

    break;

    //-----------AUTO MODE -----------//
  case generatorState::IDLE_AUTO:
    Display_Home_Screen();
    overideSwitch();
    setLED(0, 180, 0); //-- Green for Auto
    MODE = 1;

    if (ENTER_PRESS == 1 && SELECT_PRESS == 1)
    {
      delay(button_delay);
      currentState = generatorState::MENU_REFUEL;
      debugln("State: REFUEL MENU");
      // DEBUG_CURRENT_STATE(currentState);
    }

    if (GEN_ON_PRESS == 1)
    {
      delay(button_delay);
      presance_detect = !presance_detect;
    }

    //--- Not using presance detection
    if (presance_detect_Active == false)
    {
      if (Active_Auto_Time == 1) // -- time for Auto start is valid
      {
        // *********** Set LED Auto Mode Active
        if (millis() - debugpreviousMillis > debugPeriod)
        {
          debugpreviousMillis = millis();
          debugln("Hours are good for AUTO MODE No Presance Detection");
          debugln(now.hour());
          debugln(start_time);
          debug("AC 12v Status = ");
          debugln(AC_12_VOLT);
        }

        if (AC_12_VOLT < 6) //--- Municipal Power is out - Run Start Sequance
        {
          if (started == false)
          {
            currentState = generatorState::START_WAIT;
            startWaitMillis = millis();
          }          
        }
        if (AC_12_VOLT > 6)
        {
          debugln("Municipal Power is On");
        }
      }
      else
      {
        debugln("After Hours in AUTO MODE");
        // *********** Set LED Auto Mode Inactive
      }
    }

    // --- Using Presance detection
    if (presance_detect_Active == true)
    {
      debug("Presance Active: ");
      debugln(presance_detect_Active);
      debug("presanse detect:");
      debugln(presance_detect);

      if (Active_Auto_Time == true && presance_detect == true) // -- time for Auto start is valid and presence detected is true
      {
        // *********** Set LED Auto Mode Active
        debugln("Hours are good for AUTO MODE With Presance Detection ************");
        debugln(now.hour());
        debugln(start_time);
        debug("AC 12v Status = ");
        debugln(AC_12_VOLT);

        if (AC_12_VOLT < 6) //--- Municipal Power is out - Run Start Sequance
        {
          if (started == false)
          {
            currentState = generatorState::START_WAIT;
            startWaitMillis = millis();
          }         
        }
        else
        {
          debugln("Municipal Power is On");
        }
      }
      else if (Active_Auto_Time == true && presance_detect == false)
      {
        debugln("No One Home, Not starting Generator");
      }
      else if (Active_Auto_Time == false)
      {
        debugln("After Hours in AUTO MODE");
        // *********** Set LED Auto Mode Inactive
      }
    }
    break;

  case generatorState::MENU:
    setLED(0, 0, 180); //-- Blue for Menu
    if (MENU == 1)
    {
      Display_Main_Menu();
    }
    if (MENU == 2)
    {
      DISPLAY_SET_TIME_MENU();
    }
    if (MENU == 3)
    {
      DISPLAY_GENERATOR_CONFIG();
    }
    if (MENU == 4)
    {
      DISPLAY_NETWORKING();
    }
    if (MENU == 5)
    {
      Display_CHOKE_SET();
    }
    break;

  case generatorState::MENU_REFUEL:
    setLED(0, 0, 180); //-- Blue for Menu
    Display_refuel();
    break;

  case generatorState::START_WAIT:
    Display_Home_Screen();
    setLED(180, 150, 0); //-- Yellow for Starting
    if (AC_12_VOLT < 6)
    {
      if (millis() - startWaitMillis > (startWaitPeriod * 1000))
      {
        currentState = generatorState::STARTING;
        debugln("State: STARTING - Starting");
      }
    }
    else
    {
      debugln("Power came back on");
      currentState = generatorState::IDLE_AUTO;
    }
    break;

  case generatorState::STARTING:

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(10, 2);
    display.print("Staring Generator");
    display.display();
    digitalWrite(RELAY_ON, HIGH); // Relay On to turn on Generator Power
    display.setCursor(1, 13);
    display.print("Setting Choke");
    debugln("Opening Choke");
    chokeServo.attach(SERVO_PIN);
    chokeServo.write(servoMax); // Set Choke to Open
    currentState = generatorState::CHOKE_ON;
    startMillis = millis();

    break;

  case generatorState::CHOKE_ON:

    if (millis() - startMillis > 1000)
    {
      display.setCursor(1, 25);
      display.print("Cranking");
      display.display();
      debugln("Starting Generator");
      digitalWrite(RELAY_START, HIGH); // Simulate key to start
      startMillis = millis();
      currentState = generatorState::CRANKING;
    }
    break;

  case generatorState::CRANKING:

    if (millis() - startMillis > (crank_time * 1000))
    {
      debugln("Simulating Releasing of the key");
      digitalWrite(RELAY_START, LOW); // Simutales stopping the start and cranking of the key
      startMillis = millis();
      currentState = generatorState::RUNNING;
    }
    break;

  case generatorState::RUNNING:

    if (TEMPERATURE < 15)
    {
      if (millis() - startMillis > 8000)
      {
        debugln("Cold Day Closeing Choke");
        display.setCursor(1, 38);
        display.println("Closing Choke");
        display.display();
        chokeServo.attach(SERVO_PIN);
        chokeServo.write(servoMin); // Set Choke to Closed
        startMillis = millis();
        currentState = generatorState::STARTED;
      }
    }
    else
    {
      if (millis() - startMillis > 2000)
      {
        debugln("Warm Day Closeing Choke");
        display.setCursor(1, 38);
        display.println("Closing Choke");
        display.display();
        chokeServo.attach(SERVO_PIN);
        chokeServo.write(servoMin); // Set Choke to Closed
        startMillis = millis();
        currentState = generatorState::STARTED;
      }
    }
    break;

  case generatorState::STARTED:
    if (millis() - startMillis > 500)
    {
      display.setCursor(1, 50);
      display.println("Start Complete");
      display.display();
      chokeServo.detach();
      started = true;
      runtimeStart= 0;
      runtimeEnd = 0;
      runtimeStart = millis();
      debugln("Start Sequance Complete");
      if (MODE == 1) //---Was in Auto Mode go to Monitoring
      {
        currentState = generatorState::MONITORING_INITIALIZE;
        monitorWaitMillis = millis();
        debugln("State: MONITORING_INITIALIZE");
      }
      if (MODE == 0) //--- Was in Manual Mode
      {
        currentState = generatorState::IDLE_MANUAL;
      }
    }
    break;

  case generatorState::MONITORING_INITIALIZE:
    Display_Home_Screen();
    setLED(180, 0, 180); //-- Magenta for Monitoring
    if (millis() - monitorWaitMillis > monitorWaitPeriod)
    {
      digitalWrite(RELAY_MAINS, HIGH);
      currentState = generatorState::MONITORING_RUN;
      debugln("State: MONITORING_RUN");
    }
    break;

  case generatorState::MONITORING_RUN:
    Display_Home_Screen();
    setLED(180, 0, 180); //-- Magenta for Monitoring
    MODE = 2;

    if (START_PRESS == 1)
    {
      currentState = generatorState::SHUTDOWN;
      MODE = 0;
    }

    if (Active_Auto_Time == false)
    {
      currentState = generatorState::SHUTDOWN;
    }

    if (AC_12_VOLT > 6)
    {
      currentState = generatorState::SHUTDOWN_WAIT; // changed for testing from shutdownwait
      shutdownWaitMillis = millis();
    }

    //if (GEN_VOLT < 50)
    //{
    //  currentState = generatorState::EMERGENCY;
    //}

    break;

  case generatorState::SHUTDOWN_WAIT:
    Display_Home_Screen();
    setLED(180, 100, 0); //-- Yellow for Starting

    if (AC_12_VOLT > 6)
    {
      if (millis() - shutdownWaitMillis > shutdownWaitPeriod)
      {
        currentState = generatorState::SHUTDOWN;
        debugln("State: SHUTDOWN");
      }
    }
    else
    {
      currentState = generatorState::MONITORING_RUN;
    }
    break;

  case generatorState::SHUTDOWN:
    setLED(180, 180, 0); //-- Yellow for Starting
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(10, 20);
    // Display static text
    display.println("Shutting Down");
    display.display();

    digitalWrite(RELAY_MAINS, LOW);
    delay(500);
    digitalWrite(RELAY_ON, LOW);
    runtimeEnd = millis();
    Runtime_Count();
    
    if (MODE == 0)
    {
      currentState = generatorState::IDLE_MANUAL;
    }
    if (MODE == 1 || MODE == 2)
    {
      currentState = generatorState::IDLE_AUTO;
    }
    started = false;
    break;

  case generatorState::EMERGENCY:
    Display_Home_Screen();
    setLED(180, 0, 0); //-- Red for Emergency
    debugln("State: EMERGENCY SHUTDOWN");
    digitalWrite(RELAY_MAINS, LOW);
    delay(500);
    digitalWrite(RELAY_ON, LOW);
    MODE = 3;
    emergency = true;

    if (OVERIDE_PRESS == 1 && MAINS_PRESS == 1)
    {
      delay(button_delay);
      currentState = generatorState::IDLE_MANUAL;
      emergency = false;
      MODE = 0;
      debugln("State: IDLE_MANUAL");
    }
    break;

  default:
    break;
  }
}
