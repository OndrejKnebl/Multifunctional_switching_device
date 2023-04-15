/*******************************************************************************
 * The Things Network - OTAA Feather M0
 * 
 * Copyright (c) 2015 Thomas Telkamp and Matthijs Kooijman
 * Copyright (c) 2018 Terry Moore, MCCI
 *
 * Permission is hereby granted, free of charge, to anyone
 * obtaining a copy of this document and accompanying files,
 * to do whatever they want with them without any restriction,
 * including, but not limited to, copying, modification and redistribution.
 * NO WARRANTY OF ANY KIND IS PROVIDED.
 * 
 * Reference: https://github.com/mcci-catena/arduino-lmic
 *
 * Modified for Multifunctional switching device (Multifunkční spínací zařízení), Ondřej Knebl, 16. 4. 2023
 *******************************************************************************/
#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>

#include <CayenneLPP.h>               // Cayenne Low Power Payload (LPP)
CayenneLPP lpp(115);                  // 115 = max payload size for SF9/125kHz

#include "Adafruit_LC709203F.h"       // LC709203F
Adafruit_LC709203F lc;

#include <hp_BH1750.h>                // BH1750
hp_BH1750 BH1750;

#include <PZEM004Tv30.h>              // PZEM
PZEM004Tv30 pzem(Serial1, PZEM_DEFAULT_ADDR);

#include "RTClib.h"                   // RTC - DS3231
RTC_DS3231 rtc;

#include <TimeLib.h>                  // Time
#include <Timezone.h>

#include <eeprom_functions.h>         // Additional functions for EEPROM read / write
#include "Adafruit_EEPROM_I2C.h"      // RTC - EEPROM AT24C32
Adafruit_EEPROM_I2C i2ceeprom;
#define EEPROM_ADDR 0x57

#include <SunriseSunset.h>           // Sunset / sunrise library
#define sun_rise_set(year,month,day,lon,lat,rise,set) __sunriset__( year, month, day, lon, lat, -35.0/60.0, 1, rise, set )  // This macro computes times for sunrise/sunset

//---------------------------------------------------------------------------------------------------------------
//--------------------------------------- Here change your keys -------------------------------------------------
//---------------------------------------------------------------------------------------------------------------
static const u1_t PROGMEM APPEUI[8]={ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };   // AppEUI, LSB
void os_getArtEui (u1_t* buf) { memcpy_P(buf, APPEUI, 8);}                        

static const u1_t PROGMEM DEVEUI[8]={ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };   // DevEUI, LSB
void os_getDevEui (u1_t* buf) { memcpy_P(buf, DEVEUI, 8);}

static const u1_t PROGMEM APPKEY[16] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };  // AppKey, MSB
void os_getDevKey (u1_t* buf) {  memcpy_P(buf, APPKEY, 16);}

//---------------------------------------------------------------------------------------------------------------
//--------------------------------- Here change your default settings -------------------------------------------
//---------------------------------------------------------------------------------------------------------------
// This setting cannot be configured with downlinks
uint8_t configFPort = 2;                      // FPort number - Used for sending / receiving configuration data
uint8_t dataFPort = 1;                        // FPort number - Used for sending measured data

uint32_t myPassword = 1234;                   // Password to apply settings received in downlink

uint32_t user_time_correction = -18;          // Time correction in seconds - you have to find out experimentally
int requestTimeEverySend = 400;               // Request network time every 400 sends

enum _dr_configured_t mySF = DR_SF7;          // Spreading factor - Payload is > 51 bytes => only mySF = DR_SF7, DR_SF8 or DR_SF9

//-----------------------------------------
// Some settings that can be configured by downlinks
int workingMode = 0;                          // 0 = Open, 1 = Close, 2 = Light intensity, 3 = Time , 4 = Light intensity in Time, 5 = Sunset / sunrise switching times

float latitude = 0.0;                         // Device location - latitude
float longitude = 0.0;                        // Device location - longitude

uint32_t sendDataEvery = 60;                  // Send data every 1 minute (measured data)(60 - 3600)
uint32_t measureSamples = 6;                  // Number of samples +-1 (1-10)
float lightIThreshold = 0.0;                  // 0.0 = will close in full dark (0-65535)
float lightISafeZone = 0.0;                   // 0.0 = no safe zone (will be flickering) (0-65535)
int myTimeZone = 0;                           // Choosed timezone - cases in checkOnOffTime() (0-8)
int myPowerGrid = 0;                          // Power Grid voltage 0 = 230 V (one phase), 1 = 400 V (three phases)

//---------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------
uint8_t LMIC_IO_PIN = 11;                     // IO1 pin connected to pin 11

const lmic_pinmap lmic_pins = {               // Pin mapping for the Adafruit Feather M0
    .nss = 8,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = 4,
    .dio = {3, LMIC_IO_PIN, LMIC_UNUSED_PIN},
    .rxtx_rx_active = 0,
    .rssi_cal = 8,
    .spi_freq = 8000000,
};

static osjob_t measurejob, sendjob, resetFeatherjob;    // Jobs

// Reset Feather
int resetPin = 14;                            // Reset pin connected to pin 14 (A0)
bool resetFeather = false;                    // Set according to the downlink, if we want to restart Feather, then it is true
bool resetFeatherWithDelay = true;            // If any of the sensors does not work during device initialization, the device will automatically restart with a delay
bool rstFEraseEEPROM = false;                 // Set according to the downlink, if we want to delete the EEPROM with the saved configuration and restart the Feather, then this is true

// Relay
int relayPin = 12;                            // Relay controlled by pin 12
bool relaySwitchState = false;                // Contains information whether the relay is closed or open
int relayStateChanges = 0;                    // The variable contains information about the number of relay changes (closed/open) between sent uplinks

// Timer
unsigned long previousMillis = 0;             // Previous time
const long interval = 1000;                   // Interval 1000 ms
uint32_t countSeconds = 1;                    // Counting seconds

// Light Intensity sensor BH1750
uint8_t statusBH1750 = 1;                     // Information on the initial connection check of the BH1750 sensor
float lightIntensity = 0.0;                   // Variable for storing the sum of measured light intensity values
float numberOfSamplesBH = 0.0;                // Variable for storing the number of samples measured by the BH1750 sensor
bool lightISwitch = false;                    // Variable in which the information is stored, whether the relay can close or open depending on the light intensity

// LC709203F
uint8_t statusLC709203F = 1;                  // Information on the initial connection check of the LC709203F
float batteryVoltage = 0.0;                   // Variable for storing the sum of measured battery voltage values 
float batteryCapacity = 0.0;                  // Variable for storing the sum of measured battery capacity values
float batteryTemp = 0.0;                      // Variable for storing the sum of measured battery temperature values
float numberOfSamplesLC = 0.0;                // Variable for storing the number of samples measured by the LC709203F
float lastBatteryVoltage = 0.0;               // Variable for storing the last calculated battery voltage (for EEPROM write)

// PZEM
float voltage = 0.0;                          // Variable for storing the sum of measured power line voltage values
float current = 0.0;                          // Variable for storing the sum of measured current values
float power = 0.0;                            // Variable for storing the sum of measured power values
float energy = 0.000;                         // Variable for storing the measured energy value
float frequency = 0.0;                        // Variable for storing the sum of measured power line frequency values
float pf = 0.0;                               // Variable for storing the sum of measured power factor values
float numberOfSamplesPZEM_powerLine = 0.0;    // Variable for storing the number of samples measured by the PZEM - power line voltage, power line frequency
float numberOfSamplesPZEM_closedRelay = 0.0;  // Variable for storing the number of samples measured by the PZEM - values measured only if relay is closed

// Network time
int countSends = 0;                           // Counting sends
uint32_t userUTCTime;                         // Seconds since the UTC epoch

// RTC - DS3231
uint8_t statusDS3231 = 1;                     // Information on the initial connection check of the DS3231
float rtcTemp = 0.0;                          // Variable for storing the sum of measured RTC temperature values
float numberOfSamplesRTC = 0.0;               // Variable for storing the number of samples measured by the RTC

// RTC - EEPROM AT24C32
uint8_t statusAT24C256 = 1;                   // Information on the initial connection check of the EEPROM

