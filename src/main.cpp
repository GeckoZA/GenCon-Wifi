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

bool loadConfigFile()
{
  // clean FS, for testing
  if (reset_data == true)
  {
    SPIFFS.format();
  }
  // read configuration from FS json
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
          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_username, json["mqtt_username"]);
          strcpy(mqtt_password, json["mqtt_password"]);

          mqtt_port = json["mqtt_port"].as<int>();
          utc_offset = json["utc_offset"].as<int>();
          start_time = json["start_time"].as<int>();
          stop_time = json["stop_time"].as<int>();
          crank_time = json["crank_time"].as<int>();

          MQTT_Active = json["mqtt_active"].as<bool>();
          wifi_time = json["wifi_time"].as<bool>();
          presance_detect = json["presance_detect"].as<bool>();

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

  json["mqtt_active"] = MQTT_Active;
  json["wifi_time"] = wifi_time;
  json["presance_detecte"] = presance_detect;
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

  wifi_time = (strncmp(wifi_time_checkbox->getValue(), "t", 1) == 0);
  debug("wifi_time: ");
  if (wifi_time)
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

void mqttConnect(){
  if (!client.connected())
  {
    debug("Connecting to MQTT broker ...");
    if (client.connect("GenCon", mqtt_username, mqtt_password))
    {
      debugln("MQTT Connection OK");
      debugln("connected");
      // Subscribe
      client.subscribe("GenCon/Cmnd/mode");
      client.subscribe("GenCon/Cmnd/start");
      client.subscribe("GenCon/Cmnd/UTC-Offset");
      client.subscribe("GenCon/Cmnd/mains");
      client.subscribe("GenCon/Cmnd/start-time");
      client.subscribe("GenCon/Cmnd/stop-time");
      client.subscribe("GenCon/Cmnd/presance-detect");
      init_mqtt_conn = true;
    }
  }
}
void reconnect()
{
  if ((unsigned long)(millis() - mqtt_retry_previousMillis) >= mqtt_retry_Period)
  {
    mqtt_retry_previousMillis = millis();
  //int reconnect_tries = 0;
  if (!client.connected())
  {
    debug("Connecting to MQTT broker ...");
    if (client.connect("GenCon", mqtt_username, mqtt_password))
      {
      debugln("MQTT Connection OK");
      debugln("connected");
      // Subscribe
      client.subscribe("GenCon/Cmnd/mode");
      client.subscribe("GenCon/Cmnd/start");
      client.subscribe("GenCon/Cmnd/UTC-Offset");
      client.subscribe("GenCon/Cmnd/mains");
      client.subscribe("GenCon/Cmnd/start-time");
      client.subscribe("GenCon/Cmnd/stop-time");
      client.subscribe("GenCon/Cmnd/presance-detect");
      }
    else
      {
      debug("MQTT Error : ");
      debug(client.state());
      debugln(" Waiting 5 secondes before retrying");

      //reconnect_tries++;
      //if (reconnect_tries >= 10)
      //{
        //reconnect_tries = 0;
        //delay(1000);
        //esp_restart();
      //}
      //delay(1000);
      }
    }
  }
}

void mqttPublish()
{
  if ((unsigned long)(millis() - startpreviousMillis) >= startPeriod)
  {
    startpreviousMillis = millis();
    debugln("Sending MQTT Data");

    client.publish(UTC_offset_topic, String(utc_offset).c_str(), true);
    client.publish(start_time_topic, String(start_time).c_str(), true);
    client.publish(stop_time_topic, String(stop_time).c_str(), true);
    client.publish(presance_detect_topic, String(presance_detect).c_str(), true);
    client.publish(power_state_topic, String(AC_input).c_str(), true);
    client.publish(battery_volt_topic, String(BATT_VOLT).c_str(), true);
    client.publish(temp_topic, String(TEMPERATURE).c_str(), true);
    client.publish(gen_volt_topic, String(GEN_VOLT).c_str(), true);
    client.publish(control_mode_topic, String(MODE).c_str(), true);
  }
}

