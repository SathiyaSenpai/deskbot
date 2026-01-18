#ifndef SERVO_CONTROLLER_H
#define SERVO_CONTROLLER_H

#include <ESP32Servo.h>
#include "pins.h"

class ServoController {
public:
  void begin() {
    servo_.setPeriodHertz(50); // standard servo
    servo_.attach(PIN_SERVO, 500, 2500); // min/max µs
    setAngle(90);
  }

  void setAngle(int angle) {
    angle = constrain(angle, 0, 180);
    servo_.write(angle);
    currentAngle_ = angle;
  }

  int angle() const { return currentAngle_; }

private:
  Servo servo_;
  int currentAngle_ = 90;
};

#endif // SERVO_CONTROLLER_H
