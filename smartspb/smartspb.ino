#define SERIAL_RX_BUFFER_SIZE 256

#include <EEPROMex.h>
#include <EEPROMVar.h>
#include "HX711.h"

// Pins
#define HX711_DOUT  2
#define HX711_CLK  3
#define HX711_AVERAGE_NUMBER 5
#define HX711_READING_DELAY_MILLIS 100
#define HX711_NUMBER_OF_READINGS 4

#define PHONE_POWER_PIN (8)
#define PHONE_RESET_PIN (9)

#define PHONE_BAUD (115200)

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
#define AT_HTTP_RESPONSE_INCOMPLETE ("+CHTTPACT: DATA,")
#define AT_HTTP_RESPONSE_COMPLETE ("+CHTTPACT: 0")

#define OK ("OK")
#define ESC (0x1A)
#define CRLF ('\n')
#define SPACE (" ")

#define MANUFACTURER ("Manufacturer: ")
#define MODEL ("Model: ")
#define REVISION ("Revision: ")
#define IMEI ("IMEI: ")

#define READING_BUFFER_SIZE (20)
#define MAX_HOST_LENGTH (61)

#define HTTP_TIMEOUT (20000)
#define HTTP_HOST ("Host: %s")
#define HTTP_CONTENT_TYPE_JSON ("Content-Type: application/json")
#define HTTP_TRANSFER_ENCODING ("Transfer-Encoding: chunked")

#define HTTP_CACHE_CONTROL_NO_CACHE ("Cache-Control: no-cache")
#define HTTP_ACCEPT ("Accept: */*")
#define HTTP_API_KEY ("apiKey: %s")

#define HTTP_POST_READING ("POST /rest/remote/spb/%s/reading HTTP/1.1")
#define JSON_READING ("{\"grams\":%ld,\"totalGrams\":%ld,\"degreesC\":%d.%d,\"secondsAgo\":%ld}")

#define SCALE_TOLERANCE (6)

HX711 scale(HX711_DOUT, HX711_CLK);   // parameter "gain" is ommited; the default value 128 is used by the library

struct Configuration {
  char remoteUrlBase[MAX_HOST_LENGTH];
  char apiKey[37];
  char apn[21];
  unsigned int readingMillis;
  unsigned int remoteSendMillis;
  unsigned int version;
};

struct PhoneConfig {
  //char manufacturer[20];
  //char model[20];
  //char revision[20];
  char imei[16];
};

struct Reading {
  unsigned long timeMillis;
  long grams;
  long totalGrams;
  int temperatureTenthsDegree;
};

long scaleZeroReading;
boolean firstReading = true;
long scaleGramsFactor = 231243l;

struct PhoneConfig phoneConfig;
struct Configuration configuration;
struct Reading readings[READING_BUFFER_SIZE];
unsigned int readingsSize = 0;
long previousGrams[3];

uint8_t indent = 0;
unsigned long cumulativeSleepMillis = 0;
unsigned long previousScaleMillis;
unsigned long previousSendMillis;
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
  strcpy(configuration.apn, "telstra.internet");
  configuration.readingMillis = 8000;
  configuration.remoteSendMillis = 60000;
  configuration.version = 1;
  EEPROM.writeBlock(0, configuration);
*/

  EEPROM.readBlock(0, configuration);

  logConfiguration();

  Serial1.begin(PHONE_BAUD);
  
  phonePowerOn();
  
  phoneConfiguration();

  // Send a message that we are online

  phonePowerOff();

  previousSendMillis = ms();
  previousScaleMillis = ms();
  // Testing only
  // sendRemote();
  
  popLogLevel();
  logln(F("setup()- End"));
}

void loop() {
  if (firstReading) {
    takeReading();
    firstReading = false;
  }
  goToSleep();
  performReading();
  performSending();  
}

long readScale() {
  scale.power_up();
  long readingTotal = 0;
  for (int i = 0; i < HX711_NUMBER_OF_READINGS; i++) {
    readingTotal +=  scale.read_average(HX711_AVERAGE_NUMBER);
    delay(HX711_READING_DELAY_MILLIS);
  }
  scale.power_down();
  return readingTotal / HX711_NUMBER_OF_READINGS;
}

void goToSleep() {
  waitForSerialBufferToEmpty();
  // LowPower.powerDown(configuration.sleepTime, ADC_OFF, BOD_OFF);

  // Timer stops while we are asleep, so we need to keep track of it
  // cumulativeSleepMillis += 8000;  
}

void performReading() {
  if (isReadingTime()) {
    takeReading();
  }
}

