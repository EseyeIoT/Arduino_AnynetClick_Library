/* Define NANO if building for ATmega328 (use for promini/uno etc.) */
/* Define PROMICRO if building for ATmega32u4 */

/* This demo uses softwareserial on the 328 (pins 2 and 3) and Serial1 on the 32u4 so Serial is available for debugging */
/* Support is also included for reading a DHT22 on pin 4 and publishing the temp/hum data */

#define NANO
//#define PROMICRO

/* Define the serial port to be used to talk to the modem */

#ifdef NANO
/* For SoftwareSerial use this */
#define RX_PIN 2 //5
#define TX_PIN 3 //4

#include <SoftwareSerial.h>
SoftwareSerial mySerial(RX_PIN, TX_PIN); // RX, TX

/* Alternative NeoSWSerial */
//#include <NeoSWSerial.h>
//NeoSWSerial mySerial( RX_PIN, TX_PIN);

#define ATSerial mySerial

#endif

#ifdef PROMICRO
/* To use hardware serial (Serial1) */
#define ATSerial Serial1
#endif

#ifndef ATSerial
#error need to define platform!
#endif

#include "eseyeaws.h"

eseyeAWS myAWS(&ATSerial);

#define WITH_DHT

#ifdef WITH_DHT
#include <DHT.h>

#define DHT22_PIN 4
DHT dht(DHT22_PIN, DHT22);
float lastTemp = 0.0;
float lastHum = 0.0;
#endif

unsigned long lasttime;
int testpub = -1;
int testsub = -1;
int testpub2 = -1;
int testsub2 = -1;

/* 10 minute delay */
unsigned long pausebetween = 60000;

#define STATE_TEST_SUBOPEN     0
#define STATE_TEST_SUBOPEN2    1
#define STATE_TEST_PUBOPEN     2
#define STATE_TEST_PUBOPEN2    3
#define STATE_TEST_PUBLISH     4
#define STATE_TEST_PUBLISH2    5
#define STATE_TEST_PUBCLOSE    6
#define STATE_TEST_PUBCLOSE2   7
#define STATE_TEST_SUBCLOSE    8
#define STATE_TEST_SUBCLOSE2   9
#define STATE_SENDBUTTONSINGLE 10
#define STATE_GETICCID         11
#define STATE_WAITICCID        12
#define STATE_GETAWSVER        13
#define STATE_WAITAWSVER       14
#define STATE_COMPLETE         15
#define STATE_DONE             16
#define STATE_NONE             100
static int teststate = STATE_NONE;

void urccb(char *atresp){
  Serial.print("Got AT response data: ");
  Serial.println(atresp);
  if(teststate == STATE_WAITICCID && strstr(atresp, "QCCID") != NULL)
      teststate = STATE_GETAWSVER;
  if(teststate == STATE_WAITAWSVER && strstr(atresp, "AWSVER") != NULL)
      teststate = STATE_COMPLETE;
}

void testcb(uint8_t *data, uint8_t length){
  Serial.print("Test got data ");
  Serial.println((char *)data);
}

void mysubcb(uint8_t *data, uint8_t length){
  Serial.print("Mysub got data ");
  Serial.println((char *)data);    
}

void setup() {
  Serial.begin(115200);

  ATSerial.begin(9600);

  myAWS.init(urccb, &Serial);

  delay(2000);

  lasttime = millis();

  myAWS.pubunreg(0);

  /* Comment this out to prevent auto-start */
  teststate = STATE_TEST_SUBOPEN;
}