void OLEDSetup()
{
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
  {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ; // Don't proceed, loop forever
  }
  // delay(2000); // Pause for 2 seconds
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(10, 20);
  // Display static text
  // display.println("Initializing...");

  display.drawBitmap(0, 0, Logo_image, 128, 63, 1);

  display.display();
  delay(500);
  display.clearDisplay();
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

void RTCsetup()
{
  if (!rtc.begin())
  {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    while (1)
      delay(10);
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

void RTC_to_NTP_time()
{
  // timeClient.begin();
  NTPtimeClient.update();
  delay(100);

  int NTP_Day = NTPtimeClient.getDay();
  rtc.adjust(DateTime(RTC_Year, RTC_Month, RTC_Day, NTP_Hrs, NTP_Min, NTP_Sec));
}

void setLED(int red, int green, int blue)
{
  analogWrite(RED_LED, red);
  analogWrite(GREEN_LED, green);
  analogWrite(BLUE_LED, blue);
}

void overideSwitch()
{
  // changing momentary button to switch
  int OverideSwitchNew = digitalRead(OVERIDE_BTN);

  if (OverideSwitchOld == 0 && OverideSwitchNew == 1)
  {
    if (overideState == 0)
    {
      debugln("Manual Mode Set");
      setLED(180, 0, 0);
      overideState = 1;
      MODE = 1;
    }
    else
    {
      debugln("Auto Mode Set");
      setLED(0, 180, 0);
      overideState = 0;
      MODE = 0;
    }
  }
  OverideSwitchOld = OverideSwitchNew;
  delay(20);
}

void Auto_Start_Sequance()
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 40);
  // Display static text
  display.println("Staring Generator");
  // display.drawBitmap(0, 0, epd_bitmap_This_is_the_way, 128, 64, 1);
  display.display();

  setLED(255, 255, 0); // LED Yellow For Starting
  debugln("Turning On Generator.");
  digitalWrite(RELAY_ON, HIGH); // Relay On tu turn on Generator Power
  debugln("Opening Choke");
  chokeServo.write(180); // Set Choke to Open
  delay(1000);
  debugln("Starting Generator");
  digitalWrite(RELAY_START, HIGH); // Simulate key to start
  delay(crank_time * 1000);
  debugln("Simulating Releasing of the key");
  digitalWrite(RELAY_START, LOW); // Simutales stopping the start and cranking of the key
  if (TEMPERATURE < 15)
  {
    delay(10 * 1000); // Cold Temperature operation
  }
  else if (TEMPERATURE > 15)
  {
    delay(5 * 1000); // Warm Temperature Operation
  }
  debugln("Closeing Choke");
  chokeServo.write(0); // Set Choke to Closed
  started = true;
  debugln("Start Sequance Complete");
}

void Generator_Monitoring()
{
  int RawValue = analogRead(ZMPT); // read the analog in value:
  inputStats.input(RawValue);      // log to Stats function

  Volts_TRMS = inputStats.sigma() * slope + intercept;
  GEN_VOLT = Volts_TRMS;

  if (Volts_TRMS >= 190 && Volts_TRMS <= 235)
  {
    debug("Generator Volatge: ");
    GEN_VOLT = Volts_TRMS;
    debugln(Volts_TRMS);
  }
  if (Volts_TRMS < 189)
  {
    GEN_VOLT = 0;
  }
}

void ShutDown_Sequance()
{
  if (started == true)
  {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(10, 20);
    // Display static text
    display.println("Shutting Down");
    display.display();
    if (emergency == true)
    {
      digitalWrite(RELAY_MAINS, LOW);
      digitalWrite(RELAY_ON, LOW);
      MODE = 1;
    }
    
    digitalWrite(RELAY_MAINS, LOW);
    delay(1000);
    digitalWrite(RELAY_ON, LOW);

    runTime = millis() - runTime;
    totalRunTime = totalRunTime + runTime;
      //saving run time
      debugln(F("Saving config"));
      StaticJsonDocument<512> json;
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

    shutdown = true;
    started = false;
  }
}

void Display_Home_Screen()
{
  DateTime now = rtc.now();

  display.clearDisplay();
  display.drawLine(0, 15, 128, 15, SSD1306_WHITE); // top Horizontal Line
  display.drawLine(0, 40, 128, 40, SSD1306_WHITE); // bottom Horizontal Line
  display.drawLine(42, 44, 42, 64, SSD1306_WHITE);
  display.drawLine(85, 44, 85, 64, SSD1306_WHITE);

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
  if (MQTT_Active == true)
  {
     display.drawBitmap(15, 0, mqtt_icon, 13, 13, 1);
  }
  if (wifi_time == true)
  {
    display.drawBitmap(30, 0, Clock_icon, 13, 13, 1);
  }
  if (use_presance_detect == true)
  {
    display.drawBitmap(45, 0, Presance_icon, 13, 13, 1);
  }
  

  display.setCursor(65, 2);
  display.print(TEMPERATURE);
  display.print("C");

  //display.setCursor(75, 0);
  if (MODE == 0)
  {
    display.setCursor(95, 2);
    display.print("Auto");
  }
  if (MODE == 1)
  {
    display.setCursor(92, 2);
    display.print("Manual");
  }
  if (MODE == 2)
  {
    display.setCursor(90, 2);
    display.print("Monitor");
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

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(15, 45);
  display.print("AC");
  display.setCursor(50, 45);
  display.print("Gen V");
  display.setCursor(95, 45);
  display.print("Batt");

  display.setCursor(15, 55);
  if (AC_12_VOLT > 10)
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

void SET_TIME_MANUALLY()
{
  NTPtimeClient.update();
  DateTime now = rtc.now();
  int hrs = (now.hour(), DEC);
  int min = (now.minute(), DEC);
  debug(hrs);
  debug(":");
  debugln(min);

  int TimeSelect = 0;
  int increaseSelected = 0;
  int entercmd = digitalRead(SELECT_BTN);
  


  while (MENU == 3)
  {
    //DateTime now = rtc.now();

    display.clearDisplay();

    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(15, 4);
    display.print("Set Time");

    display.setTextSize(1);
    display.setCursor(55, 50);
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

    int SELECT_STATE = digitalRead(ENTER_BTN);
    int entercmd = digitalRead(SELECT_BTN);
    int add_state = digitalRead(OVERIDE_BTN);
    int subtract_state = digitalRead(GEN_ON_BTN);
    int BACK_State = digitalRead(MAINS_BTN);

    if (entercmd == 1)
    {
      delay(button_delay);
      ++TimeSelect;
      Serial.println("Time Select");
      if (TimeSelect > 2)
      {
        TimeSelect = 0;
      }
    }
    if (TimeSelect == 0) // Hours
    {
      display.drawRoundRect(10, 20, 40, 20, 2, WHITE);
      if (add_state == 1)
      {
        delay(button_delay);
        ++hrs;
        if (hrs >24)
        {
          hrs = 0;
        }
      }
      if (subtract_state == 1)
      {
        delay(button_delay);
        --hrs;
        if (hrs < 0)
        {
          hrs = 24;
        }
      }      
    }
    if (TimeSelect == 1) // Min
    {
      display.drawRoundRect(47, 20, 40, 20, 2, WHITE);
      if (add_state == 1)
      {
        delay(button_delay);
        ++min;
        if (min >= 60)
        {
          min = 0;
        }
      }
      if (subtract_state == 1)
      {
        delay(button_delay);
        --min;
        if (min < 0)
        {
          min = 59;
        }
      }
    }
    if (TimeSelect == 2) // Save
    {
      display.drawRoundRect(48, 47, 40, 15, 2, WHITE);
      if (SELECT_STATE == 1)
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
        MODE = 1;
        MENU = 0;
      }
    }
if (BACK_State == 1)
    {
      delay(button_delay);
      MENU = 0;
      MODE = 1;
      debugln("back Selected");
    }

    display.display();
  }
}

void SET_GENERATOR_FUNCTIONS()
{
  int select_option = 0;

  while (MENU == 4)
  {
    int select_state = digitalRead(SELECT_BTN);
    int enter_state = digitalRead(ENTER_BTN);
    int add_state = digitalRead(OVERIDE_BTN);
    int subtract_state = digitalRead(GEN_ON_BTN);
    int BACK_State = digitalRead(MAINS_BTN);

    if (BACK_State == 1)
    {
      delay(button_delay);
      MENU = 0;
      MODE = 1;
      debugln("back Selected");
    }

    display.clearDisplay();

    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 2);
    display.print("Configuration");

    display.setCursor(95, 2);
    display.print("Save");

    display.setCursor(2, 18);
    display.print("UTC Addition:");
    display.setCursor(2, 30);
    display.print("Start Time:");
    display.setCursor(2, 42);
    display.print("Stop Time:");
    display.setCursor(2, 54);
    display.print("Crank Time:");

    display.setCursor(85, 18);
    if (utc_offset > 0)
    {
      display.print("+");
    }

    display.setCursor(90, 18);
    display.print(utc_offset);
    if (select_option == 0)
    {
      display.drawRoundRect(82, 16, 40, 11, 2, WHITE);
      if (add_state == 1)
      {
        delay(button_delay);
        utc_offset++;
        if (utc_offset >= 13)
        {
          utc_offset = -12;
        }
      }
      if (subtract_state == 1)
      {
        delay(button_delay);
        utc_offset--;
        if (utc_offset <= -13)
        {
          utc_offset = 12;
        }
      }
    }

    display.setCursor(85, 30);
    display.print(start_time);
    if (select_option == 1)
    {
      display.drawRoundRect(82, 28, 40, 11, 2, WHITE);
      if (add_state == 1)
      {
        delay(button_delay);
        start_time++;
        if (start_time >= 24)
        {
          start_time = 0;
        }
      }
      if (subtract_state == 1)
      {
        delay(button_delay);
        start_time--;
        if (start_time <= 0)
        {
          start_time = 24;
        }
      }
    }

    display.print(":00");

    display.setCursor(85, 42);
    display.print(stop_time);
    if (select_option == 2)
    {
      display.drawRoundRect(82, 40, 40, 11, 2, WHITE);
      if (add_state == 1)
      {
        delay(button_delay);
        stop_time++;
        if (stop_time >= 24)
        {
          start_time = 0;
        }
      }
      if (subtract_state == 1)
      {
        delay(button_delay);
        stop_time--;
        if (stop_time <= 0)
        {
          start_time = 24;
        }
      }
    }
    display.print(":00");

    display.setCursor(85, 54);
    display.print(crank_time);
    if (select_option == 3)
    {
      display.drawRoundRect(82, 52, 40, 11, 2, WHITE);
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
      display.drawRoundRect(88, 0, 30, 11, 2, WHITE);
      if (enter_state == 1)
      {
        delay(button_delay);
        saveConfigFile();
        MENU = 1;
      }
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
    display.display();
  }
}

void CONNECTION_CONFIG_DISPLAY()
{
  int select_option = 0;
  delay(button_delay);

  while (MENU == 5)
  {
    int select_state = digitalRead(SELECT_BTN);
    int enter_state = digitalRead(ENTER_BTN);
    int add_state = digitalRead(OVERIDE_BTN);
    int subtract_state = digitalRead(GEN_ON_BTN);
    int BACK_State = digitalRead(MAINS_BTN);

    if (BACK_State == 1)
    {
      delay(button_delay);
      MENU = 0;
      MODE = 1;
      debugln("back Selected");
    }

    display.clearDisplay();

    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    // display.setCursor(0,2);
    // display.print("Networking");
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
    if (select_option == 0)
    {

      display.fillCircle(72, 31, 3, WHITE);
      if (enter_state == 1)
      {
        delay(button_delay);
        wifi_time = !wifi_time;
      }
    }
    if (wifi_time == true)
    {
      display.fillRect(82, 29, 8, 8, WHITE);
    }

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

    display.setCursor(2, 52);
    display.print("Presance:");
    display.drawRect(80, 51, 10, 10, WHITE);
    if (select_option == 2)
    {
      display.fillCircle(72, 55, 3, WHITE);
      if (enter_state == 1)
      {
        delay(button_delay);
        use_presance_detect = !use_presance_detect;
      }
    }
    if (use_presance_detect == true)
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
        MENU = 1;
      }
    }

    if (select_state == 1)
    {
      delay(button_delay);
      select_option++;
      if (select_option > 3)
      {
        select_option = 0;
      }
      debugln(select_option);
    }
    display.display();
  }
}

