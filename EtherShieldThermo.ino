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
#define BUFFER_SIZE 500


//please modify the following two lines. mac and ip have to be unique
//in your local area network. You can not have the same numbers in
//two devices:
static uint8_t mymac[6] = {0x54,0x55,0x58,0x10,0x00,0x24};
static uint8_t myip[4] = {192,168,99,123};

static uint8_t buf[BUFFER_SIZE+1];
#define STR_BUFFER_SIZE 22
static char strbuf[STR_BUFFER_SIZE+1];


// Needed for prog_char PROGMEM
//#include <avr/pgmspace.h>

/** ****************************************************
*
* Main constants, all times in millis
*/

// Number of rooms
#define ROOMS 2

// Pins
#define ONE_WIRE_PIN 9
#define PUMP_PIN 1

#define ROOM_1_PIN 2
#define ROOM_2_PIN 3

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


Timer t;


/** *****************************************************
*
* RTC part
*
*/


RTC_DS1307 RTC;
DateTime now;

/******************************
 *
 * EEPROM
 *
 */


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
    {1, { 0x28, 0xAD, 0x4C, 0xC4, 0x03, 0x00, 0x00, 0x13}, ROOM_1_PIN, 3},
    {2, { 0x28, 0x6C, 0x41, 0xC4, 0x03, 0x00, 0x00, 0x57}, ROOM_2_PIN, 8}
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

//prepare the webpage by writing the data to the tcp send buffer
uint16_t print_homepage(uint8_t *buf);
int8_t analyse_cmd(char *str);

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
 * Get the numeric single digit parameter
 */
int8_t analyse_cmd(char *str){
  int8_t r=-1;
  if(find_key_val(str,"cmd")){
    if(*strbuf < 0x3a && *strbuf > 0x2f){
      //is a ASCII number, return it
      r=(*strbuf - 0x30);
    }
  }
  return r;
}

/**
 * Standard header
 */
uint16_t print_200ok(uint8_t *buf){
  uint16_t plen;
  plen=es.ES_fill_tcp_data_p(buf,0,PSTR("HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n"));
  return(plen);
}


/**
 * Main home page
 */
uint16_t print_homepage(uint8_t *buf){
  uint16_t plen;
  plen=print_200ok(buf);
  plen=es.ES_fill_tcp_data_p(buf,plen,PSTR("<html><head><script src=\"http://ajax.googleapis.com/ajax/libs/jquery/1.8.1/jquery.min.js\"></script><script src=\"//ajax.googleapis.com/ajax/libs/jqueryui/1.8.23/jquery-ui.min.js\"></script></head><body>"));
  plen=es.ES_fill_tcp_data_p(buf,plen,PSTR("<h1>Loading...</h1>"));
  plen=es.ES_fill_tcp_data_p(buf,plen,PSTR("</body></head></html>"));
  return(plen);
}

/**
 * Print status
 */
uint16_t print_status(uint8_t *buf){
    uint16_t plen;
    char buf2[32];

    plen=es.ES_fill_tcp_data_p(buf,0,PSTR("HTTP/1.0 200 OK\r\nContent-Type: text/json\r\n\r\n"));
    plen=es.ES_fill_tcp_data_p(buf,plen,PSTR("{"));
    for(int room=0; room<ROOMS; room++){

        itoa(room, buf2, 10);
        plen=es.ES_fill_tcp_data(buf,plen, buf2);
        plen=es.ES_fill_tcp_data_p(buf,plen, PSTR(":{t:"));

        dtostrf(rooms[room].temperature, 5, 2, buf2);
        plen=es.ES_fill_tcp_data(buf,plen, buf2);

        plen=es.ES_fill_tcp_data_p(buf,plen, PSTR(",T:"));
        dtostrf(get_desired_temperature(room), 5, 2, buf2);
        plen=es.ES_fill_tcp_data(buf,plen, buf2);

        itoa(rooms[room].program, buf2, 10);
        plen=es.ES_fill_tcp_data_p(buf,plen, PSTR(",p:"));
        plen=es.ES_fill_tcp_data(buf,plen, buf2);

        itoa(memoryFree(), buf2, 10);
        plen=es.ES_fill_tcp_data_p(buf,plen, PSTR(",f:"));
        plen=es.ES_fill_tcp_data(buf,plen, buf2);

        itoa(now.hour(), buf2, 10);
        plen=es.ES_fill_tcp_data_p(buf,plen, PSTR(",hh:"));
        plen=es.ES_fill_tcp_data(buf,plen, buf2);

        itoa(now.minute(), buf2, 10);
        plen=es.ES_fill_tcp_data_p(buf,plen, PSTR(",mm:"));
        plen=es.ES_fill_tcp_data(buf,plen, buf2);

        itoa(now.second(), buf2, 10);
        plen=es.ES_fill_tcp_data_p(buf,plen, PSTR(",ss:"));
        plen=es.ES_fill_tcp_data(buf,plen, buf2);


        plen=es.ES_fill_tcp_data_p(buf,plen, PSTR(",s:"));
        buf2[0] = rooms[room].status;
        buf2[1] = '\0';
        plen=es.ES_fill_tcp_data(buf,plen, buf2);

        plen=es.ES_fill_tcp_data_p(buf,plen, room != ROOMS - 1 ? PSTR("},") : PSTR("}"));
    }
    plen=es.ES_fill_tcp_data_p(buf,plen,PSTR("}"));
    return(plen);
}




/**
 * Main loop
 *
 */
void loop(){
    uint16_t plen, dat_p;
    int8_t cmd;
    plen = es.ES_enc28j60PacketReceive(BUFFER_SIZE, buf);
    /*plen will be unequal to zero if there is a valid packet (without crc error) */
    if(plen!=0){
        // arp is broadcast if unknown but a host may also verify the mac address by sending it to a unicast address.
        if(es.ES_eth_type_is_arp_and_my_ip(buf,plen)){
            es.ES_make_arp_answer_from_request(buf);
            return;
        }
        // check if ip packets are for us:
        if(es.ES_eth_type_is_ip_and_my_ip(buf,plen)==0){
            return;
        }
        if(buf[IP_PROTO_P]==IP_PROTO_ICMP_V && buf[ICMP_TYPE_P]==ICMP_TYPE_ECHOREQUEST_V){
            es.ES_make_echo_reply_from_request(buf,plen);
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
                    plen=es.ES_fill_tcp_data_p(buf,plen,PSTR("<h1>200 OK</h1>"));
                    goto SENDTCP;
                }
                if(strncmp("/ ",(char *)&(buf[dat_p+4]),2)==0){
                    plen=print_homepage(buf);
                    goto SENDTCP;
                }
                cmd=analyse_cmd((char *)&(buf[dat_p+5]));
                if(cmd==2){
                }
                else if (cmd==3){
                }
                plen=print_status(buf);
SENDTCP:        es.ES_make_tcp_ack_from_any(buf); // send ack for http get
                es.ES_make_tcp_ack_with_data(buf, plen); // send data
            }
        }
    }

    // Thermo
    thermo_loop();

}
