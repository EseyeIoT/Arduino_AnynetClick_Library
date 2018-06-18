
/***************************************************************************
  eseyeaws library (v0.8)
  
  This library provides simplified access functions for the AT command 
  interface of AWS anynet-secure enabled modem-modules.
  
  The anynet-secure click AT interface is quite simple but this library
  tries to make it simpler still to use - specifically for subscribed
  topics by simply registering for topic subscriptions and supplying
  a callback function for received messages.
  
  Error-detection in pub/sub requests can also be enabled so the application
  can detect problems with its use of pub/sub indices.
  
  by Paul Tupper (01/2018) <paul.tupper@dataflex.com>
  
 ***************************************************************************/

/* TODO:
 * store AT strings in PROGMEM to save ram.
 * profile variable usage to reduce RAM requirements
 * complete implementation of sleep support for battery-powered applications.
 */

#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>

#include <SoftwareSerial.h>
#include "eseyeaws.h"

#ifdef DEBUG_ESEYEAWS
#define UARTDEBUG(x)   if(this->dbguart != NULL){ this->dbguart->print(x); }
#define UARTDEBUGLN(x) if(this->dbguart != NULL){ this->dbguart->println(x); }
#else
#define UARTDEBUG(x)
#define UARTDEBUGLN(x)
#endif

#ifndef ATSerial
#define ATSerial Serial
#endif

const char aws_start[]   = "AT+AWS";
const char aws_sub[]     = "SUBOPEN=";
const char aws_subcl[]   = "SUBCLOSE=";
const char aws_pub[]     = "PUBOPEN=";
const char aws_pubcl[]   = "PUBCLOSE=";
const char aws_publish[] = "PUBLISH=";
const char aws_sendok[]  = "SEND OK";
const char aws_sendfail[] = "SEND FAIL";

const char at_start[]     = "AT";
const char aws_urc[]      = "+AWS";
const char pub_start[]    = "PUB";
const char sub_start[]    = "SUB";
const char open_msg[]     = "OPEN";
const char close_msg[]    = "CLOSE";
#ifdef FILTER_OK
const char ok_msg[]       = "OK";
const char error_msg[]    = "ERROR";
const char crlf_msg[]     = "\r\n";
#endif

#ifdef FILTER_OK
void eseyeAWS::incOKreq(){
    this->outstanding_ok++;
}
#endif

/* Subscribe topic API */

#define TOPIC_NOT_SUBSCRIBED 0
#define TOPIC_SUBSCRIBING    1
#define TOPIC_SUBSCRIBED     2

/* Subscribe to a topic using the first available topic index */
int eseyeAWS::subscribe(char *topic, _msgcb callback){
  int topiccount = 0;
#ifdef TIMEOUT_RESPONSES
  this->checkTimeout();
#endif
  while(this->subtopics[topiccount].substate != SUB_TOPIC_NOT_IN_USE && this->subtopics[topiccount].substate != SUB_TOPIC_ERROR && topiccount < MAX_SUB_TOPICS){
    topiccount++;
  }
  if(topiccount == MAX_SUB_TOPICS)
    return -1;
  this->atuart->write(aws_start);
  this->atuart->write(aws_sub);
  this->atuart->print(topiccount);
  this->atuart->write(",\"");
  this->atuart->write(topic);
  this->atuart->write("\"\r\n");
#ifdef FILTER_OK
  this->incOKreq();
#endif
  this->subtopics[topiccount].substate = SUB_TOPIC_SUBSCRIBING;
  this->subtopics[topiccount].messagecb = callback;
#ifdef TIMEOUT_RESPONSES
  this->subtopics[topiccount].senttime = millis();
#endif
  return topiccount;
}

/* Have we successfully subscribed */
tsubTopicState eseyeAWS::substate(int idx){
#ifdef TIMEOUT_RESPONSES
  this->checkTimeout();
#endif
  return this->subtopics[idx].substate;
}

