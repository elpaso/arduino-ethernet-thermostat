/**
*
* Thermostat
*
*/


#include "EtherShield.h"


#include "Timer.h"
#include <Wire.h>
#include "RTClib.h"
#include <EEPROM.h>
#include <OneWire.h>
#include <DallasTemperature.h>



#define HTTP_PORT 80
#define BUFFER_SIZE 550


//please modify the following two lines. mac and ip have to be unique
//in your local area network. You can not have the same numbers in
//two devices:
static uint8_t mymac[6] = {0x54,0x55,0x58,0x10,0x00,0x24};
static uint8_t myip[4] = {192,168,99,123};


static uint8_t buf[BUFFER_SIZE+1];
#define STR_BUFFER_SIZE 22
static char strbuf[STR_BUFFER_SIZE+1];

// Used here and there...
static char buf32[32];


// Needed for prog_char PROGMEM
//#include <avr/pgmspace.h>

/** ****************************************************
*
* Main constants, all times in millis
*/

// Number of rooms
#define ROOMS 5

#define WEEKLY_PROGRAM_NUMBER 10
#define DAILY_PROGRAM_NUMBER 8
#define SLOT_NUMBER 8

// Pins
#define ONE_WIRE_PIN 9
#define PUMP_PIN 1

#define ROOM_1_PIN 2
#define ROOM_2_PIN 3
#define ROOM_3_PIN 4
#define ROOM_4_PIN 5
#define ROOM_5_PIN 6

// TODO: move to config

#define TEMP_READ_INTERVAL 4000 // millis
#define VALVE_OPENING_TIME_S 120 // 2 minutes
#define BLOCKED_TIME_S 3600 // 1 hour
#define RISE_TEMP_TIME_S 600 // 10 minutes
#define RISE_TEMP_DELTA 50
#define HYSTERESIS 50
#define MAX_ALLOWED_T 2500 // in cents


// Room status
#define OPENING 'V' // valves are opening for VALVE_OPENING_TIME
#define CLOSED 'C' // Closed
#define OPEN 'O' // Open (main pump is also open)
#define BLOCKED 'B' // Blocked until BLOCKED_TIME is elapsed

// Error codes
#define ERR_NO 0
#define ERR_WRONG_COMMAND 1
#define ERR_WRONG_ROOM 2
#define ERR_WRONG_PROGRAM 3
#define ERR_WRONG_PARM 4 // Generic parameter error

// Commands
#define CMD_ROOM_SET_PGM 1
#define CMD_WRITE_EEPROM 2
#define CMD_TIME_SET 3
#define CMD_TEMPERATURE_SET 4
#define CMD_RESET 5
#define CMD_W_PGM_SET_D_PGM 6
#define CMD_D_PGM_SET_T_PGM 7
#define CMD_SLOT_SET_UPPER_BOUND 8

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
uint32_t unlock_time = 0;
byte this_weekday;
int last_error_code = ERR_NO;

// Global OFF
byte off = 0;
byte pump_open = 0;

// Temperatures
// TODO: configurable
uint16_t T[] = {500, 1500, 1800, 2800};

