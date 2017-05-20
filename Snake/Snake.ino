#include <Arduino.h>
#include <TM1637Display.h>
#include <LedControl.h>
#include <TFT.h> // Hardware-specific library
#include <SPI.h>
#include "debounce.h"
#include "beeper.h"
#include "timer.h"

enum {
  PIN_CLK = 2,
  PIN_DIO = 3,
  PIN_JX = A0,
  PIN_JY = A1,
  PIN_JB = 4,
  PIN_ARROW_SENS = 200,
  PIN_SND = 5,
  PIN_LED_CS = 6,
  PIN_LED_CLK = 7,
  PIN_LED_DIN = 8,
};

Beeper<PIN_SND> beeper;


enum Mode {
  MODE_WELCOME,
  MODE_SNAKE,
};
byte mode = 0xFF;

enum Game {
  GAME_SNAKE_SIMPLE,
  GAME_SNAKE_COMPLEX,
};
const Game GAME_LAST = GAME_SNAKE_COMPLEX;
byte game;

TM1637Display display(PIN_CLK, PIN_DIO);
LedControl led(PIN_LED_DIN, PIN_LED_CLK, PIN_LED_CS, 1);

int n = 0;

enum { ROWS = 8, COLS = 8 };
#define COORD unsigned char
const COORD COORD_NONE = 0xFF;
COORD Coord(int x, int y) {
  return ((unsigned)x << 4) | ((unsigned)y);
}
int CoordX(COORD c) {
  return c >> 4;
}
int CoordY(COORD c) {
  return c & 0xF;
}

int jx, jy;
unsigned long jxms, jyms;
const int J_DELTA = 20;
const int J_UPD = 500;
Debounce<10> jb_dbnc;
Debounce<10> jr_dbnc, jl_dbnc;

uint8_t segs[] = { 0xff, 0xff, 0xff, 0xff };
int pos;

const int MAX_LEN = 10;
COORD coords[MAX_LEN];
int coord_s, coord_e;
int len;
int target_len = 2;
inline int INCIDX(int i) { return (i + 1) % MAX_LEN; }
inline int DECIDX(int i) { return (i == 0 ? MAX_LEN - 1 : i - 1); }

COORD apple = COORD_NONE;
bool apple_vis;
unsigned long apple_blink_ms;
unsigned long apple_appear_ms;
unsigned long apple_disappear_ms;
const unsigned long APPLE_BLINK_INVL = 100;
const unsigned long APPLE_APPEAR_INVL = 200;
const unsigned long APPLE_DISAPPEAR_INVL_SIMPLE = 15000;
const unsigned long APPLE_DISAPPEAR_INVL_COMPLEX = 7000;

const unsigned long BEEP_APPLE_APPEAR = 2;
const unsigned long BEEP_APPLE_DISAPPEAR = 5;
const unsigned long BEEP_APPLE_EATEN = 20;

int score = 0;

bool in_snake(COORD c) {
  for (int i = 0; i < MAX_LEN; i++) {
    if (coords[i] == c) {
      return true;
    }
  }
  return false;
}

void add(COORD c) {
  if (len == MAX_LEN) {
    return;
  }
  coords[coord_e] = c;
  led.setLed(0, CoordX(c), CoordY(c), true);
  coord_e = INCIDX(coord_e);
  ++len;
}

void remove() {
  if (len == 0) {
    return;
  }
  COORD c = coords[coord_s];
  coords[coord_s] = COORD_NONE;
  led.setLed(0, CoordX(c), CoordY(c), in_snake(c));
  coord_s = INCIDX(coord_s);
  --len;
}

COORD get_head() {
  if (len == 0) {
    return COORD_NONE;
  }
  return coords[DECIDX(coord_e)];
}

void submove(COORD c) {
  bool eaten = false;
  if (c == apple) {
    beeper.beep(BEEP_APPLE_EATEN); 
    score += 1;
    remove_apple();
    eaten = true;
  }

  if (len >= target_len && (!eaten || len == MAX_LEN)) {
    remove();
  }
  add(c);
}