/* Unsubscribe from a topic index */
int eseyeAWS::unsubscribe(int idx){
#ifdef TIMEOUT_RESPONSES
  this->checkTimeout();
#endif
  if(idx >= MAX_SUB_TOPICS)
    return -1;
  if(this->subtopics[idx].substate == SUB_TOPIC_SUBSCRIBED){
    this->atuart->write(aws_start);
    this->atuart->write(aws_subcl);
    this->atuart->print(idx);
    this->atuart->write("\r\n");
    this->subtopics[idx].substate = SUB_TOPIC_UNSUBSCRIBING;
#ifdef TIMEOUT_RESPONSES
    this->subtopics[idx].senttime = millis();
#endif
#ifdef FILTER_OK
    this->incOKreq();
#endif  
    return 0;
  }
  return -1;
}

/* Publish topic API */

#define TOPIC_NOT_REGISTERED 0
#define TOPIC_REGISTERING    1
#define TOPIC_REGISTERED     2

/* Register a publish topic */
int eseyeAWS::pubreg(char *topic){
  int topiccount = 0;
#ifdef TIMEOUT_RESPONSES
  this->checkTimeout();
#endif
  while(this->pubtopics[topiccount].pubstate != PUB_TOPIC_NOT_IN_USE && this->pubtopics[topiccount].pubstate != PUB_TOPIC_ERROR && topiccount < MAX_PUB_TOPICS){
    topiccount++;
  }
  if(topiccount == MAX_PUB_TOPICS)
    return -1;
  this->atuart->write(aws_start);
  this->atuart->write(aws_pub);
  this->atuart->print(topiccount);
  this->atuart->write(",\"");
  this->atuart->write(topic);
  this->atuart->write("\"\r\n");
#ifdef FILTER_OK
  this->incOKreq();
#endif
  this->pubtopics[topiccount].pubstate = PUB_TOPIC_REGISTERING;
#ifdef TIMEOUT_RESPONSES
  this->pubtopics[topiccount].senttime = millis();
#endif
  return topiccount;
}

/* Check if publish topic is registered */
tpubTopicState eseyeAWS::pubstate(int idx){
#ifdef TIMEOUT_RESPONSES
  this->checkTimeout();
#endif
  return this->pubtopics[idx].pubstate;
}

/* Unregister a publish topic */
int eseyeAWS::pubunreg(int idx){
#ifdef TIMEOUT_RESPONSES
  this->checkTimeout();
#endif
  if(idx >= MAX_PUB_TOPICS)
    return -1;
  if(this->pubtopics[idx].pubstate == PUB_TOPIC_REGISTERED){
    this->atuart->write(aws_start);
    this->atuart->write(aws_pubcl);
    this->atuart->print(idx);
    this->atuart->write("\r\n"); 
    this->pubtopics[idx].pubstate = PUB_TOPIC_UNREGISTERING; 
#ifdef TIMEOUT_RESPONSES
    this->pubtopics[idx].senttime = millis();
#endif
#ifdef FILTER_OK
    this->incOKreq();
#endif
    return 0;
  }
  return -1;
}

/* Publish a message to a topic by index */
int eseyeAWS::publish(int tpcidx, uint8_t *data, uint8_t datalen){
#ifdef TIMEOUT_RESPONSES
  this->checkTimeout();
#endif  
  /* TODO - check we're not already sending something */
  if(tpcidx < MAX_PUB_TOPICS && this->pubtopics[tpcidx].pubstate == PUB_TOPIC_REGISTERED){
    this->atuart->write(aws_start);
    this->atuart->write(aws_publish);
    this->atuart->print(tpcidx);
    this->atuart->write(",");
    this->atuart->print(datalen);
    this->atuart->write("\r\n");
#ifdef FILTER_OK
    this->incOKreq();
#endif
    memcpy(this->txbuf, data, datalen);
    this->txbuflen = datalen;
  }
  return 0;
}

/* Check if previous publish is complete */
boolean eseyeAWS::pubdone(void){
  if(this->txbuflen == 0)
    return true;
  return false;
}

