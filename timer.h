#include <Arduino.h>

#ifndef Timer_h
#define Timer_h

class Timer {
  public:
    Timer();
    void setTimeout(unsigned long timeout);
    bool timeout();
  private:
    unsigned long _start;
    unsigned long _timeout;
};

#endif
