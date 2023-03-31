// #define ERASE_WIFI
#define DEBUG 1

#if DEBUG == 1
#define debug(x) Serial.print(x)
#define debugln(x) Serial.println(x)
#else
#define debug(x)
#define debugln(x)
#endif

//--- Wifi Manager Shared Defines and Variasbles ---//
#define Access_Point "GenCon WiFi"
#define Password "password"

#define JSON_CONFIG_FILE "/sample_config.json"

char mqtt_server[40] = "192.168.x.x";
char mqtt_username[20] = "MQTT Username";
char mqtt_password[20] = "MQTT Password";

int mqtt_port;
int utc_offset;
int start_time;
int stop_time;
int crank_time;

//--- Screen, Modes and Menu's ---//
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define OLED_RESET -1       // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32

int MAIN_OPTION = 0; // Display Menu, Loop
int MODE = 1;        // Mode for Generator usage 0 = Auto 1= Manual
int MENU = 0;

//--- MQTT Defines and Variables ---//
bool init_mqtt_conn = false;
#define PUBLISH_TIME 30                                     // Time in seconds to publish mqtt messages
#define UTC_offset_topic "GenCon/stat/utc-ofset"            // Topic windspeed Km/hr
#define start_time_topic "GenCon/stat/start-time"           // Topic windspeed Km/hr
#define stop_time_topic "GenCon/stat/stop-time"             // Topic windspeed Km/hr
#define presance_detect_topic "GenCon/stat/presance-detect" // Topic windspeed Km/hr
#define power_state_topic "GenCon/stat/power-state"         // Topic windspeed Km/hr
#define battery_volt_topic "GenCon/stat/batt-volt"          // Topic windspeed Km/hr
#define temp_topic "GenCon/stat/temp"                       // Topic windspeed Km/hr
#define gen_volt_topic "GenCon/stat/gen-volts"              // Topic windspeed Km/hr
#define control_mode_topic "GenCon/stat/mode"

// ###################################################################################//
// ################### Need to get check box working better ##########################//
bool MQTT_Active = false;
bool presance_detect = false;
bool use_presance_detect = false;
bool webSave = false;
bool reset_data = false;

//--- Time based action Variables ---//
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
bool wifi_time = false;

int NTP_Hrs; // Set Wifi Time,
int NTP_Min; // Set Wifi Time,
int NTP_Sec; // Set Wifi Time,
int NTP_Day; // Set Wifi Time,

int RTC_Year;  // Set RTC Time,
int RTC_Month; // Set RTC Time,
int RTC_Day;   // Set RTC Time,
int RTC_Hrs;   // Set RTC Time,
int RTC_Min;   // Set RTC Time,
int RTC_Sec;   // Set RTC Time,

unsigned long runTime = 0;
unsigned long totalRunTime;

unsigned long startPeriod = 3000; // in milliseconds
unsigned long startpreviousMillis = 0;

unsigned long debugPeriod = 10000; // in milliseconds
unsigned long debugpreviousMillis = 0;

unsigned long monitorPeriod = 3000; // in milliseconds
unsigned long monitorpreviousMillis = 0;

unsigned long mqtt_retry_Period = 5000; // in milliseconds
unsigned long mqtt_retry_previousMillis = 0;

#define button_delay 200

//--- General GPIO Pins ---//
#define RELAY_ON 19
#define RELAY_START 18
#define RELAY_MAINS 12
#define RELAY_FAN 17

#define OVERIDE_BTN 26
#define CHOKE_BTN 27
#define START_BTN 35
#define SELECT_BTN 33
#define ENTER_BTN 25
#define MAINS_BTN 32
#define GEN_ON_BTN 14

#define RED_LED 23
#define GREEN_LED 16
#define BLUE_LED 15

#define AC_12V_SENSE 36
#define BATT_SENSE 39
#define ZMPT 34
#define SERVO_PIN 13
#define DHT_SENSE 4

bool AC_input;
float BATT_VOLT = 0.0;
float AC_12_VOLT = 0.0;
int TEMPERATURE = 0;

int OverideSwitchOld = 1;
int overideState = 0;

float windowLength = 3; // how long to average the signal, for statistist
float intercept = 1.5;  // to be adjusted based on calibration testing
float slope = 1.26;     // to be adjusted based on calibration testing
float Volts_TRMS;       // estimated actual current in amps
float GEN_VOLT;         // Voltage

bool started = false;
bool shutdown = true;
bool emergency = false;