#define UART_RX_BUFSIZE 100
static uint8_t uartrxbuf[UART_RX_BUFSIZE + 1];
static unsigned char uartrxbufidx = 0;
void checkcmd(){
  char nextchar;
  while (Serial.available() > 0) {
    nextchar = Serial.read();

    //Serial.print(nextchar);

    uartrxbuf[uartrxbufidx++] = nextchar;
    uartrxbuf[uartrxbufidx] = 0;

    if(nextchar == '\n'){
      Serial.print("Got ");
      Serial.println((char *)uartrxbuf);
      if(strncmp(uartrxbuf, "test", 4) == 0){
        teststate = STATE_TEST_SUBOPEN;
      }
      if(strncmp(uartrxbuf, "stop", 4) == 0){
        teststate = STATE_TEST_PUBCLOSE;
      }
      if(strncmp(uartrxbuf, "clear", 5) == 0){
        Serial.println("Resetting mqtt contexts");
          myAWS.init();
      }
      if(strncmp(uartrxbuf, "time", 4) == 0){
        Serial.print("Uptime is ");
        Serial.println(millis());
      }
      if(strncmp(uartrxbuf, "pause", 5) == 0){
        unsigned long newpause = strtol(&uartrxbuf[6], NULL, 10);
        pausebetween = newpause * 1000;
        Serial.print("Delay is now ");
        Serial.println(pausebetween);
      }
      if(strncmp(uartrxbuf, "send", 4) == 0){
        uartrxbuf[uartrxbufidx - 1] = 0;
        Serial.print("Sending ");
        Serial.print((char *)&uartrxbuf[5]);
        Serial.println(" to modem");
        ATSerial.print((char *)&uartrxbuf[5]);
        ATSerial.print("\r\n");
      }
      uartrxbufidx = 0;
    }

    if(uartrxbufidx >= UART_RX_BUFSIZE)
      uartrxbufidx = 0;
  }
}

