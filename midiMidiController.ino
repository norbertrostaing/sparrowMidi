#include <FastLED.h>

#define DATA_PIN 8
#define NUM_LEDS 83
CRGB leds[NUM_LEDS];

int deadZone = 5;

int pinMultiplexA = 0;
int pinMultiplexB = 1;
int pinMultiplexC = 2;

int pinBtnA = 3;
int pinBtnB = 4;

int pinAnalogA = A0;

bool btnIsPressed[16];

int faderOrigin[8];
double faderValues[8];
bool faderIsMoving[8];
int sentFaderValues[8];

int faderChannels[] = {6, 1, 8, 2, 7, 4, 5, 3};
int buttonChannels[] = {7, 5, 8, 5, 8, 6, 7, 6, 3, 1, 4, 1, 4, 2, 3, 2};
int buttonNotes[] = {0, 1, 0, 0, 1, 0, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1};

int ledsByStrip = 10;
int firstLedNumber[] = {3,13,23,33,43,53,63,73};
bool invertLedStrip[] = {true, false, true, false, true, false, true, false};

int channelToFaderIndex[8];

bool ledsNeedRefresh = true;

void setup() {
  //Serial.begin(9600);

  FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS); 
  FastLED.setBrightness(50);
  for (int i=0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Black;
  }

  for (int i = 0; i< 16; i++) {
    btnIsPressed[i] = false;
  }

  for (int i = 0; i< 8; i++) {
    faderValues[i] = 0;
  }

  pinMode(pinMultiplexA, OUTPUT);
  pinMode(pinMultiplexB, OUTPUT);
  pinMode(pinMultiplexC, OUTPUT);
  pinMode(pinBtnA, INPUT_PULLUP);
  pinMode(pinBtnB, INPUT_PULLUP);
  pinMode(pinAnalogA, INPUT);

  for (int i = 0; i< 8; i++) {
    digitalWrite(pinMultiplexA, bitRead(i,0) == 1 ? HIGH : LOW);
    digitalWrite(pinMultiplexB, bitRead(i,1) == 1 ? HIGH : LOW);
    digitalWrite(pinMultiplexC, bitRead(i,2) == 1 ? HIGH : LOW);

    delay(10);
    faderOrigin[i] = analogRead(pinAnalogA);
    channelToFaderIndex[faderChannels[i]-1] = i;
    faderIsMoving[i] = false;
    displayFader(i+1,0);
  }

  usbMIDI.setHandlePitchChange(myPitchChange);
}

void loop() {

  bool isAFaderMoving = false;

  for (int i = 0; i< 8; i++) {
    digitalWrite(pinMultiplexA, bitRead(i,0) == 1 ? HIGH : LOW);
    digitalWrite(pinMultiplexB, bitRead(i,1) == 1 ? HIGH : LOW);
    digitalWrite(pinMultiplexC, bitRead(i,2) == 1 ? HIGH : LOW);
    
    // allow 50 us for signals to stablize
    delayMicroseconds(50);

    bool aPressed = digitalRead(pinBtnA) == LOW;
    bool bPressed = digitalRead(pinBtnB) == LOW;

    int indexA = i;
    int indexB = 8+i;

    if (aPressed && !btnIsPressed[indexA]) {
      btnIsPressed[indexA] = true;
      btnChanged(indexA, true);
    }
    if (bPressed && !btnIsPressed[indexB]) {
      btnIsPressed[indexB] = true;
      btnChanged(indexB, true);
    }

    if (!aPressed && btnIsPressed[indexA]) {
      btnIsPressed[indexA] = false;
      btnChanged(indexA, false);
    }
    if (!bPressed && btnIsPressed[indexB]) {
      btnIsPressed[indexB] = false;
      btnChanged(indexB, false);
    }

    int analogValA = analogRead(pinAnalogA);
    analogValA = analogValA-faderOrigin[i];
    if (abs(analogValA)>deadZone) {
      isAFaderMoving = true;
      double currentVal = faderValues[i];
      double newVal = currentVal;
      //Serial.println(currentVal);

      if (analogValA > 0) {
        analogValA -= deadZone;
        double joystickVal = analogValA/(512.0-deadZone);
        //joystickVal = pow(joystickVal, 1.2);
        double delta = joystickVal >0.95 ? 2000 : fmap(joystickVal, 0.0,1.0,0.01,200.0);
        newVal += delta;
        newVal = min(newVal,16383);
      } else {
        analogValA += deadZone;
        double joystickVal = analogValA/(512.0-deadZone);
        //joystickVal = -pow(abs(joystickVal), 1.2);
        double delta = joystickVal < -0.95 ? -2000 : fmap(joystickVal, 0.0,-1.0,-0.01,-200.0);
        newVal += delta;
        newVal = max(newVal,0);
      }
      faderValues[i] = newVal;
      if (round(newVal) != sentFaderValues[i]) {
        faderIsMoving[i] = true;
        faderChanged(i, round(newVal));
      }
    }
  }
  usbMIDI.read();
  if (!isAFaderMoving) {
    for(int i = 0; i< 8; i++) {
      if (faderIsMoving[i]) {
        faderIsMoving[i] = false;
        displayFader(faderChannels[i], faderValues[i]);
      }
    }
  }
  if (ledsNeedRefresh) {
    ledsNeedRefresh = false;
    FastLED.show();
  }
}

void btnChanged(int index, bool pressed) {
  int channel = buttonChannels[index];
  int note = buttonNotes[index];
  int velocity = pressed ? 127 : 0;
  usbMIDI.sendNoteOn(note, velocity, channel);
}

void faderChanged(int index, int newVal) {
  sentFaderValues[index] = newVal;
  int channel = faderChannels[index];
  int value = newVal;
  usbMIDI.sendPitchBend(value-8192, channel);
  displayFader(faderChannels[index], value);
}

double fmap(double x, double in_min, double in_max, double out_min, double out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void displayFader(int channel, int value) {
  int currentLed = firstLedNumber[channel-1];
  int deltaLed = 1;

  if (invertLedStrip[channel-1]) {
    currentLed += ledsByStrip-1;
    deltaLed = -1;
  }

  bool moving = faderIsMoving[channelToFaderIndex[channel-1]];

  float fValue = value / 16383.0;
  fValue *= ledsByStrip;

  int rOn = moving ? 64 : 32;
  int gOn = moving ? 64 : 32;
  int bOn = moving ? 64 : 32;
  
  int rOff = moving ? 64 : 32;
  int gOff = moving ? 0 : 0;
  int bOff = moving ? 0 : 0;

  int r = rOn;
  int g = gOn;
  int b = bOn;

  for (int i = 0; i< ledsByStrip; i++) {
    if (fValue < i) {
        r = rOff; g = gOff; b = bOff;
    } else if (fValue >= i+1) {
    } else {
      float ratio = fValue - floor(fValue);
      r = floor(fmap(ratio,0,1,rOff, rOn));
      g = floor(fmap(ratio,0,1,gOff, gOn));
      b = floor(fmap(ratio,0,1,bOff, bOn));
    }
    leds[currentLed].setRGB(r, g, b);
    currentLed += deltaLed;
  }
  ledsNeedRefresh = true;
}

void myPitchChange(byte channel, int pitch) {
  faderValues[channelToFaderIndex[channel-1]] = pitch+8192;
  sentFaderValues[channelToFaderIndex[channel-1]] = pitch+8192;
  displayFader(channel, pitch+8192);
}