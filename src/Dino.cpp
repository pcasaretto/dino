/*
  Library for dino ruby gem.
*/

#include "Arduino.h"
#include "Dino.h"

Dino::Dino(){
  reset();
}

void Dino::parse(char c) {
  if (c == '!') index = 0;        // Reset request
  else if (c == '.') process();   // End request and process
  else request[index++] = c;      // Append to request
}

void Dino::process() {
  
  // Default response.
  response[0] = '\0';

  // Parse the request.
  strncpy(cmd, request, 2);         cmd[2] =    '\0';
  strncpy(pinStr, request + 2, 2);  pinStr[2] = '\0';
  strncpy(val, request + 4, 3);     val[3] =    '\0';
  
  // Serial.println(cmd);
  // Serial.println(pin);
  // Serial.println(val);
  // if (debug) Serial.println(request);
  
  convertPin();
  if (pin == -1) return; // Should raise some kind of "bad pin" error.
  
  int cmdid = atoi(cmd);
  switch(cmdid) {
    case 0:  setMode             ();  break;
    case 1:  dWrite              ();  break;
    case 2:  dRead               ();  break;
    case 3:  aWrite              ();  break;
    case 4:  aRead               ();  break;
    case 10: addDigitalListener  ();  break;
    case 11: addAnalogListener   ();  break;
    case 12: removeListener      ();  break;
    case 90: reset               ();  break;
    case 98: setHeartRate        ();  break;
    case 99: toggleDebug         ();  break;
    default:                          break;
  }
  
  if (response[0] != '\0') writeResponse();
}


void Dino::setupWrite(void (*writeCallback)(char *str)) {
  _writeCallback = writeCallback;
}
void Dino::writeResponse() {
  _writeCallback(response);
}


void Dino::updateListeners() {
  if (listenerCount > 0) {
    updateDigitalListeners();
    updateAnalogListeners();
  }
}

void Dino::updateDigitalListeners() {
  if (timeSince(lastDigitalUpdate) > 5 || timeSince(lastDigitalUpdate) < 0) {
    for (int i = 0; i < 22; i++) {
      if (digitalListeners[i]) {
        pin = i;
        dRead();
        if (rval != digitalListenerValues[i]) {
          digitalListenerValues[i] = rval;
          writeResponse();
        } 
      }
    }
    lastDigitalUpdate = millis();
  }
}

void Dino::updateAnalogListeners() {
  if (timeSince(lastAnalogUpdate) > heartRate || timeSince(lastAnalogUpdate) < 0) {  
    for (int i = 0; i < 8; i++) {
      if (analogListeners[i] != 0) {
        pin = analogListeners[i]; pinStr[0] = 'A';
        pinStr[1] = (char)(((int)'0')+i); pinStr[2] = '\0'; // Should make this suitable for > 9 analog pins.
        aRead();
        writeResponse();
      }
    }
    lastAnalogUpdate = millis();
  }
}

long Dino::timeSince(long event) {
 long time = millis() - event;
 return time;
}



// CMD = 00 // Pin Mode
void Dino::setMode() {
  if (atoi(val) == 0) {
    pinMode(pin, OUTPUT);
  } else {
    pinMode(pin, INPUT);
  }
}

// CMD = 01 // Digital Write
void Dino::dWrite() {
  removeListener();
  pinMode(pin, OUTPUT);
  if (atoi(val) == 0) {
    digitalWrite(pin, LOW);
  } else {
    digitalWrite(pin, HIGH);
  }
}

// CMD = 02 // Digital Read
void Dino::dRead() { 
  pinMode(pin, INPUT);
  rval = digitalRead(pin);
  if (analogPin) {
    sprintf(response, "%s::%02d", pinStr, rval);
  } else {
    sprintf(response, "%02d::%02d", pin, rval);
  }
}

// CMD = 03 // Analog (PWM) Write
void Dino::aWrite() {
  removeListener();
  pinMode(pin, OUTPUT);
  analogWrite(pin,atoi(val));
}

// CMD = 04 // Analog Read
void Dino::aRead() {
  pinMode(pin, INPUT);
  rval = analogRead(pin);
  sprintf(response, "%s::%03d", pinStr, rval);  // Send response with 'A0' formatting, not raw pin number, so pinStr not pin.
}


// CMD = 10
// Listen for a digital signal on any pin.
void Dino::addDigitalListener() {
  removeListener();
  digitalListeners[pin] = true;
  digitalListenerValues[pin] = 2;
  countListeners();
}

// CMD = 11
// Listen for an analog signal on analog pins only.
void Dino::addAnalogListener() {
  removeListener();
  if (analogPin) {
    int index = atoi(&pinStr[1]);
    analogListeners[index] = pin;
  }
  countListeners();
}

// CMD = 12
// Remove analog and digital listeners from any pin.
void Dino::removeListener() {
  if (analogPin) {
    int index = atoi(&pinStr[1]);
    analogListeners[index] = 0;
  }
  digitalListeners[pin] = false;
  countListeners();
}

void Dino::countListeners() {
  listenerCount = 0;
  for (int i = 0; i < 22; i++) {
    if (digitalListeners[i]) listenerCount++;
  }
  for (int i = 0; i < 8; i++) {
    if (analogListeners[i] != 0) listenerCount++;
  }
}


// CMD = 90
void Dino::reset() {
  debug = false;
  heartRate = 5; // Default heart rate is 5ms.
  for (int i = 0; i < 22; i++) digitalListeners[i] = false;
  for (int i = 0; i < 22; i++) digitalListenerValues[i] = 2;
  for (int i = 0; i < 8; i++)  analogListeners[i] = 0;
  listenerCount = 0;
  lastDigitalUpdate = millis();
  lastAnalogUpdate = millis();
  index = 0;
}

// CMD = 98
// Set the heart rate in milliseconds. Min = 5ms.
void Dino::setHeartRate() {
  int rate = atoi(val);
  if (rate < 5 && rate !=0) rate = 5;
  heartRate = rate;
}

// CMD = 99
void Dino::toggleDebug() {
  if (atoi(val) == 0) {
    debug = false;
    strcpy(response, "Debug 0");
  } else {
    debug = true;
    strcpy(response, "Debug 1");
  }
}



// Convert the pin received in stringy form to a raw pin as an integer.
// pin is -1 on error.
void Dino::convertPin() {
  pin = -1;
  if(pinStr[0] == 'A' || pinStr[0] == 'a') {
    analogPin = true;
    int analogPin0 = A0;
    pin = analogPin0 + atoi(&pinStr[1]);
  } else {
    analogPin = false;
    pin = atoi(pinStr);
    if(pin == 0 && (pinStr[0] != '0' || pinStr[1] != '0')) {
      pin = -1;
    }
  }
}