void Display_Menu()
{

  int SELECT_OPTION = digitalRead(SELECT_BTN);
  int ENTER_STATE = digitalRead(ENTER_BTN);
  int BACK_State = digitalRead(MAINS_BTN);

  if (SELECT_OPTION == 1)
  {
    delay(button_delay);
    ++MAIN_OPTION;
    if (MAIN_OPTION > 3)
    {
      MAIN_OPTION = 0;
    }
    debugln(MAIN_OPTION);
  }
  if (BACK_State == 1)
  {
    delay(button_delay);
    MENU = 0;
    MODE = 1;
    debugln("back Selected");
  }

  display.clearDisplay();

  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(40, 0);
  display.print("Menu");

  display.setTextSize(1);
  display.setCursor(5, 19);
  display.print("Set WIFI Time");
  display.setCursor(5, 31);
  display.print("Manual Time");
  display.setCursor(5, 43);
  display.print("Generator Config");
  display.setCursor(5, 55);
  display.print("Connections");

  if (MAIN_OPTION == 0)
  {
    display.drawRoundRect(0, 17, 120, 12, 2, WHITE);
    if (ENTER_STATE == 1)
    {
      debugln("Set Wifi Time");
      SET_WIFI_TIME();
    }
  }
  if (MAIN_OPTION == 1)
  {
    display.drawRoundRect(0, 29, 120, 12, 2, WHITE);
    if (ENTER_STATE == 1)
    {
      debugln("Set Time Manually");
      MENU = 3;
      SET_TIME_MANUALLY();
    }
  }
  if (MAIN_OPTION == 2)
  {
    display.drawRoundRect(0, 41, 120, 12, 2, WHITE);
    if (ENTER_STATE == 1)
    {
      debugln("Generator Config");
      MENU = 4;
      SET_GENERATOR_FUNCTIONS();
    }
  }
  if (MAIN_OPTION == 3)
  {
    display.drawRoundRect(0, 53, 120, 11, 2, WHITE);
    if (ENTER_STATE == 1)
    {
      debugln("Connection Config");
      MENU = 5;
      CONNECTION_CONFIG_DISPLAY();
    }
  }

  display.display();
}

