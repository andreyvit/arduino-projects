const byte PIN_RED = 4;
const byte PIN_YELLOW = 3;
const byte PIN_GREEN = 2;

const unsigned long SWITCH_INVL = 500;

#define LOHI(b) ((b) ? HIGH : LOW)

enum {
  C_RED,
  C_YELLOW,
  C_GREEN
};

byte color = C_RED;
unsigned long next_switch = 0;

void setup() {
  pinMode(PIN_RED, OUTPUT);
  pinMode(PIN_YELLOW, OUTPUT);
  pinMode(PIN_GREEN, OUTPUT);

  unsigned long now = millis();
  next_switch = now + SWITCH_INVL;
}

void loop() {
  unsigned long now = millis();
  
  digitalWrite(PIN_RED,    LOHI(color == C_RED));
  digitalWrite(PIN_YELLOW, LOHI(color == C_YELLOW));
  digitalWrite(PIN_GREEN,  LOHI(color == C_GREEN));

  if (now > next_switch) {
    color = (color + 1) % 3;
    next_switch = now + SWITCH_INVL;
  }
}