/* Polling loop - the work is done here */
void eseyeAWS::poll(void){
  char nextchar;
#ifdef TIMEOUT_RESPONSES
  this->checkTimeout();
#endif 
  while (this->atuart->available() > 0) {
    nextchar = this->atuart->read();

    //UARTDEBUGLN((uint8_t)nextchar);
    
    this->modemrxbuf[this->rxbufidx++] = nextchar;
    this->modemrxbuf[this->rxbufidx] = 0;
    if(this->binaryread > 0){
      this->binaryread--;
      this->buffered++;
      if(this->binaryread == 0){
        /* modemrxbuf contains the binary response/message */
        if(this->readingsub < MAX_SUB_TOPICS && this->subtopics[this->readingsub].messagecb != NULL){
          this->subtopics[this->readingsub].messagecb(this->modemrxbuf, this->buffered);
          this->buffered = 0;
          this->readingsub = 0xff; 
        }
        this->rxbufidx = 0;
      }
    }else{
      if(this->rxbufidx == 1 && nextchar == '>'){
        if(this->txbuflen > 0)
          this->atuart->write(this->txbuf, this->txbuflen);
        this->txbuflen = 0;
        this->rxbufidx = 0;
        nextchar = 0;
      }
      if(nextchar == '\n'){
        char *parseptr;
        boolean handled = false;
        /* This is the end of a response */
        if(strncmp((char *)this->modemrxbuf, aws_urc, strlen(aws_urc)) == 0){
            parseptr = (char *)&this->modemrxbuf[strlen(aws_urc)];          
            if(*parseptr == ':'){
              parseptr++;
                /* This is a published message to which we are subscribed */
                char *lenptr;
                /* Read the subindex */
                uint8_t idx = strtol(parseptr, &lenptr, 10);
                /* Read the length */
                uint8_t len = strtol(lenptr + 1, NULL, 10);
                this->readingsub = idx;
                this->binaryread = len;
                handled = true;
            }else if(strncmp(parseptr + 3, open_msg, strlen(open_msg)) == 0){
              char *errptr;
              /* Read the index */
              uint8_t idx = strtol(parseptr + 8, &errptr, 10);
              /* Read the error code */
              int8_t err = strtol(errptr + 1, NULL, 10);
              if(strncmp(parseptr, sub_start, strlen(sub_start)) == 0){
                UARTDEBUG("subscribe ");
                UARTDEBUG(idx);
                UARTDEBUG(" err ");
                UARTDEBUGLN(err);
                /* If we get an already subscribed error assume it was us from before a reboot */
                if(err == 0 || err == -2)
                  this->subtopics[idx].substate = SUB_TOPIC_SUBSCRIBED;
                else
                  this->subtopics[idx].substate = SUB_TOPIC_ERROR;
              }else if(strncmp(parseptr, pub_start, strlen(pub_start)) == 0){
                UARTDEBUG("pubreg ");
                UARTDEBUG(idx);
                UARTDEBUG(" err ");
                UARTDEBUGLN(err);
                /* If we get an already registered error assume it was us from before a reboot */
                if(err == 0 || err == -2)
                  this->pubtopics[idx].pubstate = PUB_TOPIC_REGISTERED;
                else
                  this->pubtopics[idx].pubstate = PUB_TOPIC_ERROR;    
              }
              handled = true;
            }else if(strncmp(parseptr + 3, close_msg, strlen(close_msg)) == 0){
              char *errptr;
              /* Read the index */
              uint8_t idx = strtol(parseptr + 9, &errptr, 10);
              /* Read the error code */
              int8_t err = strtol(errptr + 1, NULL, 10);
              if(strncmp(parseptr, sub_start, strlen(sub_start)) == 0){
                UARTDEBUG("unsubscribe ");
                UARTDEBUG(idx);
                UARTDEBUG(" err ");
                UARTDEBUGLN(err);
                //if(err == 0)
                  this->subtopics[idx].substate = SUB_TOPIC_NOT_IN_USE;
                //else
                //  subtopics[idx].substate = SUB_TOPIC_SUBSCRIBED;
              }else if(strncmp(parseptr, pub_start, strlen(pub_start)) == 0){
                UARTDEBUG("pubunreg ");
                UARTDEBUG(idx);
                UARTDEBUG(" err ");
                UARTDEBUGLN(err);
                //if(err == 0)
                  this->pubtopics[idx].pubstate = PUB_TOPIC_NOT_IN_USE;
                //else
                //  pubtopics[idx].pubstate = PUB_TOPIC_REGISTERED;
              }
              handled = true;
            }
        }else if(strncmp((char *)this->modemrxbuf, aws_sendok, strlen(aws_sendok)) == 0){
          UARTDEBUGLN("Send OK");
          handled = true;
        }else if(strncmp((char *)this->modemrxbuf, aws_sendfail, strlen(aws_sendok)) == 0){
          UARTDEBUGLN("Send Fail");
          handled = true;
        }
#ifdef FILTER_OK
        else if(this->outstanding_ok > 0){
            if(strncmp((char *)this->modemrxbuf, ok_msg, strlen(ok_msg)) == 0){
              this->outstanding_ok--;
              handled = true;
            }else if(strncmp((char *)this->modemrxbuf, error_msg, strlen(error_msg)) == 0){
              this->outstanding_ok--;
              handled = true;
            }else if(strncmp((char *)this->modemrxbuf, crlf_msg, strlen(crlf_msg)) == 0){
              handled = true;
            }
        }
#endif
        if(handled == false){
          if(this->atcallback != NULL){
            //UARTDEBUG("Forwarding ");
            //UARTDEBUGLN((char *)this->modemrxbuf);
            this->atcallback((char *)this->modemrxbuf);
          }else{
            UARTDEBUG("Discarding ");
            UARTDEBUGLN((char *)this->modemrxbuf);
          }
        }
        
        //this->modemrxbuf[this->rxbufidx] = 0;
        //UARTDEBUG("Handled ");
        //UARTDEBUGLN((char *)this->modemrxbuf);
        
        this->rxbufidx = 0;
      }
    }
    if(this->rxbufidx >= MODEM_RX_BUFSIZE){
      this->rxbufidx = 0;
    }
  }
}

