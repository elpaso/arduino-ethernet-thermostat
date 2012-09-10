/**
*
* Thermostat
*
*/

// Needed for prog_char PROGMEM
//#include <avr/pgmspace.h>

/** ****************************************************
*
* Main constants, all times in millis
*/

// Number of rooms
#define ROOMS 2

// Pins
#define ONE_WIRE_PIN 8
#define PUMP_PIN 9

// TODO: move to config

#define TEMP_READ_INTERVAL 4000
#define VALVE_OPENING_TIME 120000 // 2 minutes
#define BLOCKED_TIME 3600000
#define RISE_TEMP_TIME 600000
#define RISE_TEMP_DELTA 0.5
#define IDLE_TIME 10000
#define HYSTERESIS 0.5
#define LCD_USR_SCREEN_ROTATE_TIME 4000

// Room status
#define OPENING 'V' // valves are opening for VALVE_OPENING_TIME
#define CLOSED 'C' // Closed
#define OPEN 'O' // Open (main pump is also open)
#define BLOCKED 'B' // Blocked until BLOCKED_TIME is elapsed


//#include "Timer.h"
Timer t;


/** *****************************************************
*
* RTC part
*
*/

//#include <Wire.h>
//#include "RTClib.h"

RTC_DS1307 RTC;
DateTime now;
// For settings menu:
byte hh, mm, d, m, yOff;

/*
void RTC_set(){
    if(hh || mm || d || m || yOff){
        hh = 0;
        mm = 0;
        d = 0;
        m = 0;
        yOff = 0;
        RTC.adjust(DateTime(yOff, m, d, hh, mm, 0));
    }
}*/

/******************************
 *
 * EEPROM
 *
 */
//#include <EEPROM.h>


/** ************************************************
*
* DS18B20 part
*
*/

//#include <OneWire.h>
//#include <DallasTemperature.h>

// Data wire is plugged into pin 6 on the Arduino
#define ONE_WIRE_BUS ONE_WIRE_PIN

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

// Assign the addresses of your 1-Wire temp sensors.
// See the tutorial on how to obtain these addresses:
// http://www.hacktronics.com/Tutorials/arduino-1-wire-address-finder.html


/** *************************************
 *
 * Programs
 *
 */

// Global time (minutes from 0)
unsigned int this_time;
byte this_weekday;

// Global OFF
byte off = 0;
byte pump_open = 0;
byte info_room = 0;

// Temperatures
// TODO: configurable
float T[] = {5, 15, 18, 28};

// Programs
// 8 slots    6:30  8:00 12:00 13:00 16:00 20:00 22:00
unsigned int slot[] = { 390,  480,  720,  780,  960, 1200, 1320 };
// 6 programs, T level for each slot/pgm tuple
byte daily_program[][8] = {
    //0:00 6:30  8:00 12:00 13:00 16:00 20:00 22:00
    {    0,   0,    0,    0,    0,    0,    0,    0 }, // all T0
    {    1,   1,    1,    1,    1,    1,    1,    1 }, // all T1
    {    2,   2,    2,    2,    2,    2,    2,    2 }, // all T2
    {    3,   3,    3,    3,    3,    3,    3,    3 }, // all T3
    {    1,   3,    1,    1,    1,    3,    2,    1 }, // awakening supper and evening 4
    {    1,   3,    1,    3,    1,    3,    2,    1 },  // awakening, meals and evening 5
    {    1,   3,    1,    3,    3,    3,    2,    1 },  // awakening, meals, afternoon and evening 6
    {    1,   3,    3,    3,    3,    3,    2,    1 },  // all day 7
};

// Weekly programs, 0 is monday
byte weekly_program[][7] = {
    //  Mo Tu Th We Fr Sa Su
        {0, 0, 0, 0, 0, 0, 0}, // always off
        {1, 1, 1, 1, 1, 1, 1}, // Always 1
        {2, 2, 2, 2, 2, 2, 2}, // Always 2
        {3, 3, 3, 3, 3, 3, 3}, // Always 3
        {4, 4, 4, 4, 4, 7, 7}, // 4 (5+2)
        {4, 4, 4, 4, 4, 4, 7}, // 4 (6+1)
        {5, 5, 5, 5, 5, 7, 7}, // 5 (5+2)
        {5, 5, 5, 5, 5, 5, 7}, // 5 (6+1)
        {6, 6, 6, 6, 6, 7, 7}, // 6 (5+2)
        {6, 6, 6, 6, 6, 6, 7} // 6 (6+1)
};