// Timezones
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};     // Central European Summer Time
TimeChangeRule CET = {"CET ", Last, Sun, Oct, 3, 60};       // Central European Standard Time
Timezone CE(CEST, CET);                                     // Central European Time (Frankfurt, Paris)
TimeChangeRule BST = {"BST", Last, Sun, Mar, 1, 60};        // British Summer Time
TimeChangeRule GMT = {"GMT", Last, Sun, Oct, 2, 0};         // Standard Time
Timezone UK(BST, GMT);                                      // United Kingdom (London, Belfast)
TimeChangeRule utcRule = {"UTC", Last, Sun, Mar, 1, 0};     // UTC
Timezone UTC(utcRule);                                      // UTC
TimeChangeRule usEDT = {"EDT", Second, Sun, Mar, 2, -240};  // Eastern Daylight Time = UTC - 4 hours
TimeChangeRule usEST = {"EST", First, Sun, Nov, 2, -300};   // Eastern Standard Time = UTC - 5 hours
Timezone usET(usEDT, usEST);                                // US Eastern Time Zone (New York, Detroit)
TimeChangeRule usCDT = {"CDT", Second, Sun, Mar, 2, -300};
TimeChangeRule usCST = {"CST", First, Sun, Nov, 2, -360};
Timezone usCT(usCDT, usCST);                                // US Central Time Zone (Chicago, Houston)
TimeChangeRule usMDT = {"MDT", Second, Sun, Mar, 2, -360};
TimeChangeRule usMST = {"MST", First, Sun, Nov, 2, -420};
Timezone usMT(usMDT, usMST);                                // US Mountain Time Zone (Denver, Salt Lake City)
Timezone usAZ(usMST);                                       // Arizona is US Mountain Time Zone but does not use DST
TimeChangeRule usPDT = {"PDT", Second, Sun, Mar, 2, -420};
TimeChangeRule usPST = {"PST", First, Sun, Nov, 2, -480};
Timezone usPT(usPDT, usPST);                                // US Pacific Time Zone (Las Vegas, Los Angeles)
TimeChangeRule aEDT = {"AEDT", First, Sun, Oct, 2, 660};    // UTC + 11 hours
TimeChangeRule aEST = {"AEST", First, Sun, Apr, 3, 600};    // UTC + 10 hours
Timezone ausET(aEDT, aEST);                                 // Australia Eastern Time Zone (Sydney, Melbourne)

// Switching times
#define numberOfTimes 6                                                                                                                               // Number of times in array
String setTimes[numberOfTimes] = {"small_time_101", "small_time_102", "small_time_103", "small_time_104", "small_time_105", "small_time_106"};        // Close = small_time_101, small_time_103, small_time_105  Open = small_time_102, small_time_104, small_time_106
uint32_t onOffTimesSet[numberOfTimes] = {0};                                                                                                          // Individual switch-on and switch-off times are stored in this array
bool needSortTimes = false;                                                                                                                           // If we want to sort the times
bool timeSwitch = false;                                                                                                                              // Variable in which information is stored whether the relay can close or open depending on the set times
String setTimesNotSort[numberOfTimes] = {"small_time_101", "small_time_102", "small_time_103", "small_time_104", "small_time_105", "small_time_106"}; // Close = small_time_101, small_time_103, small_time_105  Open = small_time_102, small_time_104, small_time_106
uint32_t onOffTimesSetNotSort[numberOfTimes] = {0};                                                                                                   // Individual switch-on and switch-off times are stored in this array

// Configuration from downlink and errors
bool sendConfiguration = false;                 // Sending the current configuration
bool saveNewConfig = false;                     // Saving the new configuration to the EEPROM
byte errors[8] = {0};                           // [0] - Password error, [1] - Location error, [2] - Power Grid error, [3] - Timezone error, [4] - Switching times error, [5] - Working mode error, [6] - Number of measured samples error , [7] - Send data every error

// First value
bool isFirst = true;                            // If this is the first data sent in the uplink after switching on / restarting the device

// Send only selected
#define numberOfValues 2                                                                    // Number of variables for storing settings - Send only selected
String setValues[numberOfValues] = {"digital_out_1", "digital_out_2"};                      
bool* sendValuesSet = (bool*)malloc(numberOfValues * 8 * sizeof(*sendValuesSet));           // All "Send only selected" are stored in this array

// Sunset / sunrise
#define numbOfSunTimes 2                                                          // Number of times (we have only one sunset and one sunrise, so this is two)
String setSunTimes[numbOfSunTimes] = {"small_time_111", "small_time_112"};        // Sunrise = small_time_111, Sunset = small_time_112
uint32_t onOffSunTimesSet[numbOfSunTimes] = {0};                                  // Individual switch-on and switch-off times are stored in this array
bool sunSetRiseTime = false;                                                      // Variable in which information is stored whether the relay can close or open depending on the Sunset / Sunrise times

//---------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------
void bubbleSortOnOffTimes(uint32_t a[], String b[],  int size){       // Bubble Sort function for sorting times (sorting two fields at the same time, because we need to know which value belongs to which time)
    for(int i = 0; i < (size - 1); i++) {                             // https://forum.arduino.cc/t/solved-sort-array-according-to-integer-array/699181
        for(int o = 0; o < (size - (i + 1)); o++) {
            if(((a[o] < a[o + 1]) && (a[o] >= 0)) || (a[o+1] < 0)){
                // array a
                uint32_t t = a[o];
                a[o] = a[o + 1];
                a[o + 1] = t;
                // array b
                String st = b[o];
                b[o] = b[o + 1];
                b[o + 1] = st;
            }
        }
    }
}
//---------------------------------------------------------------------------------------------------------------
// Received downlink - Processing functions

void downlinkResetFeather(JsonObject root){                         // Downlink - Reset Feather
    if(root.containsKey("digital_in_100")){
        uint8_t digital_in_100 = root["digital_in_100"];
        if(digital_in_100 == 1){                                        // 1 = Reset and load Saved - settings in EEPROM
            resetFeather = true;
            resetFeatherWithDelay = false;  
        }else if(digital_in_100 == 2){                                  // 2 = Reset and load Default - settings from Feather program
            rstFEraseEEPROM = true;
            resetFeatherWithDelay = false;   
        }
    }
}

void downlinkSetSendTime(JsonObject root){                          // Downlink - Send data every
    if(root.containsKey("small_time_100")){                             // If JsonObject contains "small_time_100"
        uint32_t small_time_100 = root["small_time_100"];               // Get value of the key "small_time_100" and store it to the variable small_time_100
        if(small_time_100 >= 60 && small_time_100 <= 3600){             // 60 - 3600 seconds
            sendDataEvery = small_time_100;                             // Setting the small_time_100 value to the sendDataEvery variable used in the program
            saveNewConfig = true;                                       // Configuration will be saved to EEPROM (all settings, not only this setting)
        }else{                                                          // If value is not in range => Error
            Serial.println("Time Error!!!");                            // Print Error to Serial Monitor
            errors[7] = 1;                                              // Save the error in the error array
        }
    }
}

void downlinkNumberOfSamples(JsonObject root){                      // Downlink - Number of measured samples      
    if(root.containsKey("presence_100")){
        uint32_t presence_100 = root["presence_100"];
        if(presence_100 >= 1 && presence_100 <= 10){                    // 1 - 10 samples
            measureSamples = presence_100;
            saveNewConfig = true;
        }else{
            Serial.println("Samples Error!!!");
            errors[6] = 1;
        }
    }
}

void downlinkWorkingMode(JsonObject root){                          // Downlink - Working Mode
    if(root.containsKey("digital_in_101")){ 
        uint32_t digital_in_101 = root["digital_in_101"];
        if(digital_in_101 >= 0 && digital_in_101 <= 5){                 // 0 = Open, 1 = Close, 2 = Light intensity, 3 = Time, 4 = Light intensity in Time, 5 = Sunset / sunrise times
            workingMode = digital_in_101;
            saveNewConfig = true;       
        }else{
          Serial.println("Working Mode Error!!!");
          errors[5] = 1;
        }
    }
}

void downlinkTimezone(JsonObject root){                             // Downlink - Timezone
    if(root.containsKey("digital_in_102")){ 
        uint32_t digital_in_102 = root["digital_in_102"];
        if(digital_in_102 >= 0 && digital_in_102 <= 8){                 // 0 - Central European Time, 1 - United Kingdom, 2 - UTC, 3 - US Eastern Time Zone, 4 - US Central Time Zone,
            myTimeZone = digital_in_102;                                // 5 - US Mountain Time Zone, 6 - US Arizona, 7 - US Pacific Time Zone, 8 - Australia Eastern Time Zone
            saveNewConfig = true;       
        }else{
          Serial.println("Timezone Error!!!");
          errors[3] = 1;
        }
    }
}