/* Send an AT command */
void eseyeAWS::sendAT(char *atcmd){
#ifdef TIMEOUT_RESPONSES
    this->checkTimeout();
#endif
    this->atuart->print(atcmd);
}

/* Create and initialise API */

eseyeAWS::eseyeAWS(Stream *uart){
    if(this->atuart == NULL)
        this->atuart = uart;
    if(this->atuart == NULL)
        this->atuart = &ATSerial;
}

void eseyeAWS::init(_atcb urccallback, Stream *trcuart) {
    int i;
    for(i = 0; i < MAX_SUB_TOPICS; i++){
        this->subtopics[i].messagecb = NULL;
        this->subtopics[i].substate = SUB_TOPIC_NOT_IN_USE;
    }
    for(i = 0; i < MAX_PUB_TOPICS; i++){
        this->pubtopics[i].pubstate = PUB_TOPIC_NOT_IN_USE;
    }

    this->atcallback = urccallback;
    this->dbguart = trcuart;

    this->txbuflen = 0;
    this->rxbufidx = 0;
    this->binaryread = 0;
    this->buffered = 0;
    this->readingsub = 0xff;
}

#ifdef TIMEOUT_RESPONSES
boolean eseyeAWS::checkTimeout(void){
    /* Check pending pub/sub requests etc. */
    int topiccount = 0;
    boolean waiting = false;
    unsigned long now = millis(), diff;
    while(topiccount < MAX_PUB_TOPICS){
        diff = now - this->pubtopics[topiccount].senttime;
        if(this->pubtopics[topiccount].pubstate == PUB_TOPIC_REGISTERING || this->pubtopics[topiccount].pubstate == PUB_TOPIC_UNREGISTERING){
            if(diff >= PUB_TIMEOUT){
                this->pubtopics[topiccount].pubstate = PUB_TOPIC_ERROR;
                UARTDEBUG("Pub idx ");
                UARTDEBUG(topiccount);
                UARTDEBUGLN(" timed out");
            }else{
                waiting = true;
            }
        }
        topiccount++;
    }
    topiccount = 0;
    while(topiccount < MAX_SUB_TOPICS){
        diff = now - this->subtopics[topiccount].senttime;
        if(this->subtopics[topiccount].substate == SUB_TOPIC_SUBSCRIBING || this->subtopics[topiccount].substate == SUB_TOPIC_UNSUBSCRIBING){
            if(diff >= SUB_TIMEOUT){
                this->subtopics[topiccount].substate = SUB_TOPIC_ERROR;
                UARTDEBUG("Sub idx ");
                UARTDEBUG(topiccount);
                UARTDEBUGLN(" timed out");
            }else{
                waiting = true;
            } 
        }
        topiccount++;
    }

    return waiting;
}
#endif