// Array of rooms
struct room_t {
  int name;
  DeviceAddress address;
  byte pin;
  byte program;
  char status;
  byte last_status;
  float temperature;
  float old_temperature;
  unsigned long last_status_change;
} rooms[ROOMS] = {
    {1, { 0x28, 0xAD, 0x4C, 0xC4, 0x03, 0x00, 0x00, 0x13}, 10, 3},
    {2, { 0x28, 0x6C, 0x41, 0xC4, 0x03, 0x00, 0x00, 0x57}, 11, 8}
};



float get_desired_temperature(byte room){
    // Get slot
    byte _slot = 0;
    while(_slot <= 6 && this_time > slot[_slot]){
        _slot++;
    }
    return T[daily_program[weekly_program[rooms[room].program][this_weekday]][_slot]];
}

/**
 * Check temperatures and perform actions
 */
void check_temperatures(){

    sensors.requestTemperatures();

    now = RTC.now();
    this_time = now.hour() * 60 + now.minute();
    this_weekday = now.dayOfWeek(); // sunday is 0
    this_weekday = this_weekday ? this_weekday - 1 : 6;

    pump_open = 0;
    int needs_heating = false;
    for(int i=0; i<ROOMS; i++){
        // Get temperature
        float tempC = sensors.getTempC(rooms[i].address);
        if (tempC != -127.00) {
            rooms[i].old_temperature = rooms[i].temperature;
            rooms[i].temperature = tempC;
        }
        byte new_status = rooms[i].status;
        needs_heating = new_status == OPENING;
        if(! needs_heating){
            float t = get_desired_temperature(i);
            needs_heating = rooms[i].temperature < (new_status == OPEN ? t + HYSTERESIS : t - HYSTERESIS);
        }
        if(!needs_heating){
            new_status = CLOSED;
        } else {
            switch(rooms[i].status){
                case OPENING:
                    if(VALVE_OPENING_TIME < millis() - rooms[i].last_status_change){
                        new_status = OPEN;
                    }
                    break;
                case OPEN:
                    if(RISE_TEMP_TIME <  millis() - rooms[i].last_status_change){
                        if(rooms[i].temperature - rooms[i].old_temperature < RISE_TEMP_DELTA){
                            new_status = BLOCKED;
                        }
                    } else {
                        pump_open = 1;
                    }
                    break;
                case BLOCKED:
                    if(BLOCKED_TIME <  millis() - rooms[i].last_status_change){
                        new_status = CLOSED;
                    }
                    break;
                default:
                case CLOSED:
                    new_status = OPENING;
            }
        }
        if(new_status != rooms[i].status){
            rooms[i].last_status_change = millis();
            rooms[i].status = new_status;
        }
        digitalWrite(rooms[i].pin, new_status == OPENING || new_status == OPEN);
    }
    digitalWrite(PUMP_PIN, pump_open);
}




/**
 * Set up
 *
 */
void thermo_setup(){

    Wire.begin();
    RTC.begin();
    /*
    if (! RTC.isrunning()) {
#if DEBUG
        Serial.println("RTC is NOT running!");
#endif
        // following line sets the RTC to the date & time this sketch was compiled
        RTC.adjust(DateTime(__DATE__, __TIME__));
    }
    */


  pinMode(PUMP_PIN, OUTPUT);



    // Sensors
    // set the resolution to 12 bit (maximum)
    sensors.begin();
    for (int i=0; i<ROOMS; i++){
        sensors.setResolution(rooms[i].address, 12);
        pinMode(rooms[i].pin, OUTPUT);
    }

  t.every(TEMP_READ_INTERVAL, check_temperatures);
  // Check if needs update
  //RTC_set();

    if(hh || mm || d || m || yOff){
        hh = 0;
        mm = 0;
        d = 0;
        m = 0;
        yOff = 0;
        RTC.adjust(DateTime(yOff, m, d, hh, mm, 0));
    }
}

void thermo_loop(){
    t.update();
}


