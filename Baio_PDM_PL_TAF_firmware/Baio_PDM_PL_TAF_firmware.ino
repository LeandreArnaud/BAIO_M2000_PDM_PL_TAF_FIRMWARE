#include <Joystick.h>
#include "Adafruit_LEDBackpack.h"
#define DCSBIOS_DEFAULT_SERIAL
#include "DcsBios.h"

/********* PINs SETUP **********/
const int TAF_SDA = 2;
const int TAF_SCL = 3;
const int TAFlatchPin = 7;
const int TAFdataPin = 4;
const int TAFclockPin = 6;
const byte TAFbacklightPin = 5;

const int PDMlatchPin = 16;
const int PDMdataPin = 15;
const int PDMclockPin = 14;
const byte PDMbacklightPin = 10;

const int PLchan0 = 20;
const int PLchan1 = 19;
const int PLchan2 = 18;
const int PLadcIn = 8; // analog pin
const byte PLbacklightPin = 9;


/********* INIT VALUES AND CST **********/
byte TAFvalues = 255;
int TAFChanel = 1;
int oldTAFChanel = 0;
bool oldTAFEncoderVal = false;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 200;
bool lastTAFEncoderMoov = true; // true = increase

byte PLvalues[6] = {0, 0, 0, 0, 0, 0};
const int PLswitchNumber = 6;
const float PLmin = 100.0;
const float PLmax = 1000.0;
const int backlightOutMax = 255;
const byte PLthreshold = 10;

byte PDMvalues = 255;

Joystick_ Joystick(
  0x03, // id of the gamepad, icrease in other pads to avoid conflict
  0x05, //Gamepad
  12, // button
  0, // hat
  true, // X
  true, // Y
  true, // Z
  true, // Rx
  true, // Ry
  true, // Rz
  false, // rudder
  false, // throttle
  false, // rudder
  false, // rudder
  false // steering
);

Adafruit_LEDBackpack matrix = Adafruit_LEDBackpack();







/********* SETUP **********/
void setup() {
  DcsBios::setup();

  matrix.begin(0x70);  // pass in the address 70 or 74
  matrix.setBrightness(1);
  matrix.setDisplayState(true);

  pinMode(TAFbacklightPin, OUTPUT);
  pinMode(PDMbacklightPin, OUTPUT);
  pinMode(PLbacklightPin, OUTPUT);

  pinMode(TAFlatchPin, OUTPUT);
  pinMode(TAFclockPin, OUTPUT);
  pinMode(TAFdataPin, INPUT);

  pinMode(PLadcIn, INPUT_PULLUP);
  pinMode(PLchan0, OUTPUT);
  pinMode(PLchan1, OUTPUT);
  pinMode(PLchan2, OUTPUT);

  pinMode(PDMlatchPin, OUTPUT);
  pinMode(PDMclockPin, OUTPUT);
  pinMode(PDMdataPin, INPUT);

  Joystick.begin();
  Joystick.setXAxisRange(0, 255);
  Joystick.setYAxisRange(0, 255);
  Joystick.setZAxisRange(0, 255);
  Joystick.setRxAxisRange(0, 255);
  Joystick.setRyAxisRange(0, 255);
  Joystick.setRzAxisRange(0, 255);

  // Serial.begin(9600);

  // init digits
  for (uint8_t i=0; i<8; i++) {
    matrix.displaybuffer[i] = 0b0000000000000111; // buttons, digitR, digitL
    delay(100);
  }
  matrix.writeDisplay();
  analogWrite(TAFbacklightPin, 255);
  analogWrite(PDMbacklightPin, 255);
  analogWrite(PLbacklightPin, 255);

  delay(1000);
}






/********* DCS BIOS **********/
// backlight
void onConsolePanelLgtKnobChange(unsigned int newValue) {
    analogWrite(TAFbacklightPin, newValue/256);
    analogWrite(PDMbacklightPin, newValue/256);
    analogWrite(PLbacklightPin, newValue/256);
}
DcsBios::IntegerBuffer consolePanelLgtKnobChange(0x72a0, 0xffff, 0, onConsolePanelLgtKnobChange);
void onCautAdvLgtChange(unsigned int newValue) {
    matrix.setBrightness(newValue/4096);
}
DcsBios::IntegerBuffer cautAdvLgtBuffer(0x72a2, 0xffff, 0, onCautAdvLgtChange);

