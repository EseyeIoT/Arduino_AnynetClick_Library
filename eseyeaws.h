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


#ifndef ESEYEAWS_H__
#define ESEYEAWS_H__

#if defined(ARDUINO) && (ARDUINO >= 100)
#include <Arduino.h>
#else
#include <WProgram.h>
#endif

/* FILTER_OK attempts to filter AT command responses from the AWS application 
 * while passing through responses to other AT commands from the application.
 * If the application is not using AT commands to the modem do not define 
 * FILTER_OK or set urccb as it will just waste program memory/cpu cycles. */
#define FILTER_OK

/* DEBUG_ESEYEAWS adds debug trace support to the uart selected in the call to 
 * init(). If you are not using this it's best not to define DEBUG_ESEYEAWS. */
#define DEBUG_ESEYEAWS

/* TIMEOUT_RESPONSES waits for a period of time after sub/pub commands and 
 * marks the index as errored if no response has been seen. You cannot publish to 
 * an errored topic */
//#define TIMEOUT_RESPONSES
#ifdef TIMEOUT_RESPONSES
#define PUB_TIMEOUT 3000UL /* 3 second timeout */
#define SUB_TIMEOUT 3000UL /* 3 second timeout */
#endif

#define ESEYEAWSLIB_VERSION "0.5"

#define MAX_SUB_TOPICS 8
#define MAX_PUB_TOPICS 8		

/* Prototype for the AT command response callback function */
typedef void (*_atcb)(char *data);
/* Prototype for the message callback function */	
typedef void (*_msgcb)(uint8_t *data, uint8_t length);
/* Publish topic state */
typedef enum {PUB_TOPIC_ERROR = -1, PUB_TOPIC_NOT_IN_USE = 0, PUB_TOPIC_REGISTERING, PUB_TOPIC_REGISTERED, PUB_TOPIC_UNREGISTERING} tpubTopicState;
/* Subscribe topic state */
typedef enum {SUB_TOPIC_ERROR = -1, SUB_TOPIC_NOT_IN_USE = 0, SUB_TOPIC_SUBSCRIBING, SUB_TOPIC_SUBSCRIBED, SUB_TOPIC_UNSUBSCRIBING} tsubTopicState;
/* Reason for waking up/not sleeping (unable to sleep currently, timer, message from click board or external interrupt) */
typedef enum {TRY_AGAIN_SHORTLY, WAKE_TIMER, WAKE_CLICK, WAKE_INT} twakeReason;

/* Subscribed topic array element */	
struct subtpc{
  _msgcb messagecb;
  tsubTopicState substate;
#ifdef TIMEOUT_RESPONSES
  /* Include a senttime for each sub/unsub to enable timeout */
  unsigned long senttime;
#endif
};

/* Publish topic array element */
struct pubtpc{
  tpubTopicState pubstate;
#ifdef TIMEOUT_RESPONSES
  /* Include a senttime for each pub to enable timeout */
  unsigned long senttime;
#endif
};
				
class eseyeAWS
{
public:
	eseyeAWS(Stream *uart);
    void init(_atcb urccallback = NULL, Stream *trcuart = NULL);
    void sleepctrl(uint8_t clickSleepPin, int clickSleepPolarity, uint8_t hostWakeIntPin, int hostWakeIntPolarity);
    twakeReason sleep(unsigned long duration_mS, int additionalWakeGpio = -1, int AdditionalWakeGpioPolarity = 0);

    /* Subscribe topic API */
    int subscribe(char *topic, _msgcb callback);
    tsubTopicState substate(int idx);
    int unsubscribe(int idx);

    /* Publish topic API */
    int pubreg(char *topic);
    tpubTopicState pubstate(int idx);
    int pubunreg(int idx);
	
    /* Publish API */
    int publish(int tpcidx, uint8_t *data, uint8_t datalen);
    boolean pubdone(void);
	
    /* Polling loop */
    void poll(void);

    /* Send AT command */
    void sendAT(char *atcmd);

    twakeReason hostSleep(uint8_t clickInt, uint8_t clickIntMode, uint8_t extInt, uint8_t extIntMode, unsigned long sleepMs);

private:
    /* Callback function for unhandled URCs */
    _atcb atcallback;	
  
    struct subtpc subtopics[MAX_SUB_TOPICS];
    struct pubtpc pubtopics[MAX_PUB_TOPICS];

    Stream *atuart;
    Stream *dbguart;

    #define MODEM_TX_BUFSIZE 128
    uint8_t txbuf[MODEM_TX_BUFSIZE];
    uint8_t txbuflen;

    #define MODEM_RX_BUFSIZE 100
    uint8_t modemrxbuf[MODEM_RX_BUFSIZE];
    unsigned char rxbufidx;
    unsigned char binaryread;
    unsigned char buffered;
    uint8_t readingsub;
#ifdef FILTER_OK
    uint8_t outstanding_ok;
    void incOKreq(void);
#endif

    uint8_t clkslppin;
    uint8_t clkslppol;
    uint8_t hstwkpin;
    uint8_t hstwkpol;

#ifdef TIMEOUT_RESPONSES
    boolean checkTimeout(void);
#endif
    bool interruptWakeUp(void);
    void hwInternalSleep(unsigned long ms);

enum period_t {
	SLEEP_15MS,
	SLEEP_30MS,
	SLEEP_60MS,
	SLEEP_120MS,
	SLEEP_250MS,
	SLEEP_500MS,
	SLEEP_1S,
	SLEEP_2S,
	SLEEP_4S,
	SLEEP_8S,
	SLEEP_FOREVER
};

    void hwPowerDown(period_t period);

};
#endif // ESEYEAWS_H