void downlinkPowerGrid(JsonObject root){                            // Downlink - Power Grid
    if(root.containsKey("digital_in_103")){ 
        uint32_t digital_in_103 = root["digital_in_103"];
        if(digital_in_103 >= 0 && digital_in_103 <= 1){                 // 0 = 230 V, 1 = 400 V 
            myPowerGrid = digital_in_103;
            saveNewConfig = true;       
        }else{
          Serial.println("Power Grid Error!!!");
          errors[2] = 1;
        }
    }
}

void downlinkLIThreshold(JsonObject root){                          // Downlink - Light Intensity Threshold      
    if(root.containsKey("luminosity_101")){
        lightIThreshold = root["luminosity_101"];                       // 0-65535, no check because it's not possible send lower or bigger value of this type
        saveNewConfig = true;
    }
}

void downlinkLISafeZone(JsonObject root){                           // Downlink - Light Intensity Safe Zone
    if(root.containsKey("luminosity_102")){
        lightISafeZone = root["luminosity_102"];                        // 0-65535, no check because it's not possible send lower or bigger value of this type
        saveNewConfig = true;
    }
}

void downlinkCoordinates(JsonObject root){                          // Downlink - Location of device (for Sunset / Sunrise times)
    if(root.containsKey("gps_101")){
        float gps_101_latitude = root["gps_101"]["latitude"];
        float gps_101_longitude = root["gps_101"]["longitude"];
        if((gps_101_latitude >= -90.0 && gps_101_latitude <= 90.0) && (gps_101_longitude >= -180.0 && gps_101_longitude <= 180.0)){   // -90 <= latitude <= 90,   -180 <= longitude <= 180 
            latitude = gps_101_latitude;
            longitude = gps_101_longitude;
            saveNewConfig = true;
        }else{
          Serial.println("Location Error!!!");
          errors[1] = 1;
        }
    }
}

void downlinkSwitchingTimes(JsonObject root){                       // Downlink - Switching times
    for(int i = 0; i < numberOfTimes; i++){
        if(root.containsKey(setTimes[i])){
            uint32_t onOffTime = root[setTimes[i]];
            needSortTimes = true;                                       // it will be necessary to sort the time field

            if(onOffTime >= 0 && onOffTime <= 86399){                   // 0-86399 seconds of day
                onOffTimesSet[i] = 1672531200 + onOffTime;              // Store 1672531200 (Sun Jan 01 2023 00:00:00 GMT+0000) plus new time in switching times array
                saveNewConfig = true;
            }else if(onOffTime == 100000){                              // 100000 == Time not set
                onOffTimesSet[i] = 0;                                   // Store 0 in switching times array
                saveNewConfig = true;
            }else{
                Serial.println("On/Off Time Error!!!");
                errors[4] = 1;
            }
        }
    }

    for(int i = 0; i < numberOfTimes; i++){                             // Again because we need an unsorted array to write to the EEPROM
        if(root.containsKey(setTimesNotSort[i])){
            uint32_t onOffTime = root[setTimesNotSort[i]];

            if(onOffTime >= 0 && onOffTime <= 86399){                 
                onOffTimesSetNotSort[i] = 1672531200 + onOffTime;     
                saveNewConfig = true;
            }else if(onOffTime == 100000){                            
                onOffTimesSetNotSort[i] = 0;
                saveNewConfig = true;
            }else{
                Serial.println("On/Off Time Error!!!");
                errors[4] = 1;
            }
        }
    }
}

void downlinkSendOnlySelected(JsonObject root){                     // Downlink - Send only selected
    for(int i = 0; i < numberOfValues; i++){
        bool oneSendValuesSet[8];

        if(root.containsKey(setValues[i])){
            int32_t value = root[setValues[i]];
            for(int j = 7; j >= 0; j--)
                oneSendValuesSet[j] = (value & (1 << j)) != 0;                  // Getting the value of individual bits from the value variable and storing these values in the oneSendValuesSet array as boolean values.

            memcpy(sendValuesSet + (8*i), oneSendValuesSet, 8 * sizeof(bool));  // Copying data from array to another resulting sendValuesSet array
            saveNewConfig = true;
        }
    }
}

void decodedDownlinkProcessing(JsonObject root){      // Function of processing decoded data from the downlink

    downlinkResetFeather(root);
    downlinkSetSendTime(root);
    downlinkNumberOfSamples(root);
    downlinkWorkingMode(root);
    downlinkTimezone(root);
    downlinkPowerGrid(root);
    downlinkLIThreshold(root);
    downlinkLISafeZone(root);
    downlinkCoordinates(root);
    downlinkSwitchingTimes(root);
    downlinkSendOnlySelected(root);

    if((saveNewConfig) && (lastBatteryVoltage >=3.65)){                   // If saveNewConfig is true and the battery voltage is greater than or equal to 3.65 V, then save the configuration
        saveNewConfiguration();
    }
    if(needSortTimes){                                                    
        bubbleSortOnOffTimes(onOffTimesSet, setTimes, numberOfTimes);     // Call function for sorting Switching times 
        needSortTimes = false;
    }
}
//---------------------------------------------------------------------------------------------------------------
// Just printing

void printHex2(unsigned v) {
    v &= 0xff;
    if (v < 16)
        Serial.print('0');
    Serial.print(v, HEX);
}

void printDebugData1(){
    Serial.print(F("Received "));
    Serial.print(LMIC.dataLen);
    Serial.println(F(" bytes of payload"));

    Serial.print(F("Frame: "));
    for (int i = 0; i < LMIC.dataBeg + LMIC.dataLen + 4; i++)
      printHex2(LMIC.frame[i]);
    Serial.println();

    Serial.print(F("FPort: "));
    Serial.println(LMIC.frame[LMIC.dataBeg - 1]);
}

void printDebugData2(JsonObject root){
    Serial.print(F("My Data: "));
    for (int i = LMIC.dataBeg; i < LMIC.dataBeg+LMIC.dataLen; i++)
      printHex2(LMIC.frame[i]);
    Serial.println();

    serializeJsonPretty(root, Serial);
    Serial.println();
    Serial.println(root.size());
}
//---------------------------------------------------------------------------------------------------------------

