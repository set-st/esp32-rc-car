#ifndef MOTOR_H
#define MOTOR_H

#include <Arduino.h>

// TB6612FNG dual motor driver abstraction (using channel A).
//
// Wiring:
//   VM    -> 2S battery (+)        (motor supply, 2.5V..13.5V)
//   VCC   -> ESP32 3.3V            (logic supply, keep separate from VM!)
//   GND   -> common ground
//   STBY  -> ESP32 GPIO (HIGH = enabled, LOW = standby)
//   AIN1  -> direction GPIO
//   AIN2  -> direction GPIO
//   PWMA  -> ESP32 PWM GPIO        (speed; ~25kHz, 0..255)
//   AOUT1 / AOUT2 -> DC motor terminals
//
// Drive scheme (sign-magnitude):
//   forward : AIN1=H, AIN2=L, PWMA=duty
//   reverse : AIN1=L, AIN2=H, PWMA=duty
//   brake   : AIN1=H, AIN2=H, PWMA=duty (or 0)
//   coast   : AIN1=L, AIN2=L
class TB6612Motor {
public:
    TB6612Motor(int pinAIN1, int pinAIN2, int pinPWMA, int pinSTBY)
        : _ain1(pinAIN1), _ain2(pinAIN2), _pwma(pinPWMA), _stby(pinSTBY),
          _lastSpeed(0), _pwm(0) {}

    void begin() {
        pinMode(_ain1, OUTPUT);
        pinMode(_ain2, OUTPUT);
        pinMode(_pwma, OUTPUT);
        pinMode(_stby, OUTPUT);
        digitalWrite(_stby, HIGH); // take driver out of standby
        stop();                    // start safely coasting/stopped
    }

    // speed: -255 (full reverse) .. +255 (full forward); 0 = stop
    void setSpeed(int speed) {
        if (speed > 255) speed = 255;
        if (speed < -255) speed = -255;
        _lastSpeed = speed;

        // Small deadband to avoid motor buzz at very low inputs.
        if (speed > 15) {            // Forward
            digitalWrite(_ain1, HIGH);
            digitalWrite(_ain2, LOW);
            lastAnalogWrite(speed);
        } else if (speed < -15) {    // Reverse
            digitalWrite(_ain1, LOW);
            digitalWrite(_ain2, HIGH);
            lastAnalogWrite(-speed);
        } else {
            stop();
        }
    }

    // Coast: motor leads floating (no braking).
    void stop() {
        digitalWrite(_ain1, LOW);
        digitalWrite(_ain2, LOW);
        lastAnalogWrite(0);
        _lastSpeed = 0;
    }

    // Brake: motor leads shorted through the driver.
    void brake() {
        digitalWrite(_ain1, HIGH);
        digitalWrite(_ain2, HIGH);
        lastAnalogWrite(255);
        _lastSpeed = 0;
    }

    // Put the driver into standby (low power, outputs disabled).
    void standby(bool on) {
        digitalWrite(_stby, on ? LOW : HIGH);
    }

    // ---- Telemetry accessors (for on-screen debug) ----
    // Last commanded speed, -255..255 (0 = stopped/coast/brake).
    int getSpeed() const { return _lastSpeed; }

    // Last commanded PWM duty actually written to PWMA, 0..255.
    // This is the real duty cycle; reading the PWM pin with digitalRead
    // would only return the logic level (0/1), not the duty.
    int getPwmDuty() const { return _pwm; }

    // Human-readable direction based on the live pin states.
    const char* getDirection() const {
        int a1 = digitalRead(_ain1);
        int a2 = digitalRead(_ain2);
        if (a1 && !a2) return "FWD";
        if (!a1 && a2) return "REV";
        if (a1 && a2)  return "BRAKE";
        return "STOP";
    }

    // Live GPIO levels (0/1) — useful to confirm the wiring is driven.
    int getPinAIN1()  const { return digitalRead(_ain1); }
    int getPinAIN2()  const { return digitalRead(_ain2); }
    int getPinPWMA()  const { return digitalRead(_pwma); }
    int getPinSTBY()  const { return digitalRead(_stby); }

private:
    // analogWrite wrapper that also records the last duty for telemetry.
    void lastAnalogWrite(int duty) {
        _pwm = duty;
        analogWrite(_pwma, duty);
    }

    int _ain1;
    int _ain2;
    int _pwma;
    int _stby;
    int _lastSpeed;
    int _pwm;
};

#endif