void performSending() {
  if (isRemoteSendTime()) {
    sendRemote();
    previousSendMillis = ms();
  }
}

boolean isReadingTime() {
  return ms() - previousScaleMillis > configuration.readingMillis;
}

boolean isRemoteSendTime() {
  return ms() - previousSendMillis > configuration.remoteSendMillis;
}

void takeReading() {
  logln(F("takeReading()- Start"));
  pushLogLevel();

  long scaleReading = readScale();
  if (scaleZeroReading == 0) {
    scaleZeroReading = scaleReading;
  }
  long grams = ((scaleReading - scaleZeroReading) * 10000l) / scaleGramsFactor;
  long newGrams;
  
  log(F("grams = "));
  logaddln(grams);
  // Temperature dummy at the moment
  if (previousGrams[0] - grams > previousGrams[0] * 0.5 && previousGrams[0] - grams > SCALE_TOLERANCE) {
    logln("Zeroing scale");
    scaleZeroReading = 0;
    previousGrams[0]=0;
    previousGrams[1]=0;
    previousGrams[2]=0;
    Reading reading = {ms(), 0, 0, 210};
    addReading(reading);
  } else if (readingChanged(grams, newGrams) || firstReading) {
    log(F("Reading has changed - adding new reading grams="));
    logaddln(newGrams);
    // The new reading is the previous reading plus the new reading.
    // This elminates slow drift in the scale as we only count changes.
    long totalGrams;
    if (readingsSize == 0) {
      totalGrams = newGrams;
    } else {
      totalGrams = readings[readingsSize - 1].totalGrams + newGrams;
    }
    log(F("Reading has changed - adding new reading totalGrams="));
    logaddln(totalGrams);
    Reading reading = {ms(), newGrams, totalGrams, 210};
    addReading(reading);
  }
  previousScaleMillis = ms();
  // Roll the history
  previousGrams[2] = previousGrams[1];
  previousGrams[1] = previousGrams[0];
  previousGrams[0] = grams;
  popLogLevel();
  logln(F("takeReading()- End"));
}