void onEvent (ev_t ev) {
    Serial.print(os_getTime());
    Serial.print(": ");
    switch(ev) {
        case EV_JOINING:
            Serial.println(F("EV_JOINING"));
            break;
        case EV_JOINED:
            Serial.println(F("EV_JOINED"));
            {
              u4_t netid = 0;
              devaddr_t devaddr = 0;
              u1_t nwkKey[16];
              u1_t artKey[16];
              LMIC_getSessionKeys(&netid, &devaddr, nwkKey, artKey);
              Serial.print("netid: ");
              Serial.println(netid, DEC);
              Serial.print("devaddr: ");
              Serial.println(devaddr, HEX);
              Serial.print("AppSKey: ");
              for (size_t i=0; i<sizeof(artKey); ++i) {
                if (i != 0)
                  Serial.print("-");
                printHex2(artKey[i]);
              }
              Serial.println("");
              Serial.print("NwkSKey: ");
              for (size_t i=0; i<sizeof(nwkKey); ++i) {
                      if (i != 0)
                              Serial.print("-");
                      printHex2(nwkKey[i]);
              }
              Serial.println();
            }
            // It must be set again here, otherwise a different setting is used
            LMIC_setLinkCheckMode(0);       // Disable link check validation
            LMIC.dn2Dr = DR_SF9;            // TTS uses SF9 for its RX2 window.
            LMIC_setDrTxpow(mySF,14);       // Set data rate and transmit power for uplink
            LMIC_setAdrMode(0);             // Adaptive data rate disabled
            break;
        case EV_JOIN_FAILED:
            Serial.println(F("EV_JOIN_FAILED"));
            break;
        case EV_REJOIN_FAILED:
            Serial.println(F("EV_REJOIN_FAILED"));
            break;
        case EV_TXCOMPLETE:
            Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
            if (LMIC.txrxFlags & TXRX_ACK)
              Serial.println(F("Received ack"));
                                                                                          // Processing of received downlink data; The FPort is checked and the password is verified before the downlink data is processed.
            if (LMIC.dataLen) {                                                           // If data was received
                printDebugData1();                                                    
                uint8_t fport = LMIC.frame[LMIC.dataBeg - 1];                             // The FPort from frame is stored in the fport variable
                               
                if(fport == configFPort){                                                 // If fport is equal to configFPort
                    DynamicJsonDocument jsonBuffer(1024);                                 // A DynamicJsonDocument, JsonObject, and downlinkData byte array are created
                    JsonObject root = jsonBuffer.to<JsonObject>();
                    byte downlinkData[1024] = {0};

                    byte j = 0;
                    for(byte i = LMIC.dataBeg; i < LMIC.dataBeg + LMIC.dataLen; i++){     // Copying the received data from frame (just our data) to the downlinkData array
                        downlinkData[j] = LMIC.frame[i];
                        j++;
                    }
                    lpp.decodeTTN(downlinkData, LMIC.dataLen, root);                      // Decoding data from Cayenne LPP format to JSON root object using lpp.decodeTTN() function
                    printDebugData2(root);

                    if(root.size() != 0){                                                     // The data was successfully decoded from Cayenne LPP format
                        if(root.containsKey("power_100")){                                        // Downlink - Password
                            uint32_t power_100 = root["power_100"];
                            if(power_100 == myPassword){                                              // If password is correct
                                sendConfiguration = true;
                                decodedDownlinkProcessing(root);                                      // Process downlink data
                            }else{
                                Serial.println("Password Error!!!");
                                errors[0] = 1;
                                sendConfiguration = true;
                            }
                        }
                    }
                    jsonBuffer.clear();                                                       // Finally, the jsonBuffer memory is freed.
                }
            }
            if(resetFeather){                                                       // If resetFeather is true, reset Feather and load saved configuration from EEPROM
                os_setCallback(&resetFeatherjob, do_resetFeather);
            }else if(rstFEraseEEPROM){                                              // If rstFEraseEEPROM is true, erase EEPROM and restart Feather to default configuration
                Serial.println("EEPROM - Erasing!");
                eraseEEPROM();
                os_setCallback(&resetFeatherjob, do_resetFeather);                  
            }else if(sendConfiguration){                                            // Send the new configuration that has been applied
                os_setCallback(&sendjob, do_send);                                            
            }else{                                                                  // Call do_measure
                os_setCallback(&measurejob, do_measure);
            }
            break;
        case EV_RESET:
            Serial.println(F("EV_RESET"));
            break;
        case EV_TXSTART:
            Serial.println(F("EV_TXSTART"));
            break;
        case EV_TXCANCELED:
            Serial.println(F("EV_TXCANCELED"));
            break;
        case EV_JOIN_TXCOMPLETE:
            Serial.println(F("EV_JOIN_TXCOMPLETE: no JoinAccept"));
            break;
        default:
            Serial.print(F("Unknown event: "));
            Serial.println((unsigned) ev);
            break;
    }
}
//---------------------------------------------------------------------------------------------------------------

void do_resetFeather(osjob_t* j){         // Function to restart Feather
    if(resetFeatherWithDelay){
        delay(300000); 
    }
    relayControl(false);                      // Open Relay
    delay(2000); 
    digitalWrite(resetPin, LOW);              // Restart Feather
}


void user_request_network_time_callback(void *pVoidUserUTCTime, int flagSuccess){           // Provides time acquisition from LoRaWAN network, calculation and setting of system time and RTC
    uint32_t *pUserUTCTime = (uint32_t *) pVoidUserUTCTime;                                     // Explicit conversion from void* to uint32_t* to avoid compiler errors
    lmic_time_reference_t lmicTimeReference;

    if(flagSuccess != 1){
        Serial.println("USER CALLBACK: Not a success");
        return;
    }

    flagSuccess = LMIC_getNetworkTimeReference(&lmicTimeReference);                             // Populate "lmic_time_reference"
    if (flagSuccess != 1) {
        Serial.println("USER CALLBACK: LMIC_getNetworkTimeReference didn't succeed");
        return;
    }
    *pUserUTCTime = lmicTimeReference.tNetwork + 315964800;                                     // Update userUTCTime, considering the difference between the GPS and UTC epoch, and the leap seconds
    ostime_t ticksNow = os_getTime();                                                           // Current time, in ticks
    ostime_t ticksRequestSent = lmicTimeReference.tLocal;                                       // Time when the request was sent, in ticks
    uint32_t requestDelaySec = osticks2ms(ticksNow - ticksRequestSent) / 1000;                  // Delay between sending the received time request and adding that delay to pUserUTCTime.
    *pUserUTCTime += requestDelaySec;
    *pUserUTCTime += user_time_correction;                                                      // User time correction

    setTime(*pUserUTCTime);                                                                     // Update the system time with the time read from the network
    rtc.adjust(*pUserUTCTime);                                                                  // Set new network time to RTC
    Serial.println("USER CALLBACK: Success");
}


uint32_t convertTimeBack(uint32_t time){        // Function to get the time values (Switching times) in the format in which they were received in the downlink
    if(time >= 1672531200){
      time = time - 1672531200;                 // Value of Set time
    }else if(time == 0){
      time = time = 100000;                     // Time not set
    }
    return time;
}


void addConfigError(){
    int errorsInOne = 0;                                    // Variable for the resulting Error value 

    for(int i = 0; i < 8; i++){                             // Merging the values from the errors array into errorsInOne, where each bit of errorsInOne represents a value from the errors array at the corresponding index
        errorsInOne = errorsInOne | (errors[i] << i);       
        errors[i] = 0;
    }

    lpp.addDigitalInput(150, errorsInOne);                  // Add all Errors in one variable into channel 150 of type Digital Input
    lpp.addDigitalInput(101, workingMode);                  // Add Working mode into channel 101 of type Digital Input
    lpp.addDigitalInput(102, myTimeZone);                   // Add Timezone into channel 102 of type Digital Input
    lpp.addDigitalInput(103, myPowerGrid);                  // Add Power grid into channel 103 of type Digital Input
    lpp.addSmallTime(100, sendDataEvery);                   // Add Send data every into channel 100 of type Small time (custom type)
    lpp.addPresence(100, measureSamples);                   // Add Number of samples into channel 100 of type Presence
    lpp.addLuminosity(101, lightIThreshold);                // Add Threshold into channel 101 of type Luminosity
    lpp.addLuminosity(102, lightISafeZone);                 // Add Safe zone into channel 102 of type Luminosity
    lpp.addGPS(101, latitude, longitude, 0);                // Add device location into channel 101 of type GPS Location

    for (int i = numberOfTimes - 1; i >= 0; i--){                         // Add all Switching times in the format in which they were received in the downlink
        if(setTimes[i] == "small_time_101")
            lpp.addSmallTime(101, convertTimeBack(onOffTimesSet[i]));         // Add On time1 into channel 101 of type Small time (custom type)
        if(setTimes[i] == "small_time_102")
            lpp.addSmallTime(102, convertTimeBack(onOffTimesSet[i]));         // Add Off time1 into channel 102 of type Small time (custom type)
        if(setTimes[i] == "small_time_103")
            lpp.addSmallTime(103, convertTimeBack(onOffTimesSet[i]));         // Add On time2 into channel 103 of type Small time (custom type)
        if(setTimes[i] == "small_time_104")
            lpp.addSmallTime(104, convertTimeBack(onOffTimesSet[i]));         // Add Off time2 into channel 104 of type Small time (custom type)
        if(setTimes[i] == "small_time_105")
            lpp.addSmallTime(105, convertTimeBack(onOffTimesSet[i]));         // Add On time3 into channel 105 of type Small time (custom type)
        if(setTimes[i] == "small_time_106")
            lpp.addSmallTime(106, convertTimeBack(onOffTimesSet[i]));         // Add Off time3 into channel 106 of type Small time (custom type)
    }

    byte oneSendValuesSet[8] = {0};
    int digital_out[numberOfValues] = {0};

    for(int i = 0; i < numberOfValues; i++){                  // Processing "Send only selected" array to store it in two values in new array digital_out[]
        for(int j = 0; j < 8; j++)
            oneSendValuesSet[j] = sendValuesSet[j+(8*i)];

        int mask = 0x80;
        int32_t value = 0;
        for (int j = 7; j >= 0; j--){
            if( oneSendValuesSet[j] == 1) value |= mask;
                mask = mask >> 1;
        }
        digital_out[i] = value;
    }
    
    lpp.addDigitalOutput(1, digital_out[0]);                  // Add first half of "Send only selected" data into channel 1 of type Digital Output (Check buttons - Relay state, Number of changes, ...)
    lpp.addDigitalOutput(2, digital_out[1]);                  // Add second half of "Send only selected" data into channel 1 of type Digital Output (Check buttons - Power line frequency, Active energy, ...)
}