// TODO: adds TAF chanel selection linked to DCS BIOS






/********* LOOP **********/
void loop() {
  scanPL();
  scanTAF();
  scanPDM();

  if (TAFChanel != oldTAFChanel) {
    oldTAFChanel = TAFChanel;
    writeTAFDigits(TAFChanel, 1, 1);
  }

  detectEncoders();
}


void scanPDM() {
  digitalWrite(PDMlatchPin,1);
  delayMicroseconds(20);
  digitalWrite(PDMlatchPin,0);

  for (int i=7; i>=0; i--) {
    digitalWrite(PDMclockPin, 0);
    delayMicroseconds(2);
    bool value = digitalRead(PDMdataPin);
    
    // if value has changed
    if (value != bitRead(PDMvalues, i)) {
      bitWrite(PDMvalues, i, value);

      // Assign to controller
      // threatJoystickSpecialCases(i, value);
      Joystick.setButton(i, value);
    }

    digitalWrite(PDMclockPin, 1);
    delayMicroseconds(2);
  }
}



void scanTAF() {
  digitalWrite(TAFlatchPin,1);
  delayMicroseconds(20);
  digitalWrite(TAFlatchPin,0);

  for (int i=7; i>=0; i--) {
    digitalWrite(TAFclockPin, 0);
    delayMicroseconds(2);
    bool value = digitalRead(TAFdataPin);
    
    // if value has changed
    if (value != bitRead(TAFvalues, i)) {
      bitWrite(TAFvalues, i, value);

      // only NVG, test and press rotary
      switch (i) {
        case 0:
          Joystick.setButton(7, value);
          break;
        case 3:
          Joystick.setButton(8, value);
          break;
        case 4:
          Joystick.setButton(9, value);
          break;
        default:
          break;
      }
      // debug
      // Serial.println(i);
      // Serial.println(value);
      // Serial.println("----");
    }

    digitalWrite(TAFclockPin, 1);
    delayMicroseconds(2);
  }
}

void scanPL() {
  for (byte i=0; i<PLswitchNumber; i++) {
    selectPLChanel(i);
    delay(1);
    int value = analogRead(PLadcIn);

    // transform ~30 to ~1015 to O to 255
    float normalizedValue = (value-PLmin)/(PLmax - PLmin);
    normalizedValue = max(normalizedValue, 0.0);
    normalizedValue = min(normalizedValue, 1.0);
    int scaledInvertedValue = (1.0-normalizedValue)*backlightOutMax;
    
    // to avoid spaming
    if (
      abs(PLvalues[i] - scaledInvertedValue) > PLthreshold
      || (scaledInvertedValue == 0 && PLvalues[i] != 0)
      || (scaledInvertedValue == 255 && PLvalues[i] != 255)
    ) {
      PLvalues[i] = scaledInvertedValue;

      // if dcs bios is not connected
      if (!Serial.available()) {
        if (i == 2) {
          analogWrite(PDMbacklightPin, PLvalues[i]);
          analogWrite(PLbacklightPin, PLvalues[i]);
          analogWrite(TAFbacklightPin, PLvalues[i]);
        }
        if (i == 4) {
          matrix.setBrightness(16*PLvalues[i]/255);
          matrix.setDisplayState(PLvalues[i] != 0);
        }
      }
      

      changeJoyAxis(i);
    }
  }
}

void selectPLChanel(byte chan) {
  digitalWrite(PLchan0, bitRead(chan, 0));
  digitalWrite(PLchan1, bitRead(chan, 1));
  digitalWrite(PLchan2, bitRead(chan, 2));
}

