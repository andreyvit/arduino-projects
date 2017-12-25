#include "ESP8266Ultrasonic.h"

enum {
    COOLDOWN_INTERVAL_SINCE_FALL_USEC = 250000, // arbitrarily chosen
    COOLDOWN_INTERVAL_SINCE_RISE_USEC = 250000, // ‘over 60ms’ recommended
    PULSE_DURATION_USEC = 10,
};

static volatile uint32_t rise_usec = 0, fall_usec = 0;

static unsigned long mm_to_usec(int mm) {
    // sounds travels at 340 000 mm/sec, roundtrip covers double the distance, so the result is (mm / (340/2) * 1 000)
    return (unsigned long)mm * 1000 / 170;
}
static int usec_to_mm(unsigned long usec) {
    return usec * 170 / 1000;
}

ESP8266Ultrasonic::ESP8266Ultrasonic(byte trigger_pin, byte echo_pin)
    : trigger_pin_(trigger_pin), echo_pin_(echo_pin),
    measuring_(false), start_usec_(0), distance_mm_(0)
{
    pinMode(trigger_pin, OUTPUT);
    pinMode(echo_pin, INPUT);
    attachInterrupt(digitalPinToInterrupt(echo_pin_), echo_interrupt, CHANGE);
}

bool ESP8266Ultrasonic::update() {
    bool updated = false;
    if (measuring_) {
        if (rise_usec != 0 && fall_usec != 0) {
            unsigned long pulse_usec = fall_usec - rise_usec;
            distance_mm_ = usec_to_mm(pulse_usec);
            // Serial.printf("Got pulse %ul usec, distance %d mm\n", pulse_usec, distance_mm_);
            measuring_ = false;
            updated = true;
        }
    } else {
        unsigned long now = micros();
        if ((fall_usec == 0 || (now - fall_usec >= COOLDOWN_INTERVAL_SINCE_FALL_USEC))
                && (rise_usec == 0 || (now - rise_usec >= COOLDOWN_INTERVAL_SINCE_RISE_USEC))) {
            send_pulse();
        }
    }
    // bool locating = (HIGH == digitalRead(echo_pin_));
    // unsigned long now = micros();
    // if (locating) {
    //     if (start_usec_ == 0) {
    //         // Serial.println("Echo start...");
    //         start_usec_ = now;
    //     }
    // } else {
    //     if (measuring_) {
    //         if (start_usec_ != 0) {
    //             unsigned long pulse_len_usec = micros() - start_usec_;
    //             distance_mm_ = usec_to_mm(pulse_len_usec);
    //             Serial.printf("Got pulse %ul usec, distance %d mm\n", pulse_len_usec, distance_mm_);
    //             updated = true;
    //         }
    //         measuring_ = false;
    //         start_usec_ = now; // start cooldown
    //     } else {
    //         if (start_usec_ == 0 || now - start_usec_ >= COOLDOWN_INTERVAL_USEC) {
    //             send_pulse();
    //             start_usec_ = 0;
    //             measuring_ = true;
    //         }
    //     }
    // }
    return updated;
}

int ESP8266Ultrasonic::distance_mm() const {
    return distance_mm_;
}

void ESP8266Ultrasonic::send_pulse() {
    measuring_ = true;
    rise_usec = 0;
    fall_usec = 0;

    // Serial.println("Sending sonar pulse...");
    digitalWrite(trigger_pin_, LOW);
    delayMicroseconds(4);
    digitalWrite(trigger_pin_, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigger_pin_, LOW);
}

void ESP8266Ultrasonic::echo_interrupt() {
    if (rise_usec == 0) {
        rise_usec = micros();
    } else if (fall_usec == 0) {
        fall_usec = micros();
    }
}