// Programs
// 8 slots    6:30  8:00 12:00 13:00 16:00 20:00 22:00
int slot[SLOT_NUMBER - 1] = { 390,  480,  720,  780,  960, 1200, 1320 };
// 6 programs, T level for each slot/pgm tuple
byte daily_program[DAILY_PROGRAM_NUMBER][SLOT_NUMBER] = {
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
byte weekly_program[WEEKLY_PROGRAM_NUMBER][7] = {
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
  DeviceAddress address;
  byte pin;
  byte program;
  char status;
  int temperature;
  int old_temperature;
  uint32_t last_status_change;
} rooms[ROOMS] = {
    {{ 0x28, 0xAD, 0x4C, 0xC4, 0x03, 0x00, 0x00, 0x13}, ROOM_1_PIN, 3},
    {{ 0x28, 0x6C, 0x41, 0xC4, 0x03, 0x00, 0x00, 0x57}, ROOM_2_PIN, 8},
    {{ 0x28, 0x6C, 0x41, 0xC4, 0x03, 0x00, 0x00, 0x37}, ROOM_3_PIN, 8},
    {{ 0x28, 0x6C, 0x41, 0xC4, 0x03, 0x00, 0x00, 0x27}, ROOM_4_PIN, 8},
    {{ 0x28, 0x6C, 0x41, 0xC4, 0x03, 0x00, 0x00, 0x67}, ROOM_5_PIN, 8}
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
    if(now.unixtime() > unlock_time){
        unlock_time = 0;
    }
    for(int i=0; i<ROOMS; i++){
        if(!unlock_time){
            // Get temperature
            float tempC = sensors.getTempC(rooms[i].address);
            if (tempC != -127.00) {
                rooms[i].temperature = (int)(tempC * 100);
            }
            char new_status = rooms[i].status;
            needs_heating = (new_status == OPENING);
            if(! needs_heating){
                float t = get_desired_temperature(i);
                needs_heating = rooms[i].temperature < (new_status == OPEN ? t + HYSTERESIS : t - HYSTERESIS);
            }
            if(!needs_heating){
                new_status = CLOSED;
            } else {
                switch(rooms[i].status){
                    case OPENING:
                        if(VALVE_OPENING_TIME_S < now.unixtime() - rooms[i].last_status_change){
                            new_status = OPEN;
                            rooms[i].old_temperature = rooms[i].temperature;
                        }
                        break;
                    case OPEN:
                        if(RISE_TEMP_TIME_S <  now.unixtime() - rooms[i].last_status_change){
                            if(rooms[i].temperature - rooms[i].old_temperature < RISE_TEMP_DELTA){
                                new_status = BLOCKED;
                                unlock_time = now.unixtime() + BLOCKED_TIME_S;
                            } else {
                                rooms[i].last_status_change = now.unixtime();
                                rooms[i].old_temperature = rooms[i].temperature;
                                pump_open = 1;
                            }
                        } else {
                            pump_open = 1;
                        }
                        break;
                    case BLOCKED: // Should only trigger while not blocked
                        new_status = CLOSED;
                        break;
                    default:
                    case CLOSED:
                        new_status = OPENING;
                }
            }
            if(new_status != rooms[i].status){
                rooms[i].last_status_change = now.unixtime();
                rooms[i].status = new_status;
            }
            digitalWrite(rooms[i].pin, (new_status == OPENING) || (new_status == OPEN));
        }
    }
    digitalWrite(PUMP_PIN, pump_open && !unlock_time);
}




/**
 * Set up
 *
 */
void thermo_setup(){

    Wire.begin();
    RTC.begin();
    if (! RTC.isrunning()) {
        // following line sets the RTC to the date & time this sketch was compiled
        RTC.adjust(DateTime(__DATE__, __TIME__));
    }


    pinMode(PUMP_PIN, OUTPUT);

    // Sensors
    // set the resolution to 12 bit (maximum)
    sensors.begin();
    for (int i=0; i<ROOMS; i++){
        sensors.setResolution(rooms[i].address, 12);
        pinMode(rooms[i].pin, OUTPUT);
    }

  t.every(TEMP_READ_INTERVAL, check_temperatures);

}

void thermo_loop(){
    t.update();
}


/** ********************************************************
 *
 *  Ethernet part
 *
 */

EtherShield es=EtherShield();


//---Predisposizione--------------------------------------------------------
void setup(){
  /*initialize enc28j60*/
  es.ES_enc28j60Init(mymac);
  es.ES_enc28j60clkout(2); // change clkout from 6.25MHz to 12.5MHz
  delay(10);
  /*Magjack leds configuration, see enc28j60 datasheet, page 11 */
  //0x880 is PHLCON LEDB=on, LEDA=on
  //enc28j60PhyWrite(PHLCON,0b0000 1000 1000 00 00);
  es.ES_enc28j60PhyWrite(PHLCON,0x880);
  delay(500);
  //0x990 is PHLCON LEDB=off, LEDA=off
  //enc28j60PhyWrite(PHLCON,0b0000 1001 1001 00 00);
  es.ES_enc28j60PhyWrite(PHLCON,0x990);
  delay(500);
  //0x880 is PHLCON LEDB=on, LEDA=on
  //enc28j60PhyWrite(PHLCON,0b0000 1000 1000 00 00);
  es.ES_enc28j60PhyWrite(PHLCON,0x880);
  delay(500);
  //0x990 is PHLCON LEDB=off, LEDA=off
  //enc28j60PhyWrite(PHLCON,0b0000 1001 1001 00 00);
  es.ES_enc28j60PhyWrite(PHLCON,0x990);
  delay(500);
  //0x476 is PHLCON LEDA=links status, LEDB=receive/transmit
  //enc28j60PhyWrite(PHLCON,0b0000 0100 0111 01 10);
  es.ES_enc28j60PhyWrite(PHLCON,0x476);
  delay(100);
  //init the ethernet/ip layer:
  es.ES_init_ip_arp_udp_tcp(mymac,myip,80);

  // Thermo
  thermo_setup();


}

// variables created by the build process when compiling the sketch
extern int __bss_end;
extern void *__brkval;
// function to return the amount of free RAM
int memoryFree()  {
    int freeValue;
    if((int)__brkval == 0)
        freeValue = ((int)&freeValue) - ((int)&__bss_end);
    else
        freeValue = ((int)&freeValue) - ((int)__brkval);
    return freeValue;
}



/**
 * Find the value for a given key
 *
 * The returned value is stored in the global var strbuf
 */
uint8_t find_key_val(char *str,char *key){
  uint8_t found=0;
  uint8_t i=0;
  char *kp;
  kp=key;
  while(*str &&  *str!=' ' && found==0){
    if(*str == *kp){
      kp++;
      if(*kp == '\0'){
        str++;
        kp=key;
        if(*str == '='){
          found=1;
        }
      }
    }
    else{
      kp=key;
    }
    str++;
  }
  if(found==1){
    //copy the value to a buffer and terminate it with '\0'
    while(*str &&  *str!=' ' && *str!='&' && i<STR_BUFFER_SIZE){
      strbuf[i]=*str;
      i++;
      str++;
    }
    strbuf[i]='\0';
  }
  return(found);
}





/**
 * Get the numeric positive int parameter
 */
int analyse_cmd(char *str, char *key){
  int r=-1;
  char *buf_p;
  if(find_key_val(str, key)){
    buf_p = strbuf;
    while(0x2f < *buf_p && *buf_p < 0x3a){
      //is a ASCII number, return it
      r = (r >= 0)  ? r * 10 + (*buf_p - 0x30) : (*buf_p - 0x30);
      buf_p++;
    }
  }
  return r;
}



/**
 * Standard header
 */
uint16_t print_200ok(uint8_t *buf){
  uint16_t plen;
  plen=es.ES_fill_tcp_data_p(buf,0,PSTR("HTTP/1.0 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n"));
  return(plen);
}


/**
 * Main home page
 */
uint16_t print_homepage(uint8_t *buf){
  uint16_t plen;
  plen=print_200ok(buf);
  plen=es.ES_fill_tcp_data_p(buf, plen,PSTR("<!DOCTYPE html><html><head></head><body>"));
  plen=es.ES_fill_tcp_data_p(buf, plen,PSTR("<h1>EtherShieldThermo</h1>"));
  plen=es.ES_fill_tcp_data_p(buf, plen,PSTR("<script src=\"http://localhost/~ale/thermoduino/loader.js\"></script>"));
  plen=es.ES_fill_tcp_data_p(buf, plen,PSTR("</body></head></html>"));
  return(plen);
}

/**
 * Int to float to string
 */
void decimal_string(int num, char* _buf){
    char buf3[3];
    itoa(num/100, _buf, 10);
    strcat(_buf, ".");
    itoa(num%100, buf3, 10);
    strcat(_buf, buf3);
}

/**
 * Json helpers
 */
uint16_t bracket_open(uint16_t plen){
    return es.ES_fill_tcp_data_p(buf, plen, PSTR("["));
}
uint16_t bracket_close(uint16_t plen){
    return es.ES_fill_tcp_data_p(buf, plen, PSTR("]"));
}

uint16_t json_array_wrap(uint16_t p[], int length, uint8_t* _buf, uint16_t plen){
    plen=bracket_open(plen);
    for(int i = 0; i < length; i++){
        decimal_string(p[i],  buf32);
        plen=es.ES_fill_tcp_data(buf, plen, buf32);
        plen=es.ES_fill_tcp_data_p(buf, plen, PSTR(","));
    }
    plen=bracket_close(plen-1);
    return plen;
}

uint16_t json_array_wrap(byte p[], int length, uint8_t* _buf, uint16_t plen){
    plen=bracket_open(plen);
    for(int i = 0; i < length; i++){
        itoa((int)p[i], buf32, 10);
        plen=es.ES_fill_tcp_data(buf, plen, buf32);
        plen=es.ES_fill_tcp_data_p(buf, plen, PSTR(","));
    }
    plen=bracket_close(plen-1);
    return plen;
}

uint16_t json_array_wrap(int p[], int length, uint8_t* _buf, uint16_t plen){
    plen=bracket_open(plen);
    for(int i = 0; i < length; i++){
        itoa(p[i], buf32, 10);
        plen=es.ES_fill_tcp_data(buf, plen, buf32);
        plen=es.ES_fill_tcp_data_p(buf, plen, PSTR(","));
    }
    plen=bracket_close(plen-1);
    return plen;
}


/**
 * Print status
 */
uint16_t print_json_response(uint8_t *buf, byte print_programs){
    uint16_t plen;

    // Update
    now = RTC.now();

    plen=es.ES_fill_tcp_data_p(buf,0,PSTR("HTTP/1.0 200 OK\r\nContent-Type: application/json; charset=UTF-8\r\n\r\n"));

    plen=es.ES_fill_tcp_data_p(buf, plen, pump_open ? PSTR("{\"P\":1,\"u\":") : PSTR("{\"P\":0,\"u\":"));
    ultoa(now.unixtime(), buf32, 10);
    plen=es.ES_fill_tcp_data(buf, plen, buf32);

    plen=es.ES_fill_tcp_data_p(buf, plen, PSTR(",\"b\":"));
    ultoa(unlock_time , buf32, 10);
    plen=es.ES_fill_tcp_data(buf, plen, buf32);

    plen=es.ES_fill_tcp_data_p(buf, plen, PSTR(",\"E\":"));
    itoa(last_error_code, buf32, 10);
    plen=es.ES_fill_tcp_data(buf, plen, buf32);

    plen=es.ES_fill_tcp_data_p(buf, plen, PSTR(",\"f\":"));
    itoa(memoryFree(), buf32, 10);
    plen=es.ES_fill_tcp_data(buf, plen, buf32);

    if(print_programs){

        plen=es.ES_fill_tcp_data_p(buf, plen, PSTR(",\"T\":"));
        plen=json_array_wrap(T, 4, buf, plen);

        plen=es.ES_fill_tcp_data_p(buf, plen, PSTR(",\"s\":"));
        plen=json_array_wrap(slot, SLOT_NUMBER - 1, buf, plen);

        plen=es.ES_fill_tcp_data_p(buf, plen, PSTR(",\"w\":["));

        for(int i=0; i<WEEKLY_PROGRAM_NUMBER; i++){
            plen=json_array_wrap(weekly_program[i], 7, buf, plen);
            plen=es.ES_fill_tcp_data_p(buf, plen, PSTR(","));
        }
        plen--;
        plen=es.ES_fill_tcp_data_p(buf, plen, PSTR("],\"d\":["));
        for(int i=0; i<DAILY_PROGRAM_NUMBER; i++){
            plen=json_array_wrap(daily_program[i], SLOT_NUMBER, buf, plen);
            plen=es.ES_fill_tcp_data_p(buf, plen, PSTR(","));
        }
        plen--;
        plen=es.ES_fill_tcp_data_p(buf, plen,PSTR("]}"));

    } else {
        plen=es.ES_fill_tcp_data_p(buf, plen,PSTR(",\"R\":["));
        for(int room=0; room<ROOMS; room++){

            plen=es.ES_fill_tcp_data_p(buf, plen, PSTR("{\"t\":"));

            decimal_string(rooms[room].temperature, buf32);
            plen=es.ES_fill_tcp_data(buf, plen, buf32);

            plen=es.ES_fill_tcp_data_p(buf, plen, PSTR(",\"T\":"));
            decimal_string(get_desired_temperature(room), buf32);
            plen=es.ES_fill_tcp_data(buf, plen, buf32);

            itoa(rooms[room].program, buf32, 10);
            plen=es.ES_fill_tcp_data_p(buf, plen, PSTR(",\"p\":"));
            plen=es.ES_fill_tcp_data(buf, plen, buf32);

            itoa(weekly_program[rooms[room].program][this_weekday], buf32, 10);
            plen=es.ES_fill_tcp_data_p(buf, plen, PSTR(",\"d\":"));
            plen=es.ES_fill_tcp_data(buf, plen, buf32);

            ultoa(rooms[room].last_status_change, buf32, 10);
            plen=es.ES_fill_tcp_data_p(buf, plen, PSTR(",\"l\":"));
            plen=es.ES_fill_tcp_data(buf, plen, buf32);


            plen=es.ES_fill_tcp_data_p(buf, plen, PSTR(",\"s\":"));
            buf32[0] = '"';
            buf32[1] = rooms[room].status;
            buf32[2] = '"';
            buf32[3] = '\0';
            plen=es.ES_fill_tcp_data(buf, plen, buf32);

            plen=es.ES_fill_tcp_data_p(buf, plen, room != ROOMS - 1 ? PSTR("},") : PSTR("}"));
        }
        plen=es.ES_fill_tcp_data_p(buf, plen,PSTR("]}"));
    }
    return(plen);
}

byte in_range(int num, int low, int high){
    return low <= num && num <= high;
}

/**
 * Main loop
 *
 */
void loop(){
    uint16_t plen, dat_p;
    int cmd, parm1, parm2, parm3;
    plen = es.ES_enc28j60PacketReceive(BUFFER_SIZE, buf);
    /*plen will be unequal to zero if there is a valid packet (without crc error) */
    if(plen!=0){
        // arp is broadcast if unknown but a host may also verify the mac address by sending it to a unicast address.
        if(es.ES_eth_type_is_arp_and_my_ip(buf, plen)){
            es.ES_make_arp_answer_from_request(buf);
            return;
        }
        // check if ip packets are for us:
        if(es.ES_eth_type_is_ip_and_my_ip(buf, plen)==0){
            return;
        }
        if(buf[IP_PROTO_P]==IP_PROTO_ICMP_V && buf[ICMP_TYPE_P]==ICMP_TYPE_ECHOREQUEST_V){
            es.ES_make_echo_reply_from_request(buf, plen);
            return;
        }
        // tcp port www start, compare only the lower byte
        if(buf[IP_PROTO_P] == IP_PROTO_TCP_V && buf[TCP_DST_PORT_H_P] == 0 && buf[TCP_DST_PORT_L_P] == HTTP_PORT){
            if(buf[TCP_FLAGS_P] & TCP_FLAGS_SYN_V){
                es.ES_make_tcp_synack_from_syn(buf); // make_tcp_synack_from_syn does already send the syn,ack
                return;
            }
            if(buf[TCP_FLAGS_P] & TCP_FLAGS_ACK_V){
                es.ES_init_len_info(buf); // init some data structures
                dat_p=es.ES_get_tcp_data_pointer();
                if(dat_p == 0){ // we can possibly have no data, just ack:
                    if(buf[TCP_FLAGS_P] & TCP_FLAGS_FIN_V){
                        es.ES_make_tcp_ack_from_any(buf);
                    }
                    return;
                }
                if(strncmp("GET ",(char *)&(buf[dat_p]),4)!=0){
                    // head, post and other methods for possible status codes see:
                    // http://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html
                    plen=print_200ok(buf);
                    plen=es.ES_fill_tcp_data_p(buf, plen,PSTR("<h1>200 OK</h1>"));
                    goto SENDTCP;
                }
                if(strncmp("/ ",(char *)&(buf[dat_p+4]),2)==0){
                    plen=print_homepage(buf);
                    goto SENDTCP;
                }
                if(strncmp("/st",(char *)&(buf[dat_p+4]),3)==0){
                    plen=print_json_response(buf, 0);
                    goto SENDTCP;
                }
                if(strncmp("/pr",(char *)&(buf[dat_p+4]),3)==0){
                    plen=print_json_response(buf, 1);
                    goto SENDTCP;
                }
                cmd=analyse_cmd((char *)&(buf[dat_p+5]), "c");
                if(cmd > 0){
                    // Switch?
                    switch(cmd){
                        case CMD_ROOM_SET_PGM:
                            parm1 = analyse_cmd((char *)&(buf[dat_p+5]), "p");
                            if(in_range(parm1, 0, ROOMS - 1)){
                                parm2 = analyse_cmd((char *)&(buf[dat_p+5]), "v");
                                if(in_range(parm2, 0, WEEKLY_PROGRAM_NUMBER - 1 )){
                                    rooms[parm1].program = parm2;
                                } else {
                                    last_error_code = ERR_WRONG_PROGRAM;
                                }
                            } else {
                                last_error_code = ERR_WRONG_ROOM;
                            }
                        break;
                        case CMD_WRITE_EEPROM:
                            // Write to EEPROM
                        break;
                        case CMD_W_PGM_SET_D_PGM:
                            parm1 = analyse_cmd((char *)&(buf[dat_p+5]), "p");
                            if(in_range(parm1, 0, WEEKLY_PROGRAM_NUMBER - 1)){
                                parm2 = analyse_cmd((char *)&(buf[dat_p+5]), "v"); // dayOfWeek
                                if(in_range(parm2, 0, 6)){
                                    parm3 = analyse_cmd((char *)&(buf[dat_p+5]), "v");
                                    if(in_range(parm3, 0, DAILY_PROGRAM_NUMBER - 1)){
                                        weekly_program[parm1][parm2] = parm3;
                                    } else {
                                        last_error_code = ERR_WRONG_PARM;
                                    }
                                } else {
                                    last_error_code = ERR_WRONG_PARM;
                                }
                            } else {
                                last_error_code = ERR_WRONG_PARM;
                            }
                        break;
                        case CMD_D_PGM_SET_T_PGM:
                            // Write to EEPROM
                        break;
                        case CMD_SLOT_SET_UPPER_BOUND:
                            // Write to EEPROM
                        break;
                        case CMD_TEMPERATURE_SET:
                            // Set T1, T2 and T3
                            parm1 = analyse_cmd((char *)&(buf[dat_p+5]), "p");
                            if(in_range(parm2, 1, 3)){
                                last_error_code = ERR_WRONG_PARM;
                            } else {
                                parm2 = analyse_cmd((char *)&(buf[dat_p+5]), "v");
                                switch(parm1){
                                    case 1:
                                        if(in_range(parm2, T[0] + 50, T[2] - 50)){
                                            T[1] = parm2;
                                        } else {
                                            last_error_code = ERR_WRONG_PARM;
                                        }
                                    break;
                                    case 2:
                                        if(in_range(parm2, T[1] + 50,  T[3] - 50)){
                                            T[2] = parm2;
                                        } else {
                                            last_error_code = ERR_WRONG_PARM;
                                        }
                                    break;
                                    case 3:
                                        if(in_range(parm2, T[2] + 50, MAX_ALLOWED_T )){
                                            T[3] = parm2;
                                        } else {
                                            last_error_code = ERR_WRONG_PARM;
                                        }
                                    break;
                                }
                            }
                        break;
                        case CMD_TIME_SET:
                            parm1 = analyse_cmd((char *)&(buf[dat_p+5]), "p");
                            if(parm1 < 0 || parm1 > 5){ // 0 = hh, 1 = mm, 2 = ss, 3 = Y, 4 = m, 5 = d
                                last_error_code = ERR_WRONG_PARM;
                            } else {
                                now = RTC.now();
                                parm2 = analyse_cmd((char *)&(buf[dat_p+5]), "v");
                                switch(parm1){
                                    case 0:
                                        RTC.adjust(DateTime(now.year(), now.month(), now.day(), parm2, now.minute(), now.second()));
                                    break;
                                    case 1:
                                        RTC.adjust(DateTime(now.year(), now.month(), now.day(), now.hour(), parm2, now.second()));
                                    break;
                                    case 2:
                                        RTC.adjust(DateTime(now.year(), now.month(), now.day(), now.hour(), now.minute(), parm2));
                                    break;
                                    case 3:
                                        RTC.adjust(DateTime(parm2, now.month(), now.day(), now.hour(), now.minute(), now.second()));
                                    break;
                                    case 4:
                                        RTC.adjust(DateTime(now.year(), parm2, now.day(), now.hour(), now.minute(), now.second()));
                                    break;
                                    case 5:
                                        RTC.adjust(DateTime(now.year(), now.month(), parm2, now.hour(), now.minute(), now.second()));
                                    break;
                                    default:
                                        last_error_code = ERR_WRONG_PARM;
                                }
                            }
                        break;
                        default:
                            last_error_code = ERR_WRONG_COMMAND;

                    }
                }
                plen=print_json_response(buf, 0);
                last_error_code = ERR_NO;
SENDTCP:        es.ES_make_tcp_ack_from_any(buf); // send ack for http get
                es.ES_make_tcp_ack_with_data(buf, plen); // send data
            }
        }
    }

    // Thermo
    thermo_loop();

}
