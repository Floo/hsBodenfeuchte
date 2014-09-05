#include "timer.h"


Timer::Timer() {
  
}

void Timer::setTimeout(unsigned long timeout) {
  _timeout = timeout;
  _start = millis();
}

bool Timer::timeout() {
  unsigned long tmpMillis;
  tmpMillis = millis();
  if (tmpMillis > _start) {
    return (tmpMillis - _start > _timeout);
  } else {
    return (_start - tmpMillis > _timeout);
  }
}
