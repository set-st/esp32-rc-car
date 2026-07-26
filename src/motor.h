#ifndef MOTOR_H
#define MOTOR_H

#include <Arduino.h>

class MX1508Motor {
public:
    MX1508Motor(int pinIN1, int pinIN2)
        : _pinIN1(pinIN1), _pinIN2(pinIN2) {}

    void begin() {
        pinMode(_pinIN1, OUTPUT);
        pinMode(_pinIN2, OUTPUT);
        stop();
    }

    // speed can be from -255 (full reverse) to 255 (full forward)
    void setSpeed(int speed) {
        if (speed > 255) speed = 255;
        if (speed < -255) speed = -255;

        // Apply a small deadband to prevent motor buzzing at very low inputs
        if (speed > 15) { // Forward
            analogWrite(_pinIN1, speed);
            analogWrite(_pinIN2, 0);
        } else if (speed < -15) { // Reverse
            analogWrite(_pinIN1, 0);
            analogWrite(_pinIN2, -speed);
        } else {
            stop();
        }
    }

    void stop() {
        analogWrite(_pinIN1, 0);
        analogWrite(_pinIN2, 0);
    }

    void brake() {
        analogWrite(_pinIN1, 255);
        analogWrite(_pinIN2, 255);
    }

private:
    int _pinIN1;
    int _pinIN2;
};

#endif