int sgn(int v) {
  return (v > 0) - (v < 0);
}

void move(COORD c) {
  COORD head = get_head();
  if (c == head) {
    return;
  }

  int hx = CoordX(head), hy = CoordY(head);
  int x = CoordX(c), y = CoordY(c);
  int dx = sgn(x - hx), dy = sgn(y - hy);

  while (dx != 0 || dy != 0) {
    if (dx != 0) {
      hx += dx;
      submove(Coord(hx, hy));
    }
    if (dy != 0) {
      hy += dy;
      submove(Coord(hx, hy));
    }
    if (hx == x) {
      dx = 0;
    }
    if (hy == y) {
      dy = 0;
    }
  }
}

void redraw_snake_field() {
  for (int y = 0; y < ROWS; ++y) {
    for (int x = 0; x < COLS; ++x) {
      led.setLed(0, x, y, in_snake(Coord(x, y)));
    }
  }
}

void draw_apple() {
  if (apple == COORD_NONE) {
    return;
  }
  led.setLed(0, CoordX(apple), CoordY(apple), apple_vis);
}
void erase_apple() {
  if (apple == COORD_NONE) {
    return;
  }
  led.setLed(0, CoordX(apple), CoordY(apple), in_snake(apple));
}
void place_apple() {
  if (apple != COORD_NONE) {
    erase_apple();
  }

  do {
    int x, y;
    if (game == GAME_SNAKE_COMPLEX) {
      x = random(COLS);
      y = random(ROWS);
    } else {
      if (random(2) == 0) {
        y = random(ROWS);
        x = (random(2) == 0 ? 0 : COLS-1);
      } else {
        x = random(COLS);
        y = (random(2) == 0 ? 0 : ROWS-1);
      }
    }
    apple = Coord(x, y);
  } while (in_snake(apple));

  apple_vis = true;
  apple_blink_ms = 0;
  draw_apple();
  apple_disappear_ms = millis() + (game == GAME_SNAKE_COMPLEX ? APPLE_DISAPPEAR_INVL_COMPLEX : APPLE_DISAPPEAR_INVL_SIMPLE);
}
void remove_apple() {
  erase_apple();
  apple = COORD_NONE;
  apple_appear_ms = millis() + APPLE_APPEAR_INVL;
}

void setup()
{
  display.setBrightness(0xf);

  uint8_t data[] = { 0xff, 0xff, 0xff, 0xff };
  display.setSegments(data);

  pinMode(PIN_JX, INPUT);
  pinMode(PIN_JY, INPUT);
  pinMode(PIN_JB, INPUT_PULLUP);
  pinMode(PIN_SND, OUTPUT);
  digitalWrite(PIN_SND, HIGH);  

  led.shutdown(0, false);
  led.setIntensity(0, 0xf);
  led.clearDisplay(0);

  place_apple();

  for (int i = 0; i < MAX_LEN; i++) {
    coords[i] = COORD_NONE;
  }

  set_mode(MODE_WELCOME);
}

void set_mode(byte m) {
  if (m == mode) {
    return;
  }
  mode = m;

  switch (mode) {
  case MODE_WELCOME:
    enter_welcome(); break;
  case MODE_SNAKE:
    enter_snake(); break;
  }
}

void loop()
{
  unsigned long ms = millis();
  beeper.loop();

  jb_dbnc.update(LOW == digitalRead(PIN_JB));

  switch (mode) {
  case MODE_WELCOME:
    loop_welcome(); break;
  case MODE_SNAKE:
    loop_snake(); break;
  }
}

const byte LED_SNAKE_SIMPLE[] = {
  B11111111,    B11111111,
  B10000001,    B10000001,
  B10000001,    B10000001,
  B10100001,    B10000001,
  B10100001,    B10000011,
  B10100001,    B10000001,
  B10000001,    B10000001,
  B11111111,    B11111111,
};

