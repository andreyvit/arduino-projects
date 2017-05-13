#include <Arduino.h>
#include <Key.h>
#include <Keypad.h>
#include "TM1637.h"
#include "LedControl.h"

const byte F_ALL[8]  = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
const byte F_RHOMB[8] = {0x18, 0x24, 0x42, 0x81, 0x81, 0x42, 0x24, 0x18};
const byte F_RECT82[8] = {0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00};
const byte F_RECT84[8] = {0x00, 0x00, 0xFF, 0x81, 0x81, 0xFF, 0x00, 0x00};
const byte F_RECT86[8] = {0x00, 0xFF, 0x81, 0x81, 0x81, 0x81, 0xFF, 0x00};
const byte F_SQUARE8[8] = {0xFF, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0xFF};
const byte F_SMALL_RHOMB_SQUARE[8] = {0x07, 0x05, 0x07, 0x00, 0x00, 0x40, 0xA0, 0x40};
const byte F_SMALL_RHOMB_SQUARE_DBL[8] = {0x47, 0xA5, 0x47, 0x00, 0x00, 0x47, 0xA5, 0x47};
const byte F_CROSS_INV[8] = {0xE7, 0xE7, 0xE7, 0x00, 0x00, 0xE7, 0xE7, 0xE7};

const byte F_ARROW_UP[8] = {0x18, 0x3C, 0x5A, 0x99, 0x18, 0x18, 0x18, 0x18};
const byte F_ARROW_DN[8] = {0x18, 0x18, 0x18, 0x18, 0x99, 0x5A, 0x3C, 0x18};
const byte F_ARROW_LT[8] = {0x10, 0x20, 0x40, 0xFF, 0xFF, 0x40, 0x20, 0x10};
const byte F_ARROW_RT[8] = {0x08, 0x04, 0x02, 0xFF, 0xFF, 0x02, 0x04, 0x08};

const byte *FIGS[] = {F_RHOMB, F_SMALL_RHOMB_SQUARE_DBL, F_SQUARE8, F_RECT86, F_RECT84, F_RECT82};
const byte NFIGS = sizeof(FIGS) / sizeof(FIGS[0]);
byte fig = 0;

enum {
  A_NONE = 0,
  A_FWD  = 1,
  A_BCK  = 2,
  A_LT   = 3,
  A_RT   = 4
};

#define ROWS 4
#define COLS 3
char keys[ROWS][COLS] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'},
//  {'1', '2', '3', 'A'},
//  {'4', '5', '6', 'B'},
//  {'7', '8', '9', 'C'},
//  {'*', '0', '#', 'D'},
};
byte rowPins[ROWS] = { 12, 11, 10, 9 };
byte colPins[COLS] = { A0, A1, A2 };

// Create the Keypad
Keypad kpd = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );

const byte LED = 6;

const byte TOY_FWD = 3;
const byte TOY_BCK = 2;
const byte TOY_RT = 4;

const byte MAT_CLK = 2;
const byte MAT_CS = 3;
const byte MAT_DIN = 4;

boolean playback = false;
byte pgn_step = 0;
unsigned long playback_ms = 0;
unsigned long playback_speed = 2000;

#define BRIGHTNESS 4

TM1637 tm1637(A2, A0);  // CLK, DIO
unsigned number = 0;
int8_t digits[] = {0x00,0x00,0x00,0x00};

LedControl mat1 = LedControl(MAT_DIN, MAT_CLK, MAT_CS, 4);

byte action = A_NONE;

// the setup function runs once when you press reset or power the board
void setup() {
  // initialize digital pin 13 as an output.
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED, OUTPUT);
  pinMode(TOY_FWD, OUTPUT);
  pinMode(TOY_BCK, OUTPUT);
  pinMode(TOY_RT, OUTPUT);

  Serial.begin(9600);

  tm1637.set(BRIGHTNESS);
  tm1637.init();

  mat1.shutdown(0, false);
  mat1.setIntensity(0, 15);

  //mat1.setLed(0, 4, 4, true);
  showFigureForAction();

  displayState();
  writeAll();
}