boolean readingChanged(long grams, long &newReading) {
  logln(F("readingChanged()- Start"));
  pushLogLevel();
  log("previousGrams[0]=");
  logaddln(previousGrams[0]);
  log("previousGrams[1]=");
  logaddln(previousGrams[1]);
  log("previousGrams[2]=");
  logaddln(previousGrams[2]);
  boolean retVal = false;
  
  log("Checking the reading ");
  logaddln(grams);
  if (readingsSize == 0) {
    newReading = grams;
    retVal = true;
  } else if (abs(grams - previousGrams[0]) > SCALE_TOLERANCE) {
    logln("Haven't got two stable readings");
    // We need two stable readings in a row to count as an official reading
    retVal = false;
  } else if (abs(previousGrams[0] - previousGrams[1]) > SCALE_TOLERANCE
    && abs(previousGrams[1] - previousGrams[2]) > SCALE_TOLERANCE
    && (previousGrams[0] - previousGrams[1] > 0 && previousGrams[1] - previousGrams[2] > 0
      || previousGrams[0] - previousGrams[1] < 0 && previousGrams[1] - previousGrams[2] < 0)) {
    // Big bounce
    logln("Big bounce");
    newReading = previousGrams[0] - previousGrams[2];
    retVal = true;
  } else if (abs(previousGrams[0] - previousGrams[1]) <= SCALE_TOLERANCE
      && abs(previousGrams[1] - previousGrams[2]) <= SCALE_TOLERANCE
      && abs(previousGrams[0] - previousGrams[2]) > SCALE_TOLERANCE
          && (previousGrams[0] - previousGrams[1] > 0 && previousGrams[1] - previousGrams[2] > 0
      || previousGrams[0] - previousGrams[1] < 0 && previousGrams[1] - previousGrams[2] < 0)) {

    // Check for a small bounce
    logln("Small bounce");
    newReading = previousGrams[0] - previousGrams[2];
    retVal = true;
  } else if (abs(previousGrams[0] - previousGrams[1]) > SCALE_TOLERANCE) {
    // Normal
    newReading = previousGrams[0] - previousGrams[1];
    log("Article detected weight = ");
    logaddln(newReading);
    retVal = true;
  } else {
    log("Reading isn't greater than scale tolerance ");
    logaddln(SCALE_TOLERANCE);
  }
  popLogLevel();
  logln(F("readingChanged()- End"));
  return retVal;
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
    
    char* host = "smartspb-infra.ap-southeast-2.elasticbeanstalk.com";
    sprintf(buffer, AT_HTTP, host);
    logln(buffer);
    
    sendATcommand(buffer, AT_HTTP_RESPONSE, 5000, 50);
    
    sprintf(buffer, HTTP_POST_READING, phoneConfig.imei);
    logln(buffer);
    sendLn(buffer);

    sprintf(buffer, HTTP_HOST, host);
    logln(buffer);
    sendLn(buffer);

    sprintf(buffer, HTTP_CONTENT_TYPE_JSON);
    logln(buffer);
    sendLn(buffer);

    sprintf(buffer, HTTP_TRANSFER_ENCODING);
    logln(buffer);
    sendLn(buffer);

    sprintf(buffer, HTTP_CACHE_CONTROL_NO_CACHE);
    logln(buffer);
    sendLn(buffer);

    sprintf(buffer, HTTP_ACCEPT);
    logln(buffer);
    sendLn(buffer);

    sprintf(buffer, HTTP_API_KEY, configuration.apiKey);
    logln(buffer);
    sendLn(buffer);

    sprintf(buffer, "");
    logln(buffer);
    sendLn(buffer);

    // Use chunked encoding so we don't have to know content length
    logln("1");
    sendln("1");
    logln("[");
    sendln("[");

    for (int i = 0; i < readingsSize; i++) {
      // If there's more than one reading don't send the first reading as we've already sent it last time
      if (i > 0 || readingsSize == 1) {
        char reading[70];
        long timeMillis = readings[i].timeMillis;
        if (readingsSize == 1) {
          // If there is only one reading, then it is just a resend, so use the
          // current time for the reading
          timeMillis = ms();
        }
        sprintf(reading, JSON_READING, readings[i].grams, readings[i].totalGrams, 21, 2, (ms() - timeMillis) / 1000);
    
        if (i < readingsSize - 1) {
          strcat(reading, ",");
        }
        String hexLength = String(strlen(reading), HEX);
        logln(hexLength);
        sendln(hexLength);
        
        logln(reading);
        sendLn(reading);
      }
    }

    logln("1");
    sendln("1");
    logln("]");
    sendln("]");

    logln("0");
    sendLn("0");
    logln("");
    sendln("");
    
    buffer[0] = ESC;
    buffer[1] = END_OF_STRING;
    send(buffer);

    logln(F("Waiting for something to appear"));

    char response[1024];
    unsigned int responsePointer = 0;
    memcpy(response, END_OF_STRING, 1024);
    
    unsigned long startTime = millis();
    
    boolean done = false;
    boolean timeout = false;
    while (!done && !timeout) {
      if (Serial1.available() > 0) {
        response[responsePointer++] = (char) Serial1.read();
        if (responsePointer == 1024) {
          timeout = true;
          clearSerialBuffer();
        }
        // Serial.print(response[responsePointer-1]);
        if (strstr(response, AT_HTTP_RESPONSE_COMPLETE) != NULL) {
          done = true;
        }
      }
      if (millis() - startTime > HTTP_TIMEOUT) {
        timeout = true;
      }

    }
    Serial.println(response);
    if (!timeout) {
      String responseString = String(response);
      int startPos = responseString.indexOf("{");
      int endPos = responseString.indexOf("}") + 1;

      String payload = responseString.substring(startPos, endPos);

      Serial.println(payload);
    } else {
      logln("Timeout");
      send(ESC);
    }
    phonePowerOff();
    previousSendMillis = ms();
    readings[0] = readings[readingsSize - 1];
    readingsSize = 1;
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
  memset(response, END_OF_STRING, bufferSize);
  uint8_t bufferPointer=0;
  unsigned long startMillis;
  
  clearSerialBuffer();

  sendLn(command);
  startMillis = millis();

  do {
    if (Serial1.available() != 0) {    
      response[bufferPointer++] = Serial1.read();
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
  Serial1.print(text);
}

void send(byte b) {
  Serial1.print(b);
}

void sendLn(char* text) {
  Serial1.println(text);
}

void sendln(String text) {
  Serial1.println(text);
}

void clearSerialBuffer() {
  while(Serial1.available() > 0) {
    Serial1.read();
  }  
}

void logConfiguration() {
  logln(F("logConfiguration()- Start"));
  pushLogLevel();
  log(F("SERIAL_TX_BUFFER_SIZE: "));
  logaddln(SERIAL_TX_BUFFER_SIZE);
  log(F("SERIAL_RX_BUFFER_SIZE: "));
  logaddln(SERIAL_RX_BUFFER_SIZE);
  
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

void logln(long message) {
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

void logaddln(long message) {
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

