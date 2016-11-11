
#include <EEPROMex.h>
#include <EEPROMVar.h>
#include "LowPower.h"
#include <SoftwareSerial.h>

// Pins
#define PHONE_POWER_PIN (8)
#define PHONE_RESET_PIN (9)
#define RX_PIN (6)
#define TX_PIN (7)

#define PHONE_BAUD (19200)

#define END_OF_STRING ('\0')

#define AT ("AT")
#define AT_IDENTIFY ("ATI")
#define AT_ECHO_OFF ("ATE0")
#define AT_POWER_OFF ("AT+CPOF")
#define AT_NETWORK_REGISTRATION ("AT+CREG?")
#define AT_NETWORK_REGISTRATION_RESPONSE ("+CREG: 0,1")
#define AT_DEFINE_SOCKET_PDP ("AT+CGSOCKCONT=1,\"IP\",\"%s\"")
#define AT_DEFINE_PDP_AUTH ("AT+CSOCKAUTH=1,0")                   // No authentication
#define AT_HTTP ("AT+CHTTPACT=\"%s\",80")
#define AT_HTTP_RESPONSE ("+CHTTPACT: REQUEST")
#define AT_HTTP_RESPONSE_COMPLETE ("+CHTTPACT: DATA,")

#define OK ("OK")
#define ESC (0x1A)
#define CRLF ('\n')
#define SPACE (" ")

#define MANUFACTURER ("Manufacturer: ")
#define MODEL ("Model: ")
#define REVISION ("Revision: ")
#define IMEI ("IMEI: ")

#define READING_BUFFER_SIZE (20)
#define MAX_HOST_LENGTH (60)

#define HTTP_HOST ("Host: %s")
#define HTTP_CONTENT_TYPE_JSON ("Content-Type: application/json")
#define HTTP_CONTENT_LENGTH ("Content-Length: %d")
#define HTTP_CACHE_CONTROL_NO_CACHE ("Cache-Control: no-cache")

#define HTTP_POST_READING ("POST /rest/remote/spb/%s/reading HTTP/1.1")
// #define HTTP_POST_READING ("POST /post HTTP/1.1")
#define JSON_READING ("{\"grams\": %d, \"degreesC\": %d.%d}")

struct Configuration {
  char remoteUrlBase[MAX_HOST_LENGTH];
  char apiKey[36];
  char apn[20];
  period_t sleepTime;
  unsigned int readingMillis;
  unsigned int remoteSendMillis;
  unsigned int version;
};

struct PhoneConfig {
  //char manufacturer[20];
  //char model[20];
  //char revision[20];
  char imei[15];
};

struct Reading {
  unsigned long readingTimeMillis;
  unsigned long grams;
  int temperatureTenthsDegree;
};

SoftwareSerial phoneSerial(RX_PIN, TX_PIN);

struct PhoneConfig phoneConfig;
struct Configuration configuration;
struct Reading readings[READING_BUFFER_SIZE];
unsigned int readingsSize = 0;

uint8_t indent = 0;
unsigned long cumulativeSleepMillis = 0;
unsigned long previousMillis;
unsigned int serialBufferSize;

void setup() {
  Serial.begin(115200);
  serialBufferSize = Serial.availableForWrite();
  logln(F("setup()- Start"));
  pushLogLevel();
/*
  //strcpy(configuration.remoteUrlBase, "http://122.107.211.41");
  //strcpy(configuration.remoteUrlBase, "http://httpbin.org");
  strcpy(configuration.remoteUrlBase, "http://smartspb-infra.ap-southeast-2.elasticbeanstalk.com");
  strcpy(configuration.apiKey, "16fa2ee7-6614-4f62-bc16-a3c6fa189675");
  strcpy(configuration.apn, "telstra.wap");
  configuration.sleepTime = SLEEP_8S;
  configuration.readingMillis = 0;
  configuration.remoteSendMillis = 60000;
  configuration.version = 1;
  EEPROM.writeBlock(0, configuration);

*/
  EEPROM.readBlock(0, configuration);

  logConfiguration();

  phoneSerial.begin(PHONE_BAUD);
  
  phonePowerOn();
  
  phoneConfiguration();

  // Send a message that we are online

  phonePowerOff();

  previousMillis = millis();

  addReading(takeReading());
  addReading(Reading {ms(), 0, 205});

  // Testing only
  sendRemote();
  
  popLogLevel();
  logln(F("setup()- End"));
}

void loop() {
  return;
  goToSleep();
  performReading();
  performSending();  
}

void goToSleep() {
  waitForSerialBufferToEmpty();
  LowPower.powerDown(configuration.sleepTime, ADC_OFF, BOD_OFF);

  // Timer stops while we are asleep, so we need to keep track of it
  cumulativeSleepMillis += 8000;  
}

void performReading() {
  if (isReadingTime()) {
    struct Reading reading = takeReading();
    if (readingChanged(reading)) {
      addReading(reading);
    }
  }
}

void performSending() {
  if (isRemoteSendTime()) {
    sendRemote();
  }
}

