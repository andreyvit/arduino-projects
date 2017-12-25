#ifndef ESP8266ULTRASONIC_H
#define ESP8266ULTRASONIC_H

#include "Arduino.h"

class ESP8266Ultrasonic {
public:
    enum {
        MAX_DISTANCE_MM = 5000,
    };

    ESP8266Ultrasonic(byte trigger_pin, byte echo_pin);
    void set_max_distance_mm(int max_distance_mm);

    bool update();
    int distance_mm() const;


    int debug_value() const {
        if (measuring_) {
            if (start_usec_ > 0) {
                return 4;
            } else {
                return 3;
            }
        } else {
            if (start_usec_ > 0) {
                return 2;
            } else {
                return 1;
            }
        }
    }

private:
    byte trigger_pin_, echo_pin_;
    bool measuring_;
    unsigned long start_usec_;
    unsigned long timeout_usec_;
    
    int distance_mm_;

    void send_pulse();

    static void echo_interrupt();
};

#endif