void callback(char *topic, byte *message, unsigned int length)
{
  debug("Message arrived on topic: ");
  debug(topic);
  debug(". Message: ");
  String messageMode;
  String messageStart;
  String messageMains;
  String messageUTC;
  String messageStartTime;
  String messageStopTime;
  String messagepresance;

  for (int i = 0; i < length; i++)
  {
    debug((char)message[i]);
    messageMode += (char)message[i];
  }
  for (int i = 0; i < length; i++)
  {
    debug((char)message[i]);
    messageStart += (char)message[i];
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

  debugln();

  if (String(topic) == "GenCon/Cmnd/mode")
  {
    debug("Changing output to ");
    if (messageMode == "on")
    {
      debugln("Auto Mode");
      digitalWrite(ledPin, HIGH);
      MODE = 0;
    }
    else if (messageMode == "off")
    {
      debugln("Manual Mode");
      digitalWrite(ledPin, LOW);
      MODE = 1;
    }
  }

  if (String(topic) == "GenCon/Cmnd/start")
  {
    debug("Changing output to ");
    if (messageStart == "on")
    {
      if (started == false)
      {
        debugln("Start on");
        Auto_Start_Sequance();
      }
    }
    else if (messageStart == "off")
    {
      if (shutdown == false)
      {
        debugln("Shutting Off Generator");
        ShutDown_Sequance();
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
      if (shutdown == false)
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
      saveConfigFile();
    }
    else if (messagepresance == "off")
    {
      debugln("No Presance");
      presance_detect = false;
      saveConfigFile();
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
}

void setup()
{
  Serial.begin(115200);
  //---Load SPIFFS Data---//
  loadConfigFile();
  //---Wifi Settings---//
  // WiFi.disconnect();
  WiFi.mode(WIFI_STA);

  OLEDSetup();
  delay(10);
  //---Setup for Menu---//
  std::vector<const char *> menu = {"wifi", "setup", "param", "info", "sep", "restart", "exit"};
  wm.setMenu(menu);
  //---Set Dark Mode---//
  wm.setClass("invert");
  //---Wipe WiFi settings settings---//
  if (reset_data == true)
  {
    wm.resetSettings();
  }
  //---Set config save notify callback---//
  wm.setSaveConfigCallback(WebSave);
  wm.setSaveParamsCallback(WebSave);
  //---Setting Non Blocking & Auto close configportal after 120 seconds---//
  //wm.setConfigPortalTimeout(120);
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
  sprintf(crankConvertValue, "%d", crank_time);                                                               // Need to convert to string to display a default value.
  crank_time_convert = new WiFiManagerParameter("crank_time", "Generator Crank Time ", crankConvertValue, 3); // 2 == max length

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
  if (wifi_time)
  {
    wifi_time_Html = "type=\"checkbox\" checked";
  }
  else
  {
    wifi_time_Html = "type=\"checkbox\"";
  }
  wifi_time_checkbox = new WiFiManagerParameter("wifi_time", "WiFi Time", "t", 2, wifi_time_Html);

  //---Add all your parameters here---//
  wm.addParameter(mqtt_checkbox);
  wm.addParameter(custom_mqtt_server);
  wm.addParameter(custom_mqtt_username);
  wm.addParameter(custom_mqtt_password);
  wm.addParameter(mqtt_port_convert);
  wm.addParameter(wifi_time_checkbox);
  wm.addParameter(utc_convert);
  wm.addParameter(start_time_convert);
  wm.addParameter(stop_time_convert);
  wm.addParameter(crank_time_convert);

  //---Setting the WiFi SSID & Password, Starting Auto Connect or SoftAP---//
  /*bool res;                                                                                                // auto generated AP name from chipid
  res = wm.autoConnect(Access_Point, Password); 
  if (!res)
  {
    debugln("Failed to connect or hit timeout");
  }
  else
  {
    debugln("Connected to... :)"); // if you get here you have connected to the WiFi
    debugln("");
    debugln("WiFi connected");
    debugln("IP address: ");
    debugln(WiFi.localIP());
  }*/
  if(wm.autoConnect(Access_Point, Password)){
        Serial.println("connected...yeey :)");
    }
    else {
        Serial.println("Configportal running");
    }

  client.setServer(mqtt_server, mqtt_port); // Configure MQTT connexion
  client.setCallback(callback);             // MQTT Subscribe

  analogReadResolution(10);
  OLEDSetup();
  RTCsetup(); // RTC module Setup
  pinSetup(); // Pin Mode Setup
  inputStats.setWindowSecs(windowLength);
  chokeServo.attach(SERVO_PIN); // Servo attach pin to object
  NTPtimeClient.begin();        // NTP Client Start up
  RTCsetup();
}

void loop()
{
  //--- Wifi Manager Non Blocking Process and Web Portal ---//
  wm.process();
  if (!wm.getWebPortalActive())
  {
    wm.startWebPortal();
  }
  //--- MQTT Functions and connections ---//
  if (WiFi.status() != WL_CONNECTED){
    wm.process();
    if (wm.getConfigPortalActive()){
      wm.startConfigPortal();
    }
  }
  else
  {
    if (MQTT_Active == 1)
  {
    client.loop();
    if (!client.connected())
    {
      if (init_mqtt_conn = true)
      {
        reconnect();
      }
      else
      {
        mqttConnect();
      }    
    }
    mqttPublish();
  }

  //--- NTP Time Update---//
  if (wifi_time == 1)
    {
    NTPtimeClient.update();
    }
  }
  DateTime now = rtc.now();
  

  BATT_VOLT = (analogRead(BATT_SENSE) * 3.3 / (1023) / (0.2272));
  AC_12_VOLT = (analogRead(AC_12V_SENSE) * 3.3 / (1023)) / (0.2272);
  TEMPERATURE = rtc.getTemperature();
  overideSwitch(); // Run function to choose between Auto or Manual Mode overide button press
  Generator_Monitoring();

  if (MODE == 0 && MENU == 0) // ################################# Auto Mode
  {
    Display_Home_Screen();

    if ((now.hour()) >= start_time && (now.hour()) <= stop_time)
    {
      setLED(0, 180, 0);

      debugln("Hours are good for AUTO MODE");
      debugln(now.hour());
      debugln(start_time);
      debug("AC 12v Status = ");
      debugln(AC_12_VOLT);
      debug("Generator Voltage = ");
      debugln(GEN_VOLT);

      if (AC_12_VOLT < 6) // Power is out Run Auto Start
      {
        AC_input = false;
        delay(2000);           //***** Delay for small power falures
        Auto_Start_Sequance(); // Auto Start Sequance

        // if((unsigned long)(millis() - startpreviousMillis) >= startPeriod) {
        // startpreviousMillis = millis();   // update time every second
        delay(2000);        // ****** Delay
        if (GEN_VOLT > 210) // Voltage is Good on Generator
        {
          debugln("Generator is Good Turning on Mains");
          digitalWrite(RELAY_MAINS, HIGH);
          MODE = 2; // Monitoring Mode
        }
        else // Voltage is not good
        {
          debugln("Generator is not good");
          MODE = 1;
        }
        //}
      }
      else
      {
        AC_input = true;
      }
    }
    else
    {
      debugln("After Hours in AUTO MODE");
      setLED(0, 0, 0);
    }
  }
  if (MODE == 2 && MENU == 0) // ################################# Monitoring Mode
  {
    Display_Home_Screen();

    setLED(180, 0, 180); // Set LED to Purple while Monitoring
    // Monitoring mode Mode Code
    debugln("Monitoring Mode");

    if (AC_12_VOLT < 6) // Municipal Power remains off
    {
      runTime = millis();
      if ((unsigned long)(millis() - monitorpreviousMillis) >= monitorPeriod)
      {
        monitorpreviousMillis = millis(); // update time every second

        debug("Generator Voltage = ");
        debugln(GEN_VOLT);
        debug("AC 12V Status = ");
        debugln(AC_12_VOLT);

        if (GEN_VOLT < 150)
        {
          emergency = true;
          ShutDown_Sequance();
        }
      }

    }

    if (AC_12_VOLT > 6)
    {
      // Power came back on during monitoring
      setLED(180, 0, 50);

      debugln("Exiting Monitoring Mode");
      delay(5000);
      ShutDown_Sequance();
      debugln("Returning to AUTO");
      MODE = 0; // Returning to Auto Watch Mode
    }
  }
  if (MODE == 1 && MENU == 0) // ################################## Manual Mode
  {
    // Generator_Monitoring();

    Display_Home_Screen();
    setLED(180, 0, 0);
    int SELECT_STATE = digitalRead(SELECT_BTN);
    int ENTER_STATE = digitalRead(ENTER_BTN);
    int MAINS_State = digitalRead(MAINS_BTN);

    if (SELECT_STATE == 1 && ENTER_STATE == 1)
    {
      MENU = 1;
      MODE = 4;
      delay(button_delay);
    }
  }

  if (MODE == 4 && MENU == 1)
  {
    setLED(0, 0, 180);
    Display_Menu();
  }

  if ((unsigned long)(millis() - debugpreviousMillis) >= debugPeriod)
  {
    debugpreviousMillis = millis(); // update time every second
                                    // debug("mqtt_server : ");
                                    // debugln(mqtt_server);
                                    // debug("mqtt_username : ");
    // debugln(mqtt_username);
    // debug("mqtt_password : ");
    // debugln(mqtt_password);
    // debug("mqtt_port : ");
    // debugln(mqtt_port);

    // debug("UTC Offset : ");
    // debugln(utc_offset);
    // debug("Start Time : ");
    // debugln(start_time);
    // debug("Stop Time : ");
    // debugln(stop_time);
    // debug("Generator Crank Time : ");
    // debugln(crank_time);
    // debugln(NTPtimeClient.getFormattedTime());
    // debugln(NTPtimeClient.getDay());
    // debugln(now.dayOfTheWeek());
    // debugln(now.day());
    // debug("Temperature: ");
    // debug(rtc.getTemperature());
    // debugln(" C");
    // debug("MQTT Active: ");
    // debugln(MQTT_Active);
    // debug("Wifi Time: ");
    // debugln(wifi_time);
  }
}