void do_send(osjob_t* j){
    if(isFirst){                                                                                          // If this is the first data sent in the uplink after switching on / restarting the device
        byte statuses[8] = {statusBH1750, statusLC709203F, statusDS3231, statusAT24C256, 0, 0, 0, 0};         // [0] - statusBH1750, [1] - statusLC709203F, [2] - statusDS3231, [3] - statusAT24C256 
        int statusesInOne = 0;

        for(int i = 0; i < 8; i++){                                                                           // Merging the values from the statuses array into statusesInOne
            statusesInOne = statusesInOne | (statuses[i] << i);
        }
        lpp.reset();
        lpp.addDigitalInput(51, statusesInOne);                                                               // Add all statuses in statusesInOne variable into channel 51 of type Digital Input
      
        if((statusBH1750 == 0) || (statusLC709203F == 0) || (statusDS3231 == 0) || (statusAT24C256 == 0)){    // If initial connection check failed - Send sensor / module error and reset Feather with delay
            LMIC_setTxData2(configFPort, lpp.getBuffer(), lpp.getSize(), 0);                                  // Prepare upstream data transmission at the next possible time.
            resetFeather = true;
        }else{                                                                                                // Sensors and modules works, send status, configuration, errors and request network time
            addConfigError();

            LMIC_requestNetworkTime(user_request_network_time_callback, &userUTCTime);                        // Schedule a network time request at the next possible time
            countSends++;                                                                                     // countSends + 1

            LMIC_setTxData2(configFPort, lpp.getBuffer(), lpp.getSize(), 0);                                  // Prepare upstream data transmission at the next possible time.
        }
        isFirst = false;

    }else if(sendConfiguration){                                                                              // Send configuration and errors
        lpp.reset();
        addConfigError();
        sendConfiguration = false;
        saveNewConfig = false;
        LMIC_setTxData2(configFPort, lpp.getBuffer(), lpp.getSize(), 0);                                      // Prepare upstream data transmission at the next possible time.

    }else{
        lpp.reset();

        // If Send only selected, add (measured) values to uplink data 
        if(sendValuesSet[0]) lpp.addSwitch(1, relaySwitchState);                       // Add Relay state into channel 1 of type Switch
        if(sendValuesSet[1]) lpp.addSwitch(2, relayStateChanges);                      // Add Number of relay changes between sends into channel 2 of type Switch          
        if(sendValuesSet[2]) lpp.addLuminosity(3, uint16_t(lightIntensity));           // Add Light intensity into channel 3 of type Luminosity
        if(sendValuesSet[3]) lpp.addVoltage(4, batteryVoltage);                        // Add Battery voltage into channel 4 of type Voltage
        if(sendValuesSet[4]) lpp.addPercentage(5, uint32_t(batteryCapacity));          // Add Battery percentage into channel 5 of type Percentage
        if(sendValuesSet[5]) lpp.addTemperature(6, batteryTemp);                       // Add Battery temperature into channel 6 of type Temperature
        if(sendValuesSet[6]) lpp.addTemperature(7, rtcTemp);                           // Add RTC temperature into channel 7 of type Temperature
        if(sendValuesSet[7]) lpp.addVoltage(8, voltage);                               // Add Power Line Voltage into channel 8 of type Voltage
        if(sendValuesSet[8]) lpp.addFrequency(9, frequency);                           // Add Power Line Frequency into channel 9 of type Frequency

        if((numberOfSamplesPZEM_closedRelay > 0) && (myPowerGrid == 0)){               // If relay was/is closed and Power grid is set to 230 V
            if(sendValuesSet[9])  lpp.addEnergy(10, energy);                              // Add Active Energy into channel 10 of type Energy
            if(sendValuesSet[10]) lpp.addCurrent(11, current);                            // Add Current into channel 11 of type Current
            if(sendValuesSet[11]) lpp.addPower(12, power);                                // Add Active Power into channel 12 of type Power
            if(sendValuesSet[12]) lpp.addAnalogOutput(13, pf);                            // Add Power factor into channel 13 of type Analog Output
        }

        for(int i = numbOfSunTimes - 1; i >= 0; i--){                                   // Process Sunset / Sunrise array
            if(setSunTimes[i] == "small_time_111"){
                if(sendValuesSet[13]) lpp.addSmallTime(111, onOffSunTimesSet[i]);           // Add Sunrise into channel 111 of type Small time (custom type)
            }
            if(setSunTimes[i] == "small_time_112"){
                if(sendValuesSet[14]) lpp.addSmallTime(112, onOffSunTimesSet[i]);           // Add Sunset into channel 111 of type Small time (custom type)
            }
        }

        if(countSends % requestTimeEverySend == 0){                                     // If condition is met, request network time
            LMIC_requestNetworkTime(user_request_network_time_callback, &userUTCTime);      // Schedule a network time request at the next possible time
            countSends = 0;
        }
        countSends++;

        LMIC_setTxData2(dataFPort, lpp.getBuffer(), lpp.getSize(), 0);                  // Prepare upstream data transmission at the next possible time.
        resetValues();                                                                  // Reset values
    }
}


void resetValues() {                          // Reset values
    lightIntensity = 0.0;
    numberOfSamplesBH = 0.0;
    batteryVoltage = 0.0;
    batteryCapacity = 0.0;
    batteryTemp = 0.0;
    numberOfSamplesLC = 0.0;    
    rtcTemp = 0.0;
    numberOfSamplesRTC = 0.0;

    if(energy >= 0.001)
        pzem.resetEnergy();

    voltage = 0.0;
    current = 0.0;
    power = 0.0;
    energy = 0.0;
    frequency = 0.0;
    pf = 0.0;
    numberOfSamplesPZEM_powerLine = 0.0;
    numberOfSamplesPZEM_closedRelay = 0.0;
    relayStateChanges = 0;
}


void averagesCalculation(){                                     // Function for calculating averages from the sums of measured values and the number of samples of individual sensors
    
    if(numberOfSamplesBH > 0){
        lightIntensity = lightIntensity / numberOfSamplesBH;
    }
    if(numberOfSamplesRTC > 0){
        rtcTemp = rtcTemp / numberOfSamplesRTC;
    }
    if(numberOfSamplesLC > 0){
        batteryVoltage = batteryVoltage / numberOfSamplesLC;
        batteryCapacity = batteryCapacity / numberOfSamplesLC;
        batteryTemp = batteryTemp / numberOfSamplesLC;
        lastBatteryVoltage = batteryVoltage;
    }
    if(numberOfSamplesPZEM_powerLine > 0){
        voltage = voltage / numberOfSamplesPZEM_powerLine;
        frequency = frequency / numberOfSamplesPZEM_powerLine;
    }
    if(numberOfSamplesPZEM_closedRelay > 0){
        current = current / numberOfSamplesPZEM_closedRelay;
        power = power / numberOfSamplesPZEM_closedRelay;
        pf = pf / numberOfSamplesPZEM_closedRelay;
    }
}


void measureLightIntesity(){
    BH1750.start();                                       // Starts a BH1750 measurement
    lightIntensity = lightIntensity + BH1750.getLux();    // Addition of the currently measured value to other measured values of light intensity
    numberOfSamplesBH++;                                  // Add +1 to number of samples of BH1750 mesurements
}


void measureRTCTemp(){
    rtcTemp = rtcTemp + rtc.getTemperature();             // Addition of the currently measured value to other measured values of RTC temperature
    numberOfSamplesRTC++;                                 // Add +1 to number of samples of RTC mesurements
}