boolean isReadingTime() {
  return ms() - previousMillis > configuration.readingMillis;
}

boolean isRemoteSendTime() {
  return ms() - previousMillis > configuration.remoteSendMillis;
}

struct Reading takeReading() {
  logln(F("takeReading()- Start"));
  pushLogLevel();

  // Read the scales and the temperature here
  Reading reading = {ms(), 10, 210};
  popLogLevel();
  logln(F("takeReading()- End"));
  return reading;
}

boolean readingChanged(struct Reading reading) {
  if (readingsSize == 0) {
    return true;
  }
  if (reading.grams != readings[readingsSize - 1].grams ||
      reading.temperatureTenthsDegree != readings[readingsSize - 1].temperatureTenthsDegree) {
    return true;
  }
  return false;
}

void addReading(struct Reading reading) {
  readings[readingsSize++] = reading;
  readingsSize = readingsSize % READING_BUFFER_SIZE;
}

boolean sendRemote() {
  logln(F("sendRemote()- Start"));
  pushLogLevel();
  boolean returnValue;
  // if (phonePowerOn()) {
    // Send the data
    char buffer[80];
    char reading[50];

    sprintf(reading, JSON_READING, 505, 212 / 10, 212 % 10);
    
    char host[MAX_HOST_LENGTH];
    getHost(host);
    //char host = 'httpbin.org';

    sprintf(buffer, AT_HTTP, host);
    logln(buffer);
    
    sendATcommand(buffer, AT_HTTP_RESPONSE, 5000, 50);
    
    sprintf(buffer, HTTP_POST_READING, phoneConfig.imei);
    logln(buffer);
    //sendLn(buffer);

    sprintf(buffer, HTTP_HOST, host);
    logln(buffer);
    //sendLn(buffer);

    sprintf(buffer, HTTP_CONTENT_TYPE_JSON);
    logln(buffer);
    //sendLn(buffer);

    sprintf(buffer, HTTP_CONTENT_LENGTH, strlen(reading));
    logln(buffer);
    //sendLn(buffer);

    sprintf(buffer, HTTP_CACHE_CONTROL_NO_CACHE);
    logln(buffer);
    //sendLn(buffer);
    
    sprintf(buffer, "");
    logln(buffer);
    //sendLn(buffer);
    
    logln(reading);
    //sendLn(reading);
    
    buffer[0] = ESC;
    buffer[1] = END_OF_STRING;
    send(buffer);
    /*

    logln(F("Waiting for something to appear"));
    while(phoneSerial.available() == 0) {
      
    }
    logln(F("Reading the buffer"));
    delay(500);
    while(phoneSerial.available() > 0) {
      Serial.print((char) phoneSerial.read());
      delay(10);
    }
    */
    phonePowerOff();
    previousMillis = ms();
    returnValue = true;
  //} else {
  //  returnValue = false;
  //}
  popLogLevel();
  logln(F("sendRemote()- End"));
  return returnValue;
}

unsigned long ms() {
  return millis() + cumulativeSleepMillis;
}

boolean phonePowerOn() {
  logln(F("phonePowerOn() - Start"));
  pushLogLevel();
  logln(F("Checking phone module power on status"));
  if (!sendATcommand(AT, OK, 200, 10)) {
    logln(F("Currently powered off - powering on"));
    digitalWrite(PHONE_POWER_PIN, HIGH);
    delay(200);
    digitalWrite(PHONE_POWER_PIN, LOW);
    logln(F("Waiting for power on"));
    delay(6000);
    while (!sendATcommand(AT, OK, 2000, 10)) {
      logln(F("Checking phone module power on status"));
      delay(1000);
    }
  }  
  logln(F("Phone module is powered on"));

  char response[30];
  logln(F("Turning echo off"));
  sendATCommandResponse(AT_ECHO_OFF, OK, 200, 30, response);
  
  do {
    logln(F("Waiting for network"));
    delay(1000);
    logln(AT_NETWORK_REGISTRATION);
    sendATCommandResponse(AT_NETWORK_REGISTRATION, OK, 200, 30, response);
    logln(response);
  } while (strstr(response, AT_NETWORK_REGISTRATION_RESPONSE) == NULL);

  char buffer[30];
  sprintf(buffer, AT_DEFINE_SOCKET_PDP, configuration.apn);
  logln(F("Defining socket PDP context"));
  logln(buffer);
  sendATCommandResponse(buffer, OK, 2000, 30, response);
  logln(response);
  
  logln(F("Setting PDP auth mode to none"));
  sendATCommandResponse(AT_DEFINE_PDP_AUTH, OK, 2000, 30, response);
  logln(response);
  
  popLogLevel();
  logln(F("phonePowerOn() - End"));
  return true;
}

boolean phonePowerOff() {
  return true;
  logln(F("phonePowerOff() - Start"));
  pushLogLevel();
  boolean result = sendATcommand(AT_POWER_OFF, OK, 2000, 20);
  popLogLevel();
  logln(F("phonePowerOff() - End"));  
  return result;
}