/* Sleep support code - as yet untested */

#define INVALID_INT	0xFF

#define NO_INT_OCCURRED 0x00
#define CLICK_INT_OCCURRED  0x01
#define EXT_INT_OCCURRED  0x02

volatile uint8_t wokeUpByInterrupt = NO_INT_OCCURRED;
uint8_t clickIntNum = INVALID_INT;
uint8_t extIntNum   = INVALID_INT;

void clickWakeUp(){
	detachInterrupt(clickIntNum);
	if (extIntNum != INVALID_INT) {
		detachInterrupt(extIntNum);
	}
	wokeUpByInterrupt = CLICK_INT_OCCURRED;
}
void extWakeUp(){
	detachInterrupt(extIntNum);
	if (clickIntNum != INVALID_INT) {
		detachInterrupt(clickIntNum);
	}
	wokeUpByInterrupt = EXT_INT_OCCURRED;
}

bool eseyeAWS::interruptWakeUp(){
	return wokeUpByInterrupt != NO_INT_OCCURRED;
}

/* Define this ISR to allow WDT to exit sleep without a reboot */
ISR (WDT_vect){
}

void eseyeAWS::hwPowerDown(period_t period){
	// disable ADC for power saving
	ADCSRA &= ~(1 << ADEN);
	// save WDT settings
	uint8_t WDTprev = WDTCSR;
	if (period != SLEEP_FOREVER) {
		wdt_enable(period);
		// enable WDT interrupt before system reset
		WDTCSR |= (1 << WDCE) | (1 << WDIE);
	} else {
		// if sleeping forever, disable WDT
		wdt_disable();
	}
	set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	cli();
	sleep_enable();
#if defined __AVR_ATmega328P__
	sleep_bod_disable();
#endif
	// Enable interrupts & sleep until WDT or ext. interrupt
	sei();
	// Directly sleep CPU, to prevent race conditions! (see chapter 7.7 of ATMega328P datasheet)
	sleep_cpu();
	sleep_disable();
	// restore previous WDT settings
	cli();
	wdt_reset();
	// enable WDT changes
	WDTCSR |= (1 << WDCE) | (1 << WDE);
	// restore saved WDT settings
	WDTCSR = WDTprev;
	sei();
	// enable ADC
	ADCSRA |= (1 << ADEN);
}