void measureBatVoltageCapacityTemp(){
    float currentBatteryVoltage = lc.cellVoltage();
    float currentBatteryCapacity = lc.cellPercent();
    float currentBatteryTemp = lc.getCellTemperature();

    if((currentBatteryVoltage != 0.0) && (currentBatteryCapacity != 0.0)){    // Check that these values are not zeros (sometimes there is a read error)
        batteryVoltage = batteryVoltage + currentBatteryVoltage;              // Addition of the currently measured value to other measured values of Battery voltage
        batteryCapacity = batteryCapacity + currentBatteryCapacity;           // Addition of the currently measured value to other measured values of Battery percentage
        batteryTemp = batteryTemp + currentBatteryTemp;                       // Addition of the currently measured value to other measured values of Battery temperature
        numberOfSamplesLC++;                                                  // Add +1 to number of samples of LC709203F mesurements
    }
}


void measurePZEM(){
    float currentVoltage = pzem.voltage();
    float currentCurrent = pzem.current();
    float currentPower = pzem.power();
    float currentEnergy = pzem.energy();
    float currentFrequency = pzem.frequency();
    float currentPf = pzem.pf();

    if(!(isnan(currentVoltage)) && !(isnan(currentCurrent)) && !(isnan(currentPower)) && !(isnan(currentEnergy)) && !(isnan(currentFrequency)) && !(isnan(currentPf))){ // If these values are not NaN
        voltage = voltage + currentVoltage;                       // Addition of the currently measured value to other measured values of Power Line Voltage
        frequency = frequency + currentFrequency;                 // Addition of the currently measured value to other measured values of Power Line Frequency
        numberOfSamplesPZEM_powerLine++;                          // Add +1 to number of PZEM power line samples

        energy = currentEnergy;                                   // Energy measurement

        if(relaySwitchState == true){                             // This is only added if the relay is closed
            current = current + currentCurrent;                       // Addition of the currently measured value to other measured values of Current
            power = power + currentPower;                             // Addition of the currently measured value to other measured values of Power
            pf = pf + currentPf;                                      // Addition of the currently measured value to other measured values of Power Factor
            numberOfSamplesPZEM_closedRelay++;                        // Add +1 to number of PZEM relay closed samples
        }
    }
}


void updateSunsetRise(String sunTime, double riseSet, int i){    // Function to update the values of sunrise and sunset times
    if(sunTime == setSunTimes[i]){                                  // To set the value to the correct Sunrise / Sunset variable
        if(riseSet < 0.0){                                          // The SunriseSunset library can return negative time values or time values greater than 24 hours, so we return them to our range 
            riseSet = (24.0 + riseSet);
        }
        if(riseSet >= 24.0){
            riseSet = (riseSet - 24.0);
        }
        uint32_t onOffSunTime = uint32_t(riseSet * 3600.0);         // Time in seconds of day = time in hours * 3600

        if(onOffSunTime >= 0 && onOffSunTime <= 86399){             // 0 - 86399 seconds in day
            onOffSunTimesSet[i] = onOffSunTime;                     // Set the Sunrise / Sunset value
        }else{
            Serial.println("Sun time Error!!!");
        }
    }
}


void checkSunsetRiseTime(){
    double rise, set;                               // Variables for sunrise and sunset times
    int rs;

    for(int i = 0; i < numbOfSunTimes; i++){        // Zeroing the times stored in the Sunset / Sunrise array
        onOffSunTimesSet[i] = 0;
    }

    DateTime now = rtc.now();
    rs = sun_rise_set(now.year(), now.month(), now.day(), longitude, latitude, &rise, &set);        // Getting sunrise and sunset times calculated using the SunriseSunset library

    if(rs == 0){                                                                                    // The sun rises and sets                                  
      for(int i = 0; i < numbOfSunTimes; i++){                                                          // Update the values of sunrise and sunset times
          updateSunsetRise("small_time_111", rise, i);
          updateSunsetRise("small_time_112", set, i);
      }
      bubbleSortOnOffTimes(onOffSunTimesSet, setSunTimes, numbOfSunTimes);                              // Sort sunrise and sunset times arrays

      uint32_t secondsTimeUTC = now.hour() * 3600 + now.minute() * 60 + now.second();
      
      for(int i = numbOfSunTimes - 1; i >= 0; i--){                                                     // Finding out if the relay should close or open according to the current time and the sunrise and sunset times stored in the array
        if(onOffSunTimesSet[i] <= secondsTimeUTC){ 
            if(setSunTimes[i] == "small_time_112")                                                      // Sunset - can close relay
                sunSetRiseTime = true;
            if(setSunTimes[i] == "small_time_111")                                                      // Sunrise - can open relay        
                sunSetRiseTime = false;
        }
        if(onOffSunTimesSet[i] > secondsTimeUTC)
            break;            
      }
    }else if(rs == 1){                                                                              // Sun above horizon
        sunSetRiseTime = false;                                                                         // Open relay
    }else if(rs == -1){                                                                             // Sun below horizon
        sunSetRiseTime = true;                                                                          // Close relay
    }
}


void compareOnOffTimes(Timezone tz, uint32_t utc){                                                  // A function to determine whether a relay should close or open based on the current time in the selected time zone and on/off times stored in the arrays
    TimeChangeRule *tcr;
    uint32_t t = tz.toLocal(utc, &tcr);                                                                                             // Local time from UTC time, taking into account the time zone and time change rules (summer time)
    uint32_t actualTime = (hour(t)*3600) + (minute(t)*60) + second(t) + 1672531200;                                                 // Seconds of day in selected Timezone (date, year ignored)
    
    for(int i = numberOfTimes - 1; i >= 0; i--){                                                                                    // Finding out if the relay should close or open
        if(onOffTimesSet[i] != 0){                                                                                                      // Time is set if it is not 0
            if(onOffTimesSet[i] <= actualTime){ 
                if((setTimes[i] == "small_time_101") || (setTimes[i] == "small_time_103") || (setTimes[i] == "small_time_105"))                 // Can close relay
                    timeSwitch = true;
                if((setTimes[i] == "small_time_102") || (setTimes[i] == "small_time_104") || (setTimes[i] == "small_time_106"))                 // Can open relay        
                    timeSwitch = false;
            }
            if(onOffTimesSet[i] > actualTime)                                                                                               // Break, because we have sorted array
                break;
        }
    }
}


void checkOnOffTime(){                            // In this function, the comparison of Switching times in the selected Timezone is called (finding whether to turn open / close the relay according to the time)
    DateTime now = rtc.now();

    if(myTimeZone == 0){                              // 0 - Central European Time
        compareOnOffTimes(CE, now.unixtime());
    }else if(myTimeZone == 1){                        // 1 - United Kingdom
        compareOnOffTimes(UK, now.unixtime());
    }else if(myTimeZone == 2){                        // 2 - UTC
        compareOnOffTimes(UTC, now.unixtime());
    }else if(myTimeZone == 3){                        // 3 - US Eastern Time Zone
        compareOnOffTimes(usET, now.unixtime());
    }else if(myTimeZone == 4){                        // 4 - US Central Time Zone
        compareOnOffTimes(usCT, now.unixtime());
    }else if(myTimeZone == 5){                        // 5 - US Mountain Time Zone
        compareOnOffTimes(usMT, now.unixtime());
    }else if(myTimeZone == 6){                        // 6 - US Arizona
        compareOnOffTimes(usAZ, now.unixtime());
    }else if(myTimeZone == 7){                        // 7 - US Pacific Time Zone
        compareOnOffTimes(usPT, now.unixtime());
    }else if(myTimeZone == 8){                        // 8 - Australia Eastern Time Zone
        compareOnOffTimes(ausET, now.unixtime());
    }
}


void checkLightIThreshold(){                                      // Function that determines whether the relay should close or open based on the average Light intensity, Threshold and Safe zone
    float actualLIaverage = lightIntensity / numberOfSamplesBH;       // Calculating the average from the sum of the measured Light intensity values and the number of Light intensity samples
    
    if(actualLIaverage <= lightIThreshold)                            // When the current average Light intensity value is less than or equal to the Threshold   
        lightISwitch = true;                                              // Can close relay
    else if(actualLIaverage >= (lightIThreshold + lightISafeZone))    // When the current average Light intensity value is greater than or equal to the sum of the Threshold and Safe zone values
        lightISwitch = false;                                             // Can open relay
}