void loop() {
  unsigned long timesince = millis() - lasttime;

  checkcmd();
  
  myAWS.poll();

  switch(teststate){
    case STATE_TEST_SUBOPEN:
      if(timesince > 100){
        testsub = myAWS.subscribe((char *)"test", testcb);
        Serial.print("Subscribing on ");
        Serial.println(testsub);
        lasttime = millis();
        teststate = STATE_TEST_SUBOPEN2;
      }
    break;
    case STATE_TEST_SUBOPEN2:
      if(timesince > 100){
        testsub2 = myAWS.subscribe((char *)"mysub", mysubcb);
        Serial.print("Subscribing on ");
        Serial.println(testsub2);
        lasttime = millis();
        teststate = STATE_TEST_PUBOPEN;
      }
    break;
    case STATE_TEST_PUBOPEN:
      if(timesince > 100){
        testpub = myAWS.pubreg((char *)"test");
        Serial.print("Pubreg on ");
        Serial.println(testpub);
        lasttime = millis();
        teststate = STATE_TEST_PUBOPEN2;
      }
    break;
    case STATE_TEST_PUBOPEN2:
      if(timesince > 100){
        testpub2 = myAWS.pubreg((char *)"mysub");
        Serial.print("Pubreg on ");
        Serial.println(testpub2);
        lasttime = millis();
        teststate = STATE_TEST_PUBLISH;
      }
    break;
    case STATE_TEST_PUBLISH:
      if(timesince > 1000 && testpub != -1 && testsub != -1 && myAWS.pubstate(testpub) == PUB_TOPIC_REGISTERED && myAWS.substate(testsub) == SUB_TOPIC_SUBSCRIBED && myAWS.pubdone() == true){
        Serial.print("Publish hello on ");
        Serial.println(testpub);
        myAWS.publish(testpub, (uint8_t *)"{\"msg\": \"hello\"}", 16);
        lasttime = millis();
        teststate = STATE_TEST_PUBLISH2;
      }else if(testpub == -1 || testsub == -1 || myAWS.pubstate(testpub) == PUB_TOPIC_ERROR || myAWS.substate(testsub) == SUB_TOPIC_ERROR){
        Serial.println("Error state TEST_PUBLISH");
        lasttime = millis();
        teststate = STATE_TEST_PUBLISH2;
      }
      break;
    case STATE_TEST_PUBLISH2:
      if(timesince > 10 && testpub2 != -1 && testsub2 != -1 && myAWS.pubstate(testpub2) == PUB_TOPIC_REGISTERED && myAWS.substate(testsub2) == SUB_TOPIC_SUBSCRIBED && myAWS.pubdone() == true){
#ifdef WITH_DHT
        char report[50];
        int repidx = 0;
        uint8_t tempi, humi;
        uint8_t tempfrac, humfrac;
        lastHum = dht.readHumidity();
        lastTemp = dht.readTemperature();
        if (!isnan(lastHum) && !isnan(lastTemp)){
            tempi = (int)lastTemp;
            tempfrac = (uint8_t)((lastTemp - (float)tempi) * 100);
            humi = (int)lastHum;
            humfrac = (uint8_t)((lastHum - (float)humi) * 100);
            repidx += sprintf(&report[repidx], "{\"Humidity\": \"%d.%d\", \"Temp\": \"%d.%d\"}", humi, humfrac, tempi, tempfrac); 
            Serial.print("Report ");
            Serial.println(report);
            myAWS.publish(testpub2, report, repidx); 
        }
#else        
        Serial.print("Publish testsub on ");
        Serial.println(testpub2);
        myAWS.publish(testpub2, (uint8_t *)"{\"msg\": \"testsub\"}", 18);
#endif
        lasttime = millis();
        teststate = STATE_TEST_PUBCLOSE;
      }else if(testpub2 == -1 || testsub2 == -1 || myAWS.pubstate(testpub2) == PUB_TOPIC_ERROR || myAWS.substate(testsub2) == SUB_TOPIC_ERROR){
        Serial.println("Error state TEST_PUBLISH2");
        lasttime = millis();
        teststate = STATE_TEST_PUBCLOSE;
      }
      break;
    case STATE_TEST_PUBCLOSE:
      if(timesince > 10000){
        if(testpub != -1){
          myAWS.pubunreg(testpub);
          Serial.print("Pubunreg ");
          Serial.println(testpub);
        }
        lasttime = millis();
        teststate = STATE_TEST_PUBCLOSE2;
      }
      break;
    case STATE_TEST_PUBCLOSE2:
      if(timesince > 100){
        if(testpub2 != -1){
          myAWS.pubunreg(testpub2);
          Serial.print("Pubunreg ");
          Serial.println(testpub2);
        }
        lasttime = millis();
        teststate = STATE_TEST_SUBCLOSE;
      }
      break;
    case STATE_TEST_SUBCLOSE:
      if(timesince > 100){
        if(testsub != -1){
          myAWS.unsubscribe(testsub);
          Serial.print("Unsubscribe ");
          Serial.println(testsub);
        }
        lasttime = millis();
        teststate = STATE_TEST_SUBCLOSE2;   
      }
      break;
    case STATE_TEST_SUBCLOSE2:
      if(timesince > 100){
        if(testsub2 != -1){
          myAWS.unsubscribe(testsub2);
          Serial.print("Unsubscribe ");
          Serial.println(testsub2);
        }
        lasttime = millis();
        teststate = STATE_SENDBUTTONSINGLE;   
      }
      break;
    case STATE_SENDBUTTONSINGLE:
      if(timesince > 1000){
        lasttime = millis();
        myAWS.sendAT("at+awsbutton=single\r\n");
        teststate = STATE_GETICCID;
      }
      break;
    case STATE_GETICCID:
      if(timesince > 2000){
        lasttime = millis();
        myAWS.sendAT("at+qccid\r\n");
        teststate = STATE_WAITICCID;
      }
      break;
    case STATE_WAITICCID:  
      if(timesince > 2000){
        lasttime = millis();
        Serial.println("Failed to get ICCID");
        teststate = STATE_GETAWSVER;
      }
      break;
    case STATE_GETAWSVER:
        lasttime = millis();
        myAWS.sendAT("at+awsver\r\n");
        teststate = STATE_WAITAWSVER;
      break;
    case STATE_WAITAWSVER:  
      if(timesince > 2000){
        lasttime = millis();
        Serial.println("Failed to get AWSVER");
        teststate = STATE_COMPLETE;
      }
      break;
    case STATE_COMPLETE:
        Serial.print(millis());
        Serial.print(": Wait for ");
        Serial.print(pausebetween);
        Serial.println(" mS");
        teststate = STATE_DONE;
        break;
    case STATE_DONE:
      if(timesince > pausebetween){
        lasttime = millis();
        teststate = STATE_TEST_SUBOPEN;
        //teststate = STATE_NONE;
      }
      break;
    case STATE_NONE:  
      lasttime = millis();
      break;
  }
  
}

