//--- Erase all wifi and Spiffs files ---//
//#define ERASE_WIFI


#define DEBUG 0

#if DEBUG == 1
#define debug(x) Serial.print(x)
#define debugln(x) Serial.println(x)
#else
#define debug(x)
#define debugln(x)
#endif

// Button Setup
#define OVERIDE_BTN 26
#define CHOKE_BTN 27
#define START_BTN 35
#define SELECT_BTN 33
#define ENTER_BTN 25
#define MAINS_BTN 32
#define GEN_ON_BTN 14

#define ENTER_PRESS (digitalRead(ENTER_BTN))
#define SELECT_PRESS (digitalRead(SELECT_BTN))
#define OVERIDE_PRESS (digitalRead(OVERIDE_BTN))
#define GEN_ON_PRESS (digitalRead(GEN_ON_BTN))
#define CHOKE_PRESS (digitalRead(CHOKE_BTN))
#define START_PRESS (digitalRead(START_BTN))
#define MAINS_PRESS (digitalRead(MAINS_BTN))

// Relay Setup

#define RELAY_ON 19
#define RELAY_START 18
#define RELAY_MAINS 12
#define RELAY_FAN 17

#define MAINS_TOGGLE_RELAY (digitalWrite(RELAY_MAINS, !digitalRead(RELAY_MAINS)))
#define GEN_TOGGLE_RELAY (digitalWrite(RELAY_ON, !digitalRead(RELAY_ON)))

//--- Wifi Manager Shared Defines and Variasbles ---//
#define Access_Point "GenCon WiFi"
#define Password "password"

#define JSON_CONFIG_FILE "/sample_config.json"

char mqtt_server[40] = "192.168.x.x";
char mqtt_username[20] = "MQTT Username";
char mqtt_password[20] = "MQTT Password";

uint16_t mqtt_port;
int8_t utc_offset = 0;
uint8_t start_time = 0;
uint8_t stop_time = 24;
uint8_t crank_time = 1;

//--- Screen, Modes and Menu's ---//
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define OLED_RESET -1       // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32

uint8_t MODE = 0;        // Mode for Generator usage 0 = Auto 1= Manual
uint8_t MENU = 1;

//--- MQTT Defines and Variables ---//
bool init_mqtt_conn = false;
#define PUBLISH_TIME 30                                     
#define UTC_offset_topic "GenCon/stat/utc-ofset"            
#define start_time_topic "GenCon/stat/start-time"           
#define stop_time_topic "GenCon/stat/stop-time"             
#define presance_detect_topic "GenCon/stat/presance-detect" 
#define power_state_topic "GenCon/stat/power-state"         
#define battery_volt_topic "GenCon/stat/batt-volt"          
#define temp_topic "GenCon/stat/temp"                       
#define gen_volt_topic "GenCon/stat/gen-volts"              
#define control_mode_topic "GenCon/stat/mode"
#define crank_time_topic "GenCon/stat/crank"

bool Wifi_Active = false;
bool NTP_time_active = false;
bool MQTT_Active = false;
bool presance_detect_Active = false;

bool presance_detect = false;
bool webSave = false;
bool initial_setup= false;

//--- Time based action Variables ---//
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

uint8_t NTP_Hrs; // Set Wifi Time,
uint8_t NTP_Min; // Set Wifi Time,
uint8_t NTP_Sec; // Set Wifi Time,
uint8_t NTP_Day; // Set Wifi Time,

uint16_t RTC_Year;  // Set RTC Time,
uint8_t RTC_Month; // Set RTC Time,
uint8_t RTC_Day;   // Set RTC Time,
uint8_t RTC_Hrs;   // Set RTC Time,
uint8_t RTC_Min;   // Set RTC Time,
uint8_t RTC_Sec;   // Set RTC Time,

uint16_t wifiReconnectPeriod = 5000;
uint32_t previousWifiReconnect = 0;

uint16_t mqtt_retry_Period = 5000; // in milliseconds
uint32_t mqtt_retry_previousMillis = 0;

uint16_t mqtt_Send_Period = 2000; // in milliseconds
uint32_t mqtt_Send_previous = 0;

uint16_t debugPeriod = 3000; // in milliseconds
uint32_t debugpreviousMillis = 0;

uint32_t startWaitMillis = 0;
uint16_t startWaitPeriod = 10;

static uint32_t startMillis = millis();

uint32_t shutdownWaitMillis = 0;
uint16_t shutdownWaitPeriod = 10000;

uint16_t monitorWaitPeriod = 3000; // in milliseconds
uint32_t monitorWaitMillis = 0;

//unsigned long runTime = 0;
//unsigned long totalRunTime = 0;

#define button_delay 150

//--- General GPIO Pins ---//

#define RED_LED 23
#define GREEN_LED 16
#define BLUE_LED 15

#define AC_12V_SENSE 36
#define BATT_SENSE 39
#define ZMPT 34
#define SERVO_PIN 13
#define DHT_SENSE 4

float BATT_VOLT = 0.0;
float AC_12_VOLT = 0.0;
int8_t TEMPERATURE = 0;

uint8_t servoMin = 0;
uint8_t servoMax = 90;

float GEN_VOLT;         
static float slope_intercept = 1.38;

bool mains_state = false;
bool started = false;
bool emergency = false;
bool Monitor_Active = true;


//-------- Menu Numbers ----------//
// MENU 0 = DISPLAY HOME SCREEN
// MENU 1 = DISPLAY MAIN MENU
// MENU 2 = DISPLAY SET TIME MENU
// MENU 3 = DISPLAY GENERATOR CONFIG
// MENU 4 = DISPLAY NETWORKING
// MENU 5 = DISPLAY CHOKE SET
// MENU 6 =

//-------- MODE Numbers ----------//
// MODE 0 = Manual Mode
// MODE 1 = AUTO Mode
// MODE 2 = Monitoring Mode
// MODE 3 = Menu Mode
// MODE 4 = Start Mode
// MODE 5 = Shutdown Mode




    enum class generatorState : uint8_t{
    IDLE_MANUAL,
    IDLE_AUTO,
    MENU,
    START_WAIT,
    STARTING,   
    CHOKE_ON,   
    CRANKING,   
    RUNNING,   
    STARTED,    
    MONITORING_INITIALIZE, 
    MONITORING_RUN,
    SHUTDOWN_WAIT,
    SHUTDOWN,
    EMERGENCY,
    };
    static generatorState currentState = generatorState::IDLE_MANUAL;


    