void phoneConfiguration() {
  logln(F("phoneConfiguration() - Start"));
  pushLogLevel();

  logln(F("Getting phone configuration"));
  unsigned int bufferSize = 250;
  char response[bufferSize];
  sendATCommandResponse(AT_IDENTIFY, OK, 200, bufferSize, response);
  if (strlen(response) == 0) {
    logln(F("Timed out"));
  } else {
    //extractConfigItem(response, MANUFACTURER, phoneConfig.manufacturer);
    //extractConfigItem(response, MODEL, phoneConfig.model);
    //extractConfigItem(response, REVISION, phoneConfig.revision);
    extractConfigItem(response, IMEI, phoneConfig.imei);

    //log(MANUFACTURER);
    //logaddln(phoneConfig.manufacturer);
    //log(MODEL);
    //logaddln(phoneConfig.model);
    //log(REVISION);
    //logaddln(phoneConfig.revision);
    log(IMEI);
    logaddln(phoneConfig.imei);
  }
  
  popLogLevel();
  logln(F("phoneConfiguration() - End"));
}

boolean extractConfigItem(char* response, char* parameter, char* answer) {
  String config = String(response);
  int parameterPos = config.indexOf(parameter);
  int startPos = parameterPos + strlen(parameter);
  int newLinePos = config.indexOf(CRLF, startPos + 1);
  logln(config.substring(startPos, newLinePos - 1));
  config.substring(startPos, newLinePos - 1).toCharArray(answer, 20);
  return true;
}

boolean sendATcommand(char* ATcommand, char* expectedAnswer, unsigned int timeout, uint8_t bufferSize) {
  char response[bufferSize];
  sendATCommandResponse(ATcommand, expectedAnswer, timeout, bufferSize, response);
  if (strlen(response) == 0) {
    return false;
  } else {
    return true;
  }
}

boolean sendATCommandResponse(char* command, char* endText, unsigned int timeout, uint8_t bufferSize, char* response) {
  Serial.println("1");
  memset(response, END_OF_STRING, bufferSize);
  Serial.println("2");
  uint8_t bufferPointer=0;
  unsigned long startMillis;
  Serial.println("3");
  
  clearSerialBuffer();
  Serial.println("4");

  sendLn(command);
  startMillis = millis();
  Serial.println("5");

  do {
    if (phoneSerial.available() != 0) {    
      response[bufferPointer++] = phoneSerial.read();
      bufferPointer = bufferPointer % bufferSize;
      if (strstr(response, endText) != NULL) {
        return true;
      }
    }
  } while((millis() - startMillis) < timeout);
  response[0] = END_OF_STRING;
  
  return false;
}

void send(char* text) {
  phoneSerial.print(text);
}

void send(byte b) {
  phoneSerial.print(b);
}

void sendLn(char* text) {
  phoneSerial.println(text);
}

void clearSerialBuffer() {
  while(phoneSerial.available() > 0) {
    phoneSerial.read();
  }  
}

void logConfiguration() {
  logln(F("logConfiguration()- Start"));
  pushLogLevel();
  log(F("SERIAL_TX_BUFFER_SIZE: "));
  logaddln(SERIAL_TX_BUFFER_SIZE);
  log(F("SERIAL_RX_BUFFER_SIZE: "));
  logaddln(SERIAL_RX_BUFFER_SIZE);
  
  log(F("_SS_MAX_RX_BUFF: "));
  logaddln(_SS_MAX_RX_BUFF);
  
  log(F("remoteBaseURL: "));
  logaddln(configuration.remoteUrlBase);
  log(F("apiKey: "));
  logaddln(configuration.apiKey);
  log(F("readingMillis: "));
  logaddln(configuration.readingMillis);
  log(F("remoteSendMillis: "));
  logaddln(configuration.remoteSendMillis);
  log(F("version: "));
  logaddln(configuration.version);
  popLogLevel();
  logln(F("logConfiguration()- End"));
}

void logln(char* message) {
  indentLog();
  Serial.println(message);
}

void logln(String message) {
  indentLog();
  Serial.println(message);
}

void logln(const __FlashStringHelper* message) {
  indentLog();
  Serial.println(message);
}

void log(char* message) {
  indentLog();
  Serial.print(message);
}

void log(const __FlashStringHelper* message) {
  indentLog();
  Serial.print(message);
}

void logaddln(char* message) {
  Serial.println(message);
}

void logaddln(String message) {
  Serial.println(message);
}

void logaddln(unsigned int message) {
  Serial.println(message);
}

void pushLogLevel() {
  indent += 2;  
}

void popLogLevel() {
  indent -= 2;
}

void indentLog() {
  for (int i = 0; i < indent; i++) {
    Serial.print(" ");
  }  
}

void waitForSerialBufferToEmpty() {
  while (Serial.availableForWrite() < serialBufferSize) {
    delay(20);
  }
}

char* getHost(char* host) {
  String s = String(configuration.remoteUrlBase);
  int start = s.indexOf("//") + 2;
  String h = s.substring(start, s.length());
  h.toCharArray(host, MAX_HOST_LENGTH);
  return host;
}

