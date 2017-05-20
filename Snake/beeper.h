#ifndef ANDREYVIT_BEEP_H
#define ANDREYVIT_BEEP_H

#include "timer.h"

template <int pin, unsigned long std_dur = 10>
class Beeper {
public:
    Beeper() {
        reset();
    }

    void reset() {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, HIGH);
        beep_off.stop();
    }

    void beep(unsigned long dur = std_dur) {
        digitalWrite(pin, LOW);
        beep_off.restart(dur);
    }

    void loop() {
        if (beep_off.fired(millis())) {
            digitalWrite(pin, HIGH);  
        }
    }

private:
    FlexDelay beep_off;
};

#endif // ANDREYVIT_BEEP_H