const byte LED_SNAKE_COMPLEX[] = {
  B11111111,    B11111111,
  B10000001,    B10000001,
  B10111001,    B10000001,
  B10100001,    B10000101,
  B10100001,    B10000001,
  B10101001,    B10001001,
  B10000001,    B10000001,
  B11111111,    B11111111,
};

Blinker<100> welcome_blinker;

void enter_welcome() {
  welcome_blinker.timer.start();
}

void draw_led(bool blink, byte *rows, int y, int count) {
  while (count--) {
    byte row = *rows++;
    byte mask = *rows++;
    if (blink) {
      row ^= mask;
    }
    led.setColumn(0, y++, row);
  }
}

void loop_welcome()
{
  if (jb_dbnc.rose()) {
    set_mode(MODE_SNAKE);
    return;
  }

  welcome_blinker.loop();

  int x = analogRead(PIN_JX);
  jl_dbnc.update(x < PIN_ARROW_SENS);
  jr_dbnc.update(x > 1023-PIN_ARROW_SENS);

  if (jl_dbnc.rose()) {
    game = (game == 0 ? GAME_LAST : game - 1);
  } else if (jr_dbnc.rose()) {
    game = (game >= GAME_LAST ? 0 : game + 1);
  }

  switch (game) {
  case GAME_SNAKE_SIMPLE:
    draw_led(welcome_blinker.value, LED_SNAKE_SIMPLE, 0, ROWS);
    break;
  case GAME_SNAKE_COMPLEX:
    draw_led(welcome_blinker.value, LED_SNAKE_COMPLEX, 0, ROWS);
    break;
  }
}

void enter_snake() {
  redraw_snake_field();
}

void loop_snake()
{
  unsigned long ms = millis();
  int x = analogRead(PIN_JX);
  int y = analogRead(PIN_JY);
  if (abs(jx - x) > J_DELTA || (ms - jxms) >= J_UPD) {
    jx = x;
    jxms = ms;
  }
  if (abs(jy - y) > J_DELTA || (ms - jyms) >= J_UPD) {
    jy = y;
    jyms = ms;
  }

  if (jb_dbnc.rose()) {
    set_mode(MODE_WELCOME);
    return;
  }

  pos = (jx * 5 / 1024);

#if 0
  for (int i = 0; i < 4; ++i) {
    segs[i] = 0;
  }
  const byte marker = 0b00111111;
  bool blink = false;
  //const byte marker = 0b00110110;
  if (pos == 2) {  
    segs[1] = 0b00000110 | (blink ? 0b10000000 : 0);
    segs[2] = 0b00110000;
  } else if (pos < 2) {
    segs[pos] = marker | (blink ? 0b01000000 : 0);
  } else {
    segs[pos-1] = marker | (blink ? 0b01000000 : 0);
  }

  display.setSegments(segs);
#else
  display.showNumberDec(score, true);
#endif

  COORD c = Coord((jx * 8 / 1024), (jy * 8 / 1024));
  move(c);

  if (apple == COORD_NONE) {
    if (apple_appear_ms == 0) {
      apple_appear_ms = ms + APPLE_APPEAR_INVL;
    } else if (ms >= apple_appear_ms) {
      beeper.beep(BEEP_APPLE_APPEAR);
      place_apple();
    }
  }
  if ((apple != COORD_NONE) && (apple_disappear_ms > 0) && (ms >= apple_disappear_ms)) {
    beeper.beep(BEEP_APPLE_DISAPPEAR);
    remove_apple();
  }
  if ((apple != COORD_NONE) && (ms >= apple_blink_ms)) {
    if (apple_blink_ms > 0) {
      apple_vis = !apple_vis;
      draw_apple();
    }
    apple_blink_ms = ms + APPLE_BLINK_INVL;
  }
    
  //display.showNumberDec(jx, true);
  // n++;
}

