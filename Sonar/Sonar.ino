const byte LED = 13;
const byte SONAR_TRIG = 4;
const byte SONAR_ECHO = 5;

void setup() {
  Serial.begin (9600);
  pinMode(SONAR_TRIG, OUTPUT);
  pinMode(SONAR_ECHO, INPUT);
  pinMode(13, OUTPUT);
  pinMode(2, OUTPUT);
  pinMode(3, OUTPUT);
}

void loop() {
  digitalWrite(SONAR_TRIG, LOW); 
  delayMicroseconds(2); 
  digitalWrite(SONAR_TRIG, HIGH);
  delayMicroseconds(10); 
  digitalWrite(SONAR_TRIG, LOW);

  long duration = pulseIn(SONAR_ECHO, HIGH);
  long distance = (long)((double)duration / 58.2 + 0.5);
  
  Serial.println(distance);
  if (distance < 25){
    digitalWrite(LED, HIGH); 
    digitalWrite(2, LOW);
    digitalWrite(3, HIGH);
  } else {
    digitalWrite(LED, LOW); 
    digitalWrite(2, HIGH);
    digitalWrite(3, LOW);
  }
  
  delay(1000);
}