// the loop function runs over and over again forever
void loop() {
  char key = kpd.getKey();
  if (key) {
    Serial.write(key);
  }
  switch (key) {
    case '5':
      manualAction(A_BCK);
      break;
    case '1':
      manualAction(A_RT);
      delay(1350);
      manualAction(A_NONE);
      break;
    case '0':
      manualAction(A_NONE);
      break;
    case '2':
      manualAction(A_FWD);
      break;
    case '4':
      manualAction(A_LT);
      break;
    case '6':
      manualAction(A_RT);
      break;
    case '*':
      if (playback) {
        cancelPlayback();
      } else {
        startPlayback();
      }
      displayState();
      break;
    case '#':
      fig = (fig + 1) % NFIGS;
      showSelectedFigure();
      break;
    default:
      Serial.println(key);
      break;
  }

  if (playback) {
    unsigned long ms = millis();
    if (ms >= playback_ms) {
      playback_ms = ms + playback_speed;
      nextPlaybackStep();
      displayState();
    }
  }
}

void manualAction(byte act) {
  cancelPlayback();
  action = act;
  writeAll();
  showFigureForAction();
  displayState();
}

void startPlayback() {
  playback = true;
  playback_ms = millis() + playback_speed;
  applyPlaybackStep();
}

void cancelPlayback() {
  playback = false;
  pgn_step = 0;
  action = A_NONE;
  writeAll();
  showFigureForAction();
}

void nextPlaybackStep() {
  pgn_step = (pgn_step + 1) % 16;
  applyPlaybackStep();
}

void applyPlaybackStep() {
  byte s = pgn_step % 4;
  if (s == 0 || s == 2) {
    action = A_NONE;
  } else if (s == 1) {
    action = A_FWD;
  } else {
    action = A_BCK;
  }
  writeAll();
  showFigureForAction();
}

void showSelectedFigure() {
  showFigure(FIGS[fig]);
}

void showFigureForAction() {
  if (action == A_NONE) {
    showFigure(F_SMALL_RHOMB_SQUARE_DBL);    
  } else if (action == A_FWD) {
    showFigure(F_ARROW_UP);
  } else if (action == A_BCK) {
    showFigure(F_ARROW_DN);
  } else if (action == A_LT) {
    showFigure(F_ARROW_LT);
  } else if (action == A_RT) {
    showFigure(F_ARROW_RT);
  }
  fig = 0;
}

void writeAll() {
  byte moving = (action != A_NONE);

  digitalWrite(TOY_FWD, (action == A_FWD ? LOW : HIGH));
  digitalWrite(TOY_BCK, (action == A_BCK ? LOW : HIGH));
  digitalWrite(TOY_RT, (action == A_RT ? LOW : HIGH));

  digitalWrite(LED_BUILTIN, (moving ? HIGH : LOW));
  digitalWrite(LED, (moving ? HIGH : LOW));
}

void displayState() {
  display(pgn_step, (playback ? 1 : 0), 0, action);
}

void display(byte d1, byte d2, byte d3, byte d4) {
  digits[3] = d1;
  digits[2] = d2;
  digits[1] = d3;
  digits[0] = d4;
  tm1637.display(digits);
}

void setNumber(unsigned n) {
  number = n;
  fillDigits(number);
  tm1637.display(digits);
}

void fillDigits(unsigned n) {
  digits[3] = n % 10;  n /= 10;
  digits[2] = n % 10;  n /= 10;
  digits[1] = n % 10;  n /= 10;
  digits[0] = n % 10;
}

void showFigure(const byte *figure) {
  for (byte d = 0; d < 2; ++d) {
    for (byte i = 0; i < 8; ++i) {
      mat1.setRow(d, i, figure[i]);
    }
  }
}

