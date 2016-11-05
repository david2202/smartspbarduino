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
#define AT_POWER_OFF ("AT+CPOF")

#define OK ("OK")
#define CRLF ('\n')
#define SPACE (" ")

#define MANUFACTURER ("Manufacturer: ")
#define MODEL ("Model: ")
#define REVISION ("Revision: ")
#define IMEI ("IMEI: ")

struct PhoneConfig {
  String manufacturer;
  String model;
  String revision;
  String imei;
};

struct Configuration {
  char remoteUrlBase[40];
  period_t sleepTime;
  unsigned int remoteSendMillis;
};

SoftwareSerial phoneSerial(RX_PIN, TX_PIN);

struct PhoneConfig phoneConfig = {"", "", "", ""};

struct Configuration configuration;

uint8_t indent = 0;
unsigned long cumulativeSleepMillis = 0;
unsigned long previousMillis;

void setup() {
  Serial.begin(115200);
  logln(F("setup()- Start"));
  pushLogLevel();
  //strcpy(configuration.remoteUrlBase, "http://localhost:8080");
  //configuration.remoteSendMillis = 60000;
  //configuration.sleepTime = SLEEP_8S;
  //EEPROM.writeBlock(0, configuration);

  logln(F("Reading configuration"));
  EEPROM.readBlock(0, configuration);

  log(F("remoteBaseURL: "));
  logaddln(configuration.remoteUrlBase);
  log(F("remoteSendMillis: "));
  logaddln(configuration.remoteSendMillis);

  phoneSerial.begin(PHONE_BAUD);
  
  phonePowerOn();
  
  PhoneConfig config = phoneConfiguration();

  // Send a message that we are online

  phonePowerOff();

  previousMillis = millis();
  
  popLogLevel();
  logln(F("setup()- End"));

  delay(500);
}

void loop() {
  // Check the scales

  LowPower.powerDown(configuration.sleepTime, ADC_OFF, BOD_OFF);

  // Timer stops while we are asleep, so we need to keep track of it
  cumulativeSleepMillis += 8000;

  if (isRemoteSendTime()) {
    sendRemote();
    // Wait for things to finish
    delay(500);
  }
}

boolean isRemoteSendTime() {
  return ms() - previousMillis > configuration.remoteSendMillis;
}

boolean sendRemote() {
  logln(F("sendRemote()- Start"));
  pushLogLevel();
  boolean returnValue;
  if (phonePowerOn()) {
    // Send the data
    log(F("Sending IMEI "));
    logaddln(phoneConfig.imei);
    phonePowerOff();
    previousMillis = ms();
    returnValue = true;
  } else {
    returnValue = false;
  }
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
  popLogLevel();
  logln(F("phonePowerOn() - End"));
  return true;
}

boolean phonePowerOff() {
  logln(F("phonePowerOff() - Start"));
  pushLogLevel();
  boolean result = sendATcommand(AT_POWER_OFF, OK, 2000, 20);
  popLogLevel();
  logln(F("phonePowerOff() - End"));  
  return result;
}

struct PhoneConfig phoneConfiguration() {
  logln(F("phoneConfiguration() - Start"));
  pushLogLevel();

  logln(F("Getting phone configuration"));
  String config = sendATCommand(AT_IDENTIFY, 200, 250);
  if (config.length() == 0) {
    logln(F("Timed out"));
  } else {
    phoneConfig.manufacturer = extractConfigItem(config, MANUFACTURER);
    phoneConfig.model = extractConfigItem(config, MODEL);
    phoneConfig.revision = extractConfigItem(config, REVISION);
    phoneConfig.imei = extractConfigItem(config, IMEI);

    logln(MANUFACTURER + phoneConfig.manufacturer);
    logln(MODEL + phoneConfig.model);
    logln(REVISION + phoneConfig.revision);
    logln(IMEI + phoneConfig.imei);
  }
  
  popLogLevel();
  logln(F("phoneConfiguration() - End"));
  return phoneConfig;
}

String extractConfigItem(String config, char* parameter) {
  int parameterPos = config.indexOf(parameter);
  int startPos = parameterPos + strlen(parameter);
  int newLinePos = config.indexOf(CRLF, startPos + 1);
  String parameterValue = config.substring(startPos, newLinePos - 1);

  return parameterValue;
}

boolean sendATcommand(char* ATcommand, char* expected_answer1, unsigned int timeout, uint8_t bufferSize) {
  String response = sendATCommand(ATcommand, timeout, bufferSize);
  if (response.length() == 0) {
    return false;
  } else {
    return true;
  }
  /*
  uint8_t x = 0;
  char response[100];
  unsigned long startMillis;

  memset(response, END_OF_STRING, 100);

  delay(100);

  clearSerialBuffer();
  
  phoneSerial.println(ATcommand);

  x = 0;
  startMillis = millis();

  do {
    if (phoneSerial.available() != 0) {    
      response[x++] = phoneSerial.read();
      // check if the desired answer is in the response of the module
      if (strstr(response, expected_answer1) != NULL) {
        return true;
      }
    }
  } while((millis() - startMillis) < timeout);    
  return false;
  */
}

String sendATCommand(char* command, unsigned int timeout, uint8_t bufferSize) {
  char response[bufferSize];
  memset(response, END_OF_STRING, bufferSize);
  uint8_t bufferPointer=0;
  unsigned long startMillis;
  
  clearSerialBuffer();

  phoneSerial.println(command);

  startMillis = millis();

  do {
    if (phoneSerial.available() != 0) {    
      response[bufferPointer++] = phoneSerial.read();
      // check if the desired answer is in the response of the module
      if (strstr(response, OK) != NULL) {
        return String(response);
      }
    }
  } while((millis() - startMillis) < timeout);    
  response[0] = END_OF_STRING;
  return String(response);
}

void clearSerialBuffer() {
  while(phoneSerial.available() > 0) {
    phoneSerial.read();    // Clean the input buffer
  }  
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