void eseyeAWS::hwInternalSleep(unsigned long ms){
	/* Flush uart buffers */
#ifdef DEBUG_ESEYEAWS
    if(this->dbguart != NULL)
        this->dbguart->flush();
#endif
    this->atuart->flush();

	while (!interruptWakeUp() && ms >= 8000) {
		hwPowerDown(SLEEP_8S);
		ms -= 8000;
	}
	if (!interruptWakeUp() && ms >= 4000)    {
		hwPowerDown(SLEEP_4S);
		ms -= 4000;
	}
	if (!interruptWakeUp() && ms >= 2000)    {
		hwPowerDown(SLEEP_2S);
		ms -= 2000;
	}
	if (!interruptWakeUp() && ms >= 1000)    {
		hwPowerDown(SLEEP_1S);
		ms -= 1000;
	}
	if (!interruptWakeUp() && ms >= 500)     {
		hwPowerDown(SLEEP_500MS);
		ms -= 500;
	}
	if (!interruptWakeUp() && ms >= 250)     {
		hwPowerDown(SLEEP_250MS);
		ms -= 250;
	}
	if (!interruptWakeUp() && ms >= 125)     {
		hwPowerDown(SLEEP_120MS);
		ms -= 120;
	}
	if (!interruptWakeUp() && ms >= 64)      {
		hwPowerDown(SLEEP_60MS);
		ms -= 60;
	}
	if (!interruptWakeUp() && ms >= 32)      {
		hwPowerDown(SLEEP_30MS);
		ms -= 30;
	}
	if (!interruptWakeUp() && ms >= 16)      {
		hwPowerDown(SLEEP_15MS);
		ms -= 15;
	}
}

twakeReason eseyeAWS::hostSleep(uint8_t clickInt, uint8_t clickIntMode, uint8_t extInt, uint8_t extIntMode, unsigned long sleepMs){
    /* Disable ints until we are ready to sleep so we don't get any early ints which will prevent wakeup */
	cli();
	/* Attach ints */
	clickIntNum  = clickInt;
	extIntNum  = extInt;
	if (clickIntNum != INVALID_INT) {
		attachInterrupt(clickIntNum, clickWakeUp, clickIntMode);
	}
	if (extIntNum != INVALID_INT) {
		attachInterrupt(extIntNum, extWakeUp, extIntMode);
	}

	if (sleepMs > 0) {
		// sleep for defined time
		hwInternalSleep(sleepMs);
	} else {
		// sleep until ext interrupt triggered
		hwPowerDown(SLEEP_FOREVER);
	}

	/* Detach ints */
	if (clickIntNum != INVALID_INT) {
		detachInterrupt(clickIntNum);
	}
	if (extIntNum != INVALID_INT) {
		detachInterrupt(extIntNum);
	}

	/* What woke host? */
	twakeReason ret = WAKE_TIMER;     
	if (interruptWakeUp()) {
        switch(wokeUpByInterrupt){
            case CLICK_INT_OCCURRED:
                ret = WAKE_CLICK;
                break;
            case EXT_INT_OCCURRED:
                ret = WAKE_INT;
                break;
            default:
                break;
        }
	}
	// Clear woke-up-by-interrupt flag, so next sleeps won't return immediately.
	wokeUpByInterrupt = INVALID_INT;

	return ret;
}

/* Register the GPIO pins used to power-down the click board and wake the host */
void eseyeAWS::sleepctrl(uint8_t clickSleepPin, int clickSleepPolarity, uint8_t hostWakeIntPin, int hostWakeIntPolarity){
    clkslppin = clickSleepPin;
    clkslppol = clickSleepPolarity;
    hstwkpin = hostWakeIntPin;
    hstwkpol = hostWakeIntPolarity;
    return;
}

/* Sleep for the duration specified - signal to the click board our desire for it to power down
 * Wake on signal from click board, timeout or external interrupt (if specified) */
twakeReason eseyeAWS::sleep(unsigned long duration_mS, int additionalWakeGpio, int additionalWakeGpioPolarity){
    twakeReason wkreason;
#ifdef TIMEOUT_RESPONSES
    if(checkTimeout() == true)
        return TRY_AGAIN_SHORTLY;
#endif
    /* Set sleep GPIO to click */
    wkreason = hostSleep(hstwkpin, hstwkpol, additionalWakeGpio, additionalWakeGpioPolarity, duration_mS);
    /* Clear sleep GPIO to click */

    return wkreason;
}





