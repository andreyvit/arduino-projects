#ifndef ANDREYVIT_TIMER_H
#define ANDREYVIT_TIMER_H

template <unsigned long invl>
class Delay {
public:
    Delay() : next(0) {
    }

    bool fired(unsigned long ms) {
        if (next == 0) {
            return false;
        } else if (ms >= next) {
            next = 0;
            return true;
        } else {
            return false;
        }
    }
    bool running() const {
        return next != 0;
    }
    void stop() {
        next = 0;
    }
    void start() {
        if (next == 0) {
            restart();
        }
    }
    void restart() {
        next = millis() + invl;
    }
private:
    unsigned long next;
};

class FlexDelay {
public:
    FlexDelay() : next(0) {
    }

    bool fired(unsigned long ms) {
        if (next == 0) {
            return false;
        } else if (ms >= next) {
            next = 0;
            return true;
        } else {
            return false;
        }
    }
    bool running() const {
        return next != 0;
    }
    void stop() {
        next = 0;
    }
    void restart(unsigned long invl) {
        next = millis() + invl;
    }
private:
    unsigned long next;
};

template <unsigned long invl>
class Timer {
public:
    Timer() : next(0) {
    }

    bool fired(unsigned long ms) {
        if (next == 0) {
            return false;
        } else if (ms >= next) {
            next = ms + invl;
            return true;
        } else {
            return false;
        }
    }
    bool running() const {
        return next != 0;
    }
    void stop() {
        next = 0;
    }
    void start() {
        if (next == 0) {
            restart();
        }
    }
    void restart() {
        next = millis() + invl;
    }
private:
    unsigned long next;
};

class FlexTimer {
public:
    FlexTimer() : next(0), invl(0) {
    }

    void set_invl(unsigned long new_invl) {
        invl = new_invl;
    }
    
    bool fired(unsigned long ms) {
        if (next == 0) {
            return false;
        } else if (ms >= next) {
            next = ms + invl;
            return true;
        } else {
            return false;
        }
    }
    bool running() const {
        return next != 0;
    }
    void stop() {
        next = 0;
    }
    void start() {
        if (next == 0) {
            restart();
        }
    }
    void restart() {
        if (invl > 0) {
            next = millis() + invl;
        } else {
            next = 0;
        }
    }
    void restart(unsigned int new_invl) {
        set_invl(new_invl);
        restart();
    }
private:
    unsigned long next;
    unsigned long invl;
};

template <unsigned long invl, int state_count = 2>
class Blinker {
public:
  Blinker() : value(0) {}

  void loop() {
    if (timer.fired(millis())) {
      ++value;
      if (value >= state_count) {
        value = 0;
      }
    }
  }

  void restart() {
    timer.restart();
  }

  int value;
  Timer<invl> timer;
};

#endif // ANDREYVIT_TIMER_H
