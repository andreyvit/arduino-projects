#include <Arduino.h>
#include <TM1637Display.h>
#include <LedControl.h>

const byte PIN_CLK = 2;
const byte PIN_DIO = 3;
const byte PIN_JX = A0;
const byte PIN_JY = A1;
const byte PIN_JB = 4;
const byte PIN_SND = 5;
const byte PIN_LED_CS = 6;
const byte PIN_LED_CLK = 7;
const byte PIN_LED_DIN = 8;

// The amount of time (in milliseconds) between tests
#define TEST_DELAY   2000

const uint8_t SEG_DONE[] = {
  SEG_B | SEG_C | SEG_D | SEG_E | SEG_G,           // d
  SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,   // O
  SEG_C | SEG_E | SEG_G,                           // n
  SEG_A | SEG_D | SEG_E | SEG_F | SEG_G            // E
  };

TM1637Display display(PIN_CLK, PIN_DIO);
LedControl led(PIN_LED_DIN, PIN_LED_CLK, PIN_LED_CS, 1);

int n = 0;

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

uint8_t segs[] = { 0xff, 0xff, 0xff, 0xff };
int pos;

unsigned long beepms;
const unsigned long BEEP_INVL = 1000 * 2;
unsigned long beepoffms;

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
const unsigned long APPLE_DISAPPEAR_INVL = 15000;

const unsigned long BEEP_APPLE_APPEAR = 2;
const unsigned long BEEP_APPLE_DISAPPEAR = 5;
const unsigned long BEEP_APPLE_EATEN = 20;

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
    beep(BEEP_APPLE_EATEN);
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
    hx += dx;
    hy += dy;
    submove(Coord(hx, hy));
    if (hx == x) {
      dx = 0;
    }
    if (hy == y) {
      dy = 0;
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
    int x = random(8);
    int y = random(8);
    apple = Coord(x, y);
  } while (in_snake(apple));

  apple_vis = true;
  apple_blink_ms = 0;
  draw_apple();
  apple_disappear_ms = millis() + APPLE_DISAPPEAR_INVL;
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
}

void loop()
{
  //writeArduinoOnMatrix();
  
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
  int jb = digitalRead(PIN_JB);

  pos = (jx * 5 / 1024);

  for (int i = 0; i < 4; ++i) {
    segs[i] = 0;
  }
  const byte marker = 0b00111111;
  bool blink = (jb == LOW);
  //const byte marker = 0b00110110;
  if (pos == 2) {  
    segs[1] = 0b00000110 | (blink ? 0b10000000 : 0);
    segs[2] = 0b00110000;
  } else if (pos < 2) {
    segs[pos] = marker | (blink ? 0b01000000 : 0);
  } else {
    segs[pos-1] = marker | (blink ? 0b01000000 : 0);
  }

  if (beepoffms > 0 && ms >= beepoffms) {
    beepoffms = 0;
    digitalWrite(PIN_SND, HIGH);  
  }
  if (ms >= beepms) {
    // beepms = ms + BEEP_INVL;
    beepms = ms + 1000000;
    beep(10);
  } else if (jb == LOW) {
    analogWrite(PIN_SND, 253); 
  } else {
    digitalWrite(PIN_SND, HIGH);  
  }

  display.setSegments(segs);

  COORD c = Coord((jx * 8 / 1024), (jy * 8 / 1024));
  move(c);

  if (apple == COORD_NONE) {
    if (apple_appear_ms == 0) {
      apple_appear_ms = ms + APPLE_APPEAR_INVL;
    } else if (ms >= apple_appear_ms) {
      beep(BEEP_APPLE_APPEAR);
      place_apple();
    }
  }
  if ((apple != COORD_NONE) && (apple_disappear_ms > 0) && (ms >= apple_disappear_ms)) {
    beep(BEEP_APPLE_DISAPPEAR);
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

void beep_norm() {
  beep(10);
}
void beep(unsigned long dur) {
  digitalWrite(PIN_SND, LOW);
  beepoffms = millis() + dur;
}

void writeArduinoOnMatrix() {
  LedControl &lc = led;
  const unsigned long delaytime = 100;
  /* here is the data for the characters */
  byte a[5]={B01111110,B10001000,B10001000,B10001000,B01111110};
  byte r[5]={B00111110,B00010000,B00100000,B00100000,B00010000};
  byte d[5]={B00011100,B00100010,B00100010,B00010010,B11111110};
  byte u[5]={B00111100,B00000010,B00000010,B00000100,B00111110};
  byte i[5]={B00000000,B00100010,B10111110,B00000010,B00000000};
  byte n[5]={B00111110,B00010000,B00100000,B00100000,B00011110};
  byte o[5]={B00011100,B00100010,B00100010,B00100010,B00011100};

  /* now display them one by one with a small delay */
  lc.setRow(0,0,a[0]);
  lc.setRow(0,1,a[1]);
  lc.setRow(0,2,a[2]);
  lc.setRow(0,3,a[3]);
  lc.setRow(0,4,a[4]);
  delay(delaytime);
  lc.setRow(0,0,r[0]);
  lc.setRow(0,1,r[1]);
  lc.setRow(0,2,r[2]);
  lc.setRow(0,3,r[3]);
  lc.setRow(0,4,r[4]);
  delay(delaytime);
  lc.setRow(0,0,d[0]);
  lc.setRow(0,1,d[1]);
  lc.setRow(0,2,d[2]);
  lc.setRow(0,3,d[3]);
  lc.setRow(0,4,d[4]);
  delay(delaytime);
  lc.setRow(0,0,u[0]);
  lc.setRow(0,1,u[1]);
  lc.setRow(0,2,u[2]);
  lc.setRow(0,3,u[3]);
  lc.setRow(0,4,u[4]);
  delay(delaytime);
  lc.setRow(0,0,i[0]);
  lc.setRow(0,1,i[1]);
  lc.setRow(0,2,i[2]);
  lc.setRow(0,3,i[3]);
  lc.setRow(0,4,i[4]);
  delay(delaytime);
  lc.setRow(0,0,n[0]);
  lc.setRow(0,1,n[1]);
  lc.setRow(0,2,n[2]);
  lc.setRow(0,3,n[3]);
  lc.setRow(0,4,n[4]);
  delay(delaytime);
  lc.setRow(0,0,o[0]);
  lc.setRow(0,1,o[1]);
  lc.setRow(0,2,o[2]);
  lc.setRow(0,3,o[3]);
  lc.setRow(0,4,o[4]);
  delay(delaytime);
  lc.setRow(0,0,0);
  lc.setRow(0,1,0);
  lc.setRow(0,2,0);
  lc.setRow(0,3,0);
  lc.setRow(0,4,0);
  delay(delaytime);
}