void changeJoyAxis(int i) {
  switch (i) {
    case 0:
      Joystick.setXAxis(PLvalues[i]);
      break;
    case 1:
      Joystick.setYAxis(PLvalues[i]);
      break;
    case 2: // banq RE
      Joystick.setZAxis(PLvalues[i]);
      break;
    case 3:
      Joystick.setRxAxis(PLvalues[i]);
      break;
    case 4: // nuitJour
      Joystick.setRyAxis(PLvalues[i]);
      break;
    case 5: // blanc
      // this potar bug
      // Joystick.setRzAxis(PLvalues[i]);
      break;
  }
}


//            h hd bd b bg hg mdl dot
bool zero[] = {1, 1, 1, 1, 1, 1, 0, 0};
bool one[] = {0, 1, 1, 0, 0, 0, 0, 0};
bool two[] = {1, 1, 0, 1, 1, 0, 1, 0};
bool three[] = {1, 1, 1, 1, 0, 0, 1, 0};
bool four[] = {0, 1, 1, 0, 0, 1, 1, 0};
bool five[] = {1, 0, 1, 1, 0, 1, 1, 0};
bool six[] = {1, 0, 1, 1, 1, 1, 1, 0};
bool seven[] = {1, 1, 1, 0, 0, 0, 0, 0};
bool height[] = {1, 1, 1, 1, 1, 1, 1, 0};
bool nine[] = {1, 1, 1, 1, 0, 1, 1, 0};

bool getLedValueFromNumber(int number, int ledNumber) {
  switch (number) {
    case 0:
      return zero[ledNumber];
    case 1:
      return one[ledNumber];
    case 2:
      return two[ledNumber];
    case 3:
      return three[ledNumber];
    case 4:
      return four[ledNumber];
    case 5:
      return five[ledNumber];
    case 6:
      return six[ledNumber];
    case 7:
      return seven[ledNumber];
    case 8:
      return height[ledNumber];
    case 9:
      return nine[ledNumber];
  }
}

void writeTAFDigits(int N, bool evfLed, bool testLed) {
  for (uint8_t i=0; i<8; i++) {
    byte valToWrite = 0b0000000000000000;
    bitWrite(valToWrite, 0, getLedValueFromNumber(N/10, i));
    bitWrite(valToWrite, 1, getLedValueFromNumber(N%10, i));
    if (i == 0) {
      bitWrite(valToWrite, 2, evfLed);
    }
    if (i == 1) {
      bitWrite(valToWrite, 2, testLed);
    }

    matrix.displaybuffer[i] = valToWrite;
    delay(10);
  }
  matrix.writeDisplay();
}
  
  

void detectEncoders() {
  // quand bit 1 fait un bas vers haut
  bool tafEncoderVal = bitRead(TAFvalues, 1);
  if (tafEncoderVal != oldTAFEncoderVal) {
    oldTAFEncoderVal = tafEncoderVal;
    if (tafEncoderVal) {
      // Serial.print("front montant");

      // // on check bit 2 pour avoir le sens
      bool sens = bitRead(TAFvalues, 2);
      // Serial.println(sens);
      // Serial.println(lastTAFEncoderMoov);
      // Serial.println(millis() - lastDebounceTime);
      // Serial.println(millis());

      // si le sens n'a pas changé => OK + reinitialise debounce
      if (sens == lastTAFEncoderMoov) {
        if (sens) {
          incrementTAFChannel();
        } else {
          decrementTAFChannel();
        }
        lastDebounceTime = millis();
      // sinon on check le débounce + reinitialise debounce
      } else if (abs(millis() - lastDebounceTime) > debounceDelay) {
        if (sens) {
          incrementTAFChannel();
        } else {
          decrementTAFChannel();
        }
        lastTAFEncoderMoov = sens;
        lastDebounceTime = millis();
      }

    }
  }
}

void incrementTAFChannel() {
  if (TAFChanel == 20) {
    TAFChanel = 1;
  } else {
    TAFChanel++;
  }
  Joystick.setButton(10, true);
  delay(100);
  Joystick.setButton(10, false);
}

void decrementTAFChannel() {
  if (TAFChanel == 1) {
    TAFChanel = 20;
  } else {
    TAFChanel--;
  }
  Joystick.setButton(11, true);
  delay(100);
  Joystick.setButton(11, false);
}