void workingModeHandler(){              // Function in which the operating mode of the device is selected and the relay switching is controlled accordingly
    if(workingMode == 0){                   // 0 = OFF
        relayControl(false);                    // Open relay
    }else if(workingMode == 1){             // 1 = ON
        relayControl(true);                     // Close relay
    }else if(workingMode == 2){             // 2 = Light intensity
        relayControl(lightISwitch);             // The relay is controlled by the result of Light intensity processing
    }else if(workingMode == 3){             // 3 = Time
        relayControl(timeSwitch);               // The relay is controlled by the result of Switching times processing
    }else if(workingMode == 4){             // 4 = Light intensity in Time
        if(timeSwitch && lightISwitch){         // The relay is controlled by the results of Light intensity and Switching times processings
            relayControl(true);
        }else{
            relayControl(false);
        }
    }else if(workingMode == 5){             // 5 = Sunset / Sunrise times
        relayControl(sunSetRiseTime);           // The relay is controlled by the result of Sunset / Sunrise times processing
    }
}


void relayControl(bool relaySwitch){          // Function in which a relay is opened or closed and counts the number of times that relay state has been changed
    if (relaySwitch != relaySwitchState){         // If the state of the relay has been changed, we add +1 to relayStateChanges
        relayStateChanges++;
    }

    if(relaySwitch){                              // If relaySwitch is true, close the relay
        relaySwitchState = true;                      // Saving the current state of the relay
        digitalWrite(relayPin, HIGH);                 // HIGH = close relay
    }else{
        relaySwitchState = false;                     // Saving the current state of the relay
        digitalWrite(relayPin, LOW);                  // LOW = open relay
    }
}


// One of most important control function, in which the timing is performed and the measurement functions are called according to the set intervals, 
// determining when the relay should be switched on/off, when the values will be averaged and when the measured data will be sent in the uplink
void do_measure(osjob_t* j){
    unsigned long currentMillis = millis();                 // Current milliseconds
      
    if(currentMillis - previousMillis >= interval) {        // Timer set to 1 second
        previousMillis = currentMillis;                         // Storing the current millisecond value in the previousMillis variable

        Serial.print(countSeconds);
        Serial.print(", ");

        workingModeHandler();                                   // Calling a function in which the operating mode of the device is selected and the relay switching is controlled accordingly

        if((countSeconds % 1) == 0){                            // Check Switching times and Sunset / Sunrise times every second
            checkOnOffTime();                                       // Calling a function in which the comparison of Switching times in the selected Timezone is called
            checkSunsetRiseTime();                                  // Calling a function in which the sunset and sunrise times are calculated, processed, compared to the actual time, and the result is whether the relay should close or open
        }

        if((countSeconds % (sendDataEvery / measureSamples)) == 0){   // Measure samples according to the calculated interval, Warning: rounded down (sometimes more samples than set)
            measureBatVoltageCapacityTemp();                              // Calling a function in which the battery values are measured
            measureLightIntesity();                                       // Calling a function in which the Light intensity is measured
            measurePZEM();                                                // Calling a function in which the PZEM values are measured
            measureRTCTemp();                                             // Calling a function in which the RTC temperature is measured
            checkLightIThreshold();                                       // Calling a function that determines whether the relay should close or open based on the average Light intensity, Threshold and Safe zone
        }

        if(countSeconds % sendDataEvery == 0){                  // Send (measured) data in set interval
            Serial.println();
            averagesCalculation();
            os_setCallback(&sendjob, do_send);                      // Call do_send
            countSeconds = 0;                                       // Zero seconds counter
        }
        countSeconds++;                                         // + 1 second for timer
    }

    if((countSeconds - 1) % sendDataEvery != 0){            // When is called do_send, don't call do_measure
        os_setCallback(&measurejob, do_measure);                // Call do_measure
    }
}

//---------------------------------------------------------------------------------------------------------------
//---------------------------------- EEPROM - saving and loading configuration ----------------------------------
//---------------------------------------------------------------------------------------------------------------
void saveNewConfiguration(){                                                  // Function to save the new configuration received in the downlink to the EEPROM
    Serial.println("EEPROM - Writing!");

    char cmd[] = "LOAD";                                                      // So that we know we want to load the configuration stored in the EEPROM when the device is turned on/restarted
    uint8_t buffer[5];                                                        // Buffer
    memcpy(buffer, (void *)&cmd, sizeof(cmd));                                // Copy cmd to buffer
    writeDoubledValues(0X00, buffer, sizeof(cmd));                            // Write buffer to EEPROM starting from address 0x00 and ending according to size of cmd
    memcpy(buffer, (void *)&workingMode, sizeof(workingMode));                // Copy workingMode to buffer
    writeDoubledValues(0X10, buffer, sizeof(workingMode));                    // Write buffer to EEPROM starting from address 0x10 and ending according to size of workingMode
    memcpy(buffer, (void *)&sendDataEvery, sizeof(sendDataEvery));            // ...
    writeDoubledValues(0X20, buffer, sizeof(sendDataEvery));
    memcpy(buffer, (void *)&measureSamples, sizeof(measureSamples));
    writeDoubledValues(0X30, buffer, sizeof(measureSamples));
    memcpy(buffer, (void *)&lightIThreshold, sizeof(lightIThreshold));
    writeDoubledValues(0X40, buffer, sizeof(lightIThreshold));
    memcpy(buffer, (void *)&lightISafeZone, sizeof(lightISafeZone));
    writeDoubledValues(0X50, buffer, sizeof(lightISafeZone));
    memcpy(buffer, (void *)&myTimeZone, sizeof(myTimeZone));
    writeDoubledValues(0X60, buffer, sizeof(myTimeZone));
    memcpy(buffer, (void *)&latitude, sizeof(latitude));
    writeDoubledValues(0X70, buffer, sizeof(latitude));
    memcpy(buffer, (void *)&longitude, sizeof(longitude));
    writeDoubledValues(0X80, buffer, sizeof(longitude));
    memcpy(buffer, (void *)&myPowerGrid, sizeof(myPowerGrid));
    writeDoubledValues(0X90, buffer, sizeof(myPowerGrid));

    uint16_t addrSend = 0X100;                                              
    for(int i = 0; i < 8 * numberOfValues; i++){
        memcpy(buffer, (void *)&sendValuesSet[i], sizeof(sendValuesSet[i]));  //  As described above, but in a for loop
        writeDoubledValues(addrSend, buffer, sizeof(sendValuesSet[i])); 
        addrSend += 0x02;             
    }
    addrSend = 0X130;
    for(int i = 0; i < numberOfTimes; i++){
        memcpy(buffer, (void *)&onOffTimesSetNotSort[i], sizeof(onOffTimesSetNotSort[i])); //  As described above, but in a for loop
        writeDoubledValues(addrSend, buffer, sizeof(onOffTimesSetNotSort[i])); 
        addrSend += 0x08;             
    }
    Serial.println("EEPROM - Saved!");
}


void loadSavedConfing(){                                                      // Function to load configuration stored in EEPROM memory
    int newValueInt = 65535;
    uint32_t newValue32 = 4294967295;
    float newValueFloat = 3.4028235;
    bool newBool = false;
    String newString;
    bool returnedVal = false;
                                                                                    // workingMode
    returnedVal = loadAndCompareInts(0x10, &newValueInt);                           // Call function in which the integer data stored on two addresses is loaded, compared and if they are the same, then returnedVal is true
    if((returnedVal) && (newValueInt >= 0 && newValueInt <= 5)){                         // 0 = Open, 1 = Close, 2 = Light intensity, 3 = Time , 4 = Light intensity in Time, 5 = Sunset / sunrise switching times
        Serial.print("EEPROM - Setting workingMode to: ");
        Serial.println(newValueInt);
        workingMode = newValueInt;                                                       // The loaded newValueInt value from the EEPROM memory is stored in the workingMode variable
    }else{
        errors[5] = 1;
        Serial.println("EEPROM - Working Mode Error!!!");
    }

    returnedVal = loadAndCompareUints(0x20, &newValue32);                         // sendDataEvery
    if((returnedVal) && (newValue32 >= 60 && newValue32 <= 3600)){                // 60-3600 seconds
        Serial.print("EEPROM - Setting sendDataEvery to: ");
        Serial.println(newValue32);
        sendDataEvery = newValue32;
    }else{
        Serial.println("EEPROM - Time Error!!!");
        errors[7] = 1;
    }

    returnedVal = loadAndCompareUints(0x30, &newValue32);                         // measureSamples
    if((returnedVal) && (newValue32 >= 1 && newValue32 <= 10)){                   // 1-10 samples
        Serial.print("EEPROM - Setting measureSamples to: ");
        Serial.println(newValue32);
        measureSamples = newValue32;
    }else{
        Serial.println("EEPROM - Samples Error!!!");
        errors[6] = 1;
    }

    returnedVal = loadAndCompareFloats(0x40, &newValueFloat);                     // lightIThreshold
    if((returnedVal) && (newValueFloat >= 0 && newValueFloat <= 65535)){          // 0-65535 lux
        Serial.print("EEPROM - Setting lightIThreshold to: ");
        Serial.println(newValueFloat);
        lightIThreshold = newValueFloat;
    }else{
        Serial.println("EEPROM - Light Intensity Threshold Error!!!");
    }

    returnedVal = loadAndCompareFloats(0x50, &newValueFloat);                     // lightISafeZone
    if((returnedVal) && (newValueFloat >= 0 && newValueFloat <= 65535)){          // 0-65535 lux
        Serial.print("EEPROM - Setting lightISafeZone to: ");
        Serial.println(newValueFloat);
        lightISafeZone = newValueFloat;
    }else{
        Serial.println("EEPROM - Light Intensity Safe Zone Error!!!");
    }

    returnedVal = loadAndCompareInts(0x60, &newValueInt);                         // myTimeZone
    if((returnedVal) && (newValueInt >= 0 && newValueInt <= 8)){                  // 0-8
        Serial.print("EEPROM - Setting myTimeZone to: ");
        Serial.println(newValueInt);
        myTimeZone = newValueInt;
    }else{
        Serial.println("EEPROM - myTimeZone Error!!!");
        errors[3] = 1;
    }

    returnedVal = loadAndCompareFloats(0x70, &newValueFloat);                     // latitude
    if((returnedVal) && (newValueFloat >= -90.0 && newValueFloat <= 90.0)){       // -90 - 90
        Serial.print("EEPROM - Setting latitude to: ");
        Serial.println(newValueFloat);
        latitude = newValueFloat;
    }else{
        Serial.println("EEPROM - Device location - latitude Error!!!");
        errors[1] = 1;
    }

    returnedVal = loadAndCompareFloats(0x80, &newValueFloat);                     // longitude
    if((returnedVal) && (newValueFloat >= -180.0 && newValueFloat <= 180.0)){     // -180 - 180
        Serial.print("EEPROM - Setting longitude to: ");
        Serial.println(newValueFloat);
        longitude = newValueFloat;
    }else{
        Serial.println("EEPROM - Device location - longitude Error!!!");
        errors[1] = 1;
    }

    returnedVal = loadAndCompareInts(0x90, &newValueInt);                         // myPowerGrid
    if((returnedVal) && (newValueInt >= 0 && newValueInt <= 1)){                  // 0 = 230 V, 1 = 400 V
        Serial.print("EEPROM - Setting myPowerGrid to: ");
        Serial.println(newValueInt);
        myPowerGrid = newValueInt;
    }else{
        Serial.println("EEPROM - Power Grid Error!!!");
        errors[2] = 1;
    }

    uint16_t addrSend = 0X100;                                                    // sendValuesSet
    for(int i = 0; i < 8 * numberOfValues; i++){
        returnedVal = loadAndCompareBools(addrSend, &newBool);
        if(returnedVal){
            Serial.print("EEPROM - Setting one of sendValuesSet to: ");
            Serial.println(newBool);
            sendValuesSet[i] = newBool;
        }else{
            Serial.println("EEPROM - sendValuesSet Error!!!");
        }
        addrSend += 0x02;             
    }

    addrSend = 0X130;                                                             // onOffTimesSet
    for(int i = 0; i < numberOfTimes; i++){
        returnedVal = loadAndCompareUints(addrSend, &newValue32);
        if((returnedVal) && ((newValue32 >= 1672531200 && newValue32 <= 1672617599) || (newValue32 == 0))){            
            Serial.print("EEPROM - Setting one of onOffTimesSet to: ");
            Serial.println(newValue32);
            onOffTimesSet[i] = newValue32;
            onOffTimesSetNotSort[i] = newValue32; 
        }else{
            Serial.println("EEPROM - onOffTimesSet Error!!!");
            errors[4] = 1;
        }
        addrSend += 0x08; 
    }

    bubbleSortOnOffTimes(onOffTimesSet, setTimes, numberOfTimes);                 // Sort loaded Switching times
}


void loadConfig(){                                                                // Function to find out what device configuration to set
    char loadBuff[5];
    char loadBuff2[5];
    uint8_t buffer[sizeof(loadBuff)];
    uint8_t buffer2[sizeof(loadBuff)];
    readDoubledValues(0x00, buffer, buffer2, sizeof(loadBuff));
    
    memcpy((void *)&loadBuff, buffer, sizeof(loadBuff));
    memcpy((void *)&loadBuff2, buffer2, sizeof(loadBuff2));

    if((strcmp(loadBuff, "LOAD") == 0) && (strcmp(loadBuff2, "LOAD") == 0)){          // If LOAD was read twice from the EEPROM memory, the configuration values are read from the EEPROM memory and set to Feather
        Serial.println("Loading saved settings from EEPROM!");
        loadSavedConfing();
    }else{                                                                            // Otherwise, the configuration values from the Feather program are used
        Serial.println("Loading default Feather settings!");      
    }
}
//---------------------------------------------------------------------------------------------------------------

void setup() {

    digitalWrite(LED_BUILTIN, LOW);                       // Turning off the Feather's built-in LED

    Serial1.begin(9600);                                  // Serial1 for communication with PZEM
    Serial.begin(9600);                                   // Serial for printing to Serial Monitor
    Serial.println(F("Starting"));

    digitalWrite(resetPin, HIGH);                         // Pin of the Feather which is connected to the reset pin of the Feather
    pinMode(resetPin, OUTPUT);

    digitalWrite(relayPin, LOW);                          // Pin of the Feather which is connected to the relay
    pinMode(relayPin, OUTPUT);

    for(int i = 0; i < 8 * numberOfValues; i++)           // Setting all array values with "Send only selected" to true
        sendValuesSet[i] = true;

    os_init();
    LMIC_reset();

    //EU868
    LMIC_setupChannel(0, 868100000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    LMIC_setupChannel(1, 868300000, DR_RANGE_MAP(DR_SF12, DR_SF7B), BAND_CENTI);      // g-band
    LMIC_setupChannel(2, 868500000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    LMIC_setupChannel(3, 867100000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    LMIC_setupChannel(4, 867300000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    LMIC_setupChannel(5, 867500000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    LMIC_setupChannel(6, 867700000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    LMIC_setupChannel(7, 867900000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    LMIC_setupChannel(8, 868800000, DR_RANGE_MAP(DR_FSK,  DR_FSK),  BAND_MILLI);      // g2-band

    LMIC_setLinkCheckMode(0);                             // Disable link check validation
    LMIC.dn2Dr = DR_SF9;                                  // TTS uses SF9 for its RX2 window.
    LMIC_setDrTxpow(mySF,14);                             // Set data rate and transmit power for uplink
    LMIC_setAdrMode(0);                                   // Adaptive data rate disabled

    LMIC_setClockError(MAX_CLOCK_ERROR * 1 / 100); 


    // Check sensors and modules - BH1750, LC709203F, RTC - DS3231 and EEPROM
    if (! BH1750.begin(BH1750_TO_GROUND)) {
        Serial.println("Could not find a valid BH1750 sensor, check wiring, address, sensor ID!");
        statusBH1750 = 0;
    }

    if (!lc.begin()) {
        Serial.println(("Couldnt find Adafruit LC709203F?\nMake sure a battery is plugged in!"));
        statusLC709203F = 0;
    }
    lc.setThermistorB(3950);
    lc.setPackSize(LC709203F_APA_3000MAH);

    if (!rtc.begin()) {
        Serial.println("Couldn't find RTC");
        statusDS3231 = 0;
    }

    if (!i2ceeprom.begin(EEPROM_ADDR)) {
        Serial.println("I2C EEPROM not identified ... check your connections?\r\n");
        statusAT24C256 = 0;
    } else {
        loadConfig();           // Load configuration - Saved from EEPROM / Default from Feather
    }
    
    do_send(&sendjob);          // Start sendjob
}

void loop() {
    os_runloop_once();
}