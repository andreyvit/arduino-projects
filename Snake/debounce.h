#ifndef DEBOUNCE_H
#define DEBOUNCE_H

enum DebounceMode {
  DEBOUNCE_STABLE,
  DEBOUNCE_LOCKOUT,
  DEBOUNCE_PROMPT,
};

template <uint16_t interval_millis, DebounceMode mode = DEBOUNCE_STABLE>
class Debounce {
  enum {
    DEBOUNCED_STATE = 0,
    UNSTABLE_STATE = 1,
    STATE_CHANGED = 3,
  };
public:
  Debounce() {
    reset(false);
  }
  void reset(bool value) {
    if (value) {
      state = _BV(DEBOUNCED_STATE) | _BV(UNSTABLE_STATE);
    } else {
      state = 0;
    }
    if (mode == DEBOUNCE_LOCKOUT) {
      previous_millis = 0;
    } else {
      previous_millis = millis();
    }
  }

  bool update(bool currentState) {
    if (mode == DEBOUNCE_LOCKOUT) {
      state &= ~_BV(STATE_CHANGED);
      // Ignore everything if we are locked out
      if (millis() - previous_millis >= interval_millis) {
        if ((bool)(state & _BV(DEBOUNCED_STATE)) != currentState) {
          previous_millis = millis();
          state ^= _BV(DEBOUNCED_STATE);
          state |= _BV(STATE_CHANGED);
        }
      }
      return state & _BV(STATE_CHANGED);

    } else if (mode == DEBOUNCE_PROMPT) {
      // Clear Changed State Flag - will be reset if we confirm a button state change.
      state &= ~_BV(STATE_CHANGED);

      if ( currentState != (bool)(state & _BV(DEBOUNCED_STATE))) {
        // We have seen a change from the current button state.

        if ( millis() - previous_millis >= interval_millis ) {
          // We have passed the time threshold, so a new change of state is allowed.
          // set the STATE_CHANGED flag and the new DEBOUNCED_STATE.
          // This will be prompt as long as there has been greater than interval_misllis ms since last change of input.
          // Otherwise debounced state will not change again until bouncing is stable for the timeout period.
          state ^= _BV(DEBOUNCED_STATE);
          state |= _BV(STATE_CHANGED);
        }
      }

      // If the currentState is different from previous currentState, reset the debounce timer - as input is still unstable
      // and we want to prevent new button state changes until the previous one has remained stable for the timeout.
      if ( currentState != (bool)(state & _BV(UNSTABLE_STATE)) ) {
        // Update Unstable Bit to macth currentState
        state ^= _BV(UNSTABLE_STATE);
        previous_millis = millis();
      }
      // return just the sate changed bit
      return state & _BV(STATE_CHANGED);
    } else {
      state &= ~_BV(STATE_CHANGED);

      // If the reading is different from last reading, reset the debounce counter
      if ( currentState != (bool)(state & _BV(UNSTABLE_STATE)) ) {
        previous_millis = millis();
        state ^= _BV(UNSTABLE_STATE);
      } else if ( millis() - previous_millis >= interval_millis ) {
        // We have passed the threshold time, so the input is now stable
        // If it is different from last state, set the STATE_CHANGED flag
        if ((bool)(state & _BV(DEBOUNCED_STATE)) != currentState) {
          previous_millis = millis();
          state ^= _BV(DEBOUNCED_STATE);
          state |= _BV(STATE_CHANGED);
        }
      }

      return state & _BV(STATE_CHANGED);
    }
  }

  bool value() const {
    return state & _BV(DEBOUNCED_STATE);
  }
  bool changed() const {
    return (state & _BV(STATE_CHANGED));
  }
  bool rose() const {
    return (state & _BV(DEBOUNCED_STATE)) && (state & _BV(STATE_CHANGED));
  }
  bool fell() const {
    return !(state & _BV(DEBOUNCED_STATE)) && (state & _BV(STATE_CHANGED));
  }
private:
  unsigned long previous_millis;
  byte state;
};

#endif DEBOUNCE_H
