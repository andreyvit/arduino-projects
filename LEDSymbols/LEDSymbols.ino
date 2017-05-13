#include <Arduino.h>
#include <Key.h>
#include <Keypad.h>
#include "TM1637.h"

#define ROWS 2
#define COLS 3

char keys[ROWS][COLS] = {
  {'1', '2', '3'},
  //{'4','5','6'},
  //{'7','8','9'},
  {'*', '0', '#'}
};
// Connect keypad ROW0, ROW1, ROW2 and ROW3 to these Arduino pins.
byte rowPins[ROWS] = { 11, 10 };
// Connect keypad COL0, COL1 and COL2 to these Arduino pins.
byte colPins[COLS] = { 9, 8, 7 };

// Create the Keypad
Keypad kpd = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );

const byte LED = 6;

boolean on = true;
unsigned long blink_ms = 0;
unsigned long blink_speed = 200;

#define MODE_CONST 0
#define MODE_BLINK 1
int mode = MODE_BLINK;

#define BRIGHTNESS 4

TM1637 tm1637(A2, A0);  // CLK, DIO
unsigned number = 0;
int8_t digits[] = {0x00,0x00,0x00,0x00};

// the setup function runs once when you press reset or power the board
void setup() {
  // initialize digital pin 13 as an output.
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED, OUTPUT);
  Serial.begin(9600);
  tm1637.set(BRIGHTNESS);
  tm1637.init();
  setNumber(0);
}

// the loop function runs over and over again forever
void loop() {
  unsigned long ms = millis();
  char key = kpd.getKey();
  switch (key) {
    case '1':
      mode = MODE_BLINK;
      blink_ms = ms;
      blink_speed = 100;
      break;
    case '2':
      mode = MODE_BLINK;
      blink_ms = ms;
      blink_speed = 250;
      break;
    case '3':
      mode = MODE_BLINK;
      blink_ms = ms;
      blink_speed = 500;
      break;
    case '*':
      on = !on;
      mode = MODE_CONST;
      setNumber(number + 1);
      break;
    case '0':
      on = false;
      mode = MODE_CONST;
      break;
    case '#':
      setNumber(0);
      on = true;
      mode = MODE_CONST;
      break;
    case 0:
      break;
    default:
      Serial.println(key);
      break;
  }

  if (mode == MODE_BLINK) {
    if (ms >= blink_ms) {
      on = !on;
      blink_ms = ms + blink_speed;
      if (on) {
        setNumber(number + 1);
      }
    }
  }

  byte state = (on ? HIGH : LOW);
  digitalWrite(LED_BUILTIN, state);
  digitalWrite(LED, state);
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

