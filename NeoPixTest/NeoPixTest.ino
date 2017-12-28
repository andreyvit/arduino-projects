#define ENABLE_WIFI 0
#define ENABLE_SONAR 0
#define ENABLE_BLUE_TRACE 0

#include "config.h"

#if ENABLE_WIFI 
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

//#include <ESP8266WiFi.h>          //ESP8266 Core WiFi Library (you most likely already have this in your sketch)
//#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
//#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
//#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#endif

#include <NeoPixelBus.h>
#if ENABLE_SONAR
#include <ESP8266Ultrasonic.h>
#endif
#include "timer.h"

enum {
  PIN_LED_STRIP = 2,
  PIN_SONAR_TRIG = 15, //D8,
  PIN_SONAR_ECHO = 13, //D7,
};

const uint16_t PixelCount = 288;

#define colorSaturation 255

NeoPixelBus<NeoGrbFeature, NeoEsp8266Dma800KbpsMethod> strip(PixelCount); // GPI02 aka D4
#if ENABLE_SONAR
ESP8266Ultrasonic sonar(PIN_SONAR_TRIG, PIN_SONAR_ECHO);
#endif

RgbColor red(colorSaturation, 0, 0);
RgbColor green(0, colorSaturation, 0);
RgbColor yellow(colorSaturation, colorSaturation, 0);
RgbColor blue(0, 0, colorSaturation);
RgbColor white(colorSaturation);
RgbColor black(0);

RgbColor colors[] = {white, red, yellow, blue, green};

HslColor hslRed(red);
HslColor hslGreen(green);
HslColor hslBlue(blue);
HslColor hslWhite(white);
HslColor hslBlack(black);

Timer<150> rotate_timer;
Timer<500> watchdog_timer;

#if ENABLE_BLUE_TRACE
int blue_trace_pos = 150;
Timer<200> blue_trace_timer;
#endif

#if ENABLE_WIFI 
//WiFiManager wifiManager;
#endif

struct Mover {
  RgbColor color;
  int pos;
  int dir;
  int step_ms;
  bool bounce;

  FlexTimer timer_;

  void start() {
    timer_.restart(step_ms);
  }

  void draw() {
    strip.SetPixelColor(pos, color);
  }

  void move(unsigned long now) {
    if (timer_.fired(now)) {
      step();
    }
  }

  void step() {
    if (bounce) {
      if (dir > 0) {
        if (pos == PixelCount - 1) {
          dir = -dir;
        }
      } else {
        if (pos == 0) {
          dir = -dir;
        }
      }
    }

    pos += dir;
    if (!bounce) {
      if (dir > 0) {
        pos = pos % PixelCount;
      } else {
        while (pos < 0) {
          pos += PixelCount;
        }
      }
    }
  }
};

Mover movers[] = {
  {red,               0, +1, 20, true},
  {green,  PixelCount-1, -1, 10, true},
  {blue,            100, +1, 30, false},
  {red,              10, +1,  2, true},
  {green, PixelCount-11, +1,  8, true},
  {blue,            150, -1,  1, false},
};

void setup()
{
  Serial.begin(115200);
  //while (!Serial); // wait for serial attach
  Serial.println("Booting");

#if ENABLE_WIFI 
//  {
//    String ssid = "LEDs " + String(ESP.getChipId());
//    wifiManager.autoConnect(ssid.c_str(), NULL);
//  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(kWifiNetwork, kWifiPassword);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
#endif
  
  pinMode(LED_BUILTIN, OUTPUT);
  

    Serial.println();
    Serial.println("Initializing...");
    Serial.flush();

    // this resets all the neopixels to an off state
    strip.Begin();
    strip.Show();

    Serial.println();
    Serial.println("Running...");

    //strip.SetPixelColor(0, red);
    //strip.SetPixelColor(1, green);
    //strip.SetPixelColor(2, blue);

    rotate_timer.start();
    watchdog_timer.start();
#if ENABLE_BLUE_TRACE
    blue_trace_timer.start();
  #endif

    for (int i = 0; i < sizeof(movers)/sizeof(movers[0]); i++) {
      movers[i].start();
    }
}

bool state = false;
void loop()
{
  unsigned long now = millis();

#if ENABLE_SONAR
  if (sonar.update()) {
    //Serial.println("Got sonar update.");
  }

  const int num_leds = PixelCount-2;
  const int cm_cutoff = 30;

  int leds = num_leds - (sonar.distance_mm() / 10 /* mm to cm */ * num_leds / cm_cutoff);
  int brightness = colorSaturation - (sonar.distance_mm() / 10 /* mm to cm */ * colorSaturation / cm_cutoff);
  if (leds < 0) {
    leds = 0;
  }
  if (brightness < 0) brightness = 0;
  if (brightness > colorSaturation) brightness = colorSaturation;

  strip.SetPixelColor(0, red);
  RgbColor color (0, brightness, 0);
  for (int i = 1; i <= num_leds; i++) {
    //strip.SetPixelColor(i, (i <= leds ? color : black));
    strip.SetPixelColor(i, color);
  }
  strip.SetPixelColor(num_leds+1, blue);
#else
  for (int i = 0; i < PixelCount; i++) {
    strip.SetPixelColor(i, black);
  }
  for (int i = 0; i < sizeof(movers)/sizeof(movers[0]); i++) {
    movers[i].draw();
    movers[i].move(now);
  }
  //strip.SetPixelColor(0, red);
  //strip.SetPixelColor(PixelCount-1, green);
#endif

#if ENABLE_BLUE_TRACE
  if (blue_trace_timer.fired(now)) {
    blue_trace_pos = (blue_trace_pos + 1) % PixelCount;
    Serial.printf("blue at %d\n", blue_trace_pos);
  }
  strip.SetPixelColor(blue_trace_pos, blue);
#endif
  
  strip.Show();

  if (rotate_timer.fired(now)) {
    //strip.RotateLeft(1);  
  }
  if (watchdog_timer.fired(now)) {
//    Serial.println("Alive.");
    digitalWrite(LED_BUILTIN, (state ? HIGH: LOW));
    state = !state;
  }
}

