#include <Arduino.h>
#include <TFT_eSPI.h>
#include <ESP32Servo.h>
#include "sbus.h"
#include "motor.h"

// ================= Pin Configuration =================
#define SBUS_RX_PIN    21  // Connect Radiolink R9DS SBUS output here
#define SERVO_PIN      15  // Connect Steering Servo signal here
#define MOTOR_AIN1     12  // TB6612FNG AIN1 (direction)
#define MOTOR_AIN2     13  // TB6612FNG AIN2 (direction)
#define MOTOR_PWMA     26  // TB6612FNG PWMA (speed, PWM) — must be a PWM-capable pin
#define MOTOR_STBY     27  // TB6612FNG STBY (standby; HIGH = enabled)
#define HEADLIGHT_PIN  25  // Connect headlight LED (via transistor/resistor) here

// --- 2S battery monitoring (external divider on GPIO35) ---
// 2S LiPo: 7.4V nominal, 8.4V full. Use a 200k/100k divider so the GPIO
// never sees more than 3.3V (at 8.4V the tap is 2.8V).
// DO NOT use the T-Display onboard 1S divider (GPIO34/14) for a 2S pack ---
// it would feed >3.3V into the ESP32 and can damage the pin.
#define BAT2S_ADC_PIN  35  // external divider tap (input-only pin, safe)
#define BAT2S_RATIO    3.0 // (R1+R2)/R2 = 300k/100k
#define BAT2S_CAL      1.1 // ESP32 ADC non-linearity correction

// ================= Control Mapping Config =================
// SBUS channel indices (0-based)
// Channel 1 (index 0) usually controls Steering (Roll/Aileron on transmitter)
// Channel 3 (index 2) usually controls Throttle (Pitch/Throttle on transmitter)
// Channel 5 (index 4) usually controls Switch A (Toggles headlights)
#define STEERING_CHANNEL  0
#define THROTTLE_CHANNEL  1
#define HEADLIGHT_CHANNEL 4

// Servo steering limits (to prevent hardware/linkage binding)
#define SERVO_MIN_DEG 45
#define SERVO_CENTER_DEG 90
#define SERVO_MAX_DEG 135

// SBUS Range defaults (standard values)
#define SBUS_MIN  172
#define SBUS_MID  992
#define SBUS_MAX  1811
#define DEADBAND  40  // Prevent motor buzzing around center position

// ================= Device Instances =================
TFT_eSPI tft = TFT_eSPI();
Servo steeringServo;
SBUS sbus;
TB6612Motor motor(MOTOR_AIN1, MOTOR_AIN2, MOTOR_PWMA, MOTOR_STBY);

// ================= Global Variables =================
uint32_t lastDisplayUpdate = 0;
float batteryVoltage = 0.0;

// Read the 2S pack voltage via the external 200k/100k divider on GPIO35.
float readBatteryVoltage() {
    int raw = analogRead(BAT2S_ADC_PIN);

    // External divider (200k top / 100k bottom) => ratio 3.0
    // ESP32 ADC: ~3.3V reference, 12-bit (4095)
    // Calibration factor (~1.1) corrects ESP32 ADC non-linearity
    float volt = (raw / 4095.0) * 3.3 * BAT2S_RATIO * BAT2S_CAL;
    return volt;
}

void setup() {
    Serial.begin(115200);
    
    // Battery measurement: external 2S divider on GPIO35 is always-on
    // (resistors ~200k/100k draw only ~28uA, negligible). No enable pin needed.

    // Initialize TFT Screen
    tft.init();
    tft.setRotation(1); // Landscape mode
    tft.fillScreen(TFT_BLACK);
    
    // Display splash screen
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(10, 20);
    tft.println("ESP32 RC CAR");
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(10, 50);
    tft.println("Initializing hardware...");
    
    // Initialize ESP32 PWM Timers for Servo
    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    ESP32PWM::allocateTimer(2);
    ESP32PWM::allocateTimer(3);
    steeringServo.setPeriodHertz(50); // Standard 50Hz RC servo
    steeringServo.attach(SERVO_PIN, 500, 2400); // 500us to 2400us pulse width
    steeringServo.write(SERVO_CENTER_DEG); // Center steering on startup
    
    // Initialize DC Motor driver
    motor.begin();
    
    // Initialize SBUS receiver connection on Serial2
    sbus.begin(SBUS_RX_PIN);
    
    delay(500);
    tft.fillScreen(TFT_BLACK);
    
    Serial.println("System Initialized!");
}

void loop() {
    // Read SBUS incoming packets
    bool newFrame = sbus.read();
    
    if (sbus.isFailsafe()) {
        // Safe state: stop motor and center steering
        motor.stop();
        steeringServo.write(SERVO_CENTER_DEG);
    } else if (newFrame) {
        // --- STEERING ---
        uint16_t rawSteer = sbus.getChannel(STEERING_CHANNEL);
        // Constrain incoming SBUS values to prevent out of bounds
        rawSteer = constrain(rawSteer, SBUS_MIN, SBUS_MAX);
        // Map to servo degrees (inverted if steering direction needs correction)
        int servoAngle = map(rawSteer, SBUS_MIN, SBUS_MAX, SERVO_MIN_DEG, SERVO_MAX_DEG);
        steeringServo.write(servoAngle);
        
        // --- THROTTLE ---
        uint16_t rawThrottle = sbus.getChannel(THROTTLE_CHANNEL);
        rawThrottle = constrain(rawThrottle, SBUS_MIN, SBUS_MAX);
        
        int motorSpeed = 0;
        // Apply deadband around center midpoint
        if (rawThrottle > (SBUS_MID + DEADBAND)) {
            // Forward: map from [MID + DEADBAND, MAX] to [0, 255]
            motorSpeed = map(rawThrottle, SBUS_MID + DEADBAND, SBUS_MAX, 0, 255);
        } else if (rawThrottle < (SBUS_MID - DEADBAND)) {
            // Reverse: map from [MIN, MID - DEADBAND] to [-255, 0] (negative means reverse)
            motorSpeed = map(rawThrottle, SBUS_MIN, SBUS_MID - DEADBAND, -255, 0);
        } else {
            motorSpeed = 0;
        }
        
        motor.setSpeed(motorSpeed);
    }
    
    // --- DISPLAY & TELEMETRY UPDATE (Every 200 ms) ---
    uint32_t now = millis();
    if (now - lastDisplayUpdate >= 200) {
        lastDisplayUpdate = now;
        batteryVoltage = readBatteryVoltage();
        
        // --- Header Block ---
        tft.setTextSize(2);
        if (sbus.isFailsafe()) {
            tft.setTextColor(TFT_RED, TFT_BLACK);
            tft.setCursor(10, 4);
            tft.print("FAILSAFE / NO SIG");
        } else {
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(10, 4);
            tft.print("RC CAR: ACTIVE");
        }

        tft.setTextSize(1);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);

        // --- Battery Voltage ---
        tft.setCursor(10, 24);
        tft.print("Battery: ");
        // 2S thresholds: red < 6.8V (3.4V/cell), yellow < 7.4V (3.7V/cell)
        if (batteryVoltage < 6.8) {
            tft.setTextColor(TFT_RED, TFT_BLACK);
        } else if (batteryVoltage < 7.4) {
            tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        } else {
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
        }
        tft.print(batteryVoltage, 2);
        tft.println(" V");
        tft.setTextColor(TFT_WHITE, TFT_BLACK);

        // --- Servo & Steering Status ---
        uint16_t steerVal = sbus.getChannel(STEERING_CHANNEL);
        int steerAngle = steeringServo.read();
        tft.setCursor(10, 36);
        tft.printf("Steer (Ch%d): %-4d -> %-3d deg", STEERING_CHANNEL + 1, steerVal, steerAngle);

        // Visual Steer Bar (filled left->right, proportional to angle)
        tft.fillRect(10, 46, 220, 8, TFT_BLACK);                 // clear whole bar row
        tft.drawRect(10, 46, 220, 8, TFT_DARKGREY);             // border
        int steerFrac = map(steerAngle, SERVO_MIN_DEG, SERVO_MAX_DEG, 0, 218);
        steerFrac = constrain(steerFrac, 0, 218);
        tft.fillRect(11, 47, steerFrac, 6, TFT_BLUE);           // filled portion

        // --- Motor & Throttle Status ---
        uint16_t throttleVal = sbus.getChannel(THROTTLE_CHANNEL);
        // Active speed target. In FAILSAFE the motor is force-stopped, so show 0%.
        int activeSpeed = 0;
        if (!sbus.isFailsafe()) {
            if (throttleVal > (SBUS_MID + DEADBAND)) {
                activeSpeed = map(throttleVal, SBUS_MID + DEADBAND, SBUS_MAX, 0, 100);   // 0-100%
            } else if (throttleVal < (SBUS_MID - DEADBAND)) {
                activeSpeed = map(throttleVal, SBUS_MIN, SBUS_MID - DEADBAND, -100, 0);  // -100% to 0%
            }
        }
        activeSpeed = constrain(activeSpeed, -100, 100);
        tft.setCursor(10, 60);
        tft.printf("Throt (Ch%d): %-4d -> %-4d%%", THROTTLE_CHANNEL + 1, throttleVal, activeSpeed);

        // Visual Throttle Bar (filled from center, green fwd / red rev)
        tft.fillRect(10, 70, 220, 8, TFT_BLACK);                 // clear whole bar row
        tft.drawRect(10, 70, 220, 8, TFT_DARKGREY);             // border
        int midX = 120;
        tft.drawFastVLine(midX, 71, 6, TFT_WHITE);               // center mark
        int tFrac = map(abs(activeSpeed), 0, 100, 0, 100);       // px each side (half-width 110)
        tFrac = constrain(tFrac, 0, 100);
        if (activeSpeed > 0) {
            tft.fillRect(midX, 71, tFrac, 6, TFT_GREEN);
        } else if (activeSpeed < 0) {
            tft.fillRect(midX - tFrac, 71, tFrac, 6, TFT_RED);
        }

        // --- Motor Status (direction + live control-pin debug) ---
        int motorSpeed = motor.getSpeed();            // -255..255
        const char* dir = motor.getDirection();       // FWD / REV / BRAKE / STOP
        int motorPct = (motorSpeed * 100) / 255;      // signed percent

        // Color-coded direction label
        tft.setCursor(10, 84);
        if (strcmp(dir, "FWD") == 0)        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        else if (strcmp(dir, "REV") == 0)   tft.setTextColor(TFT_RED, TFT_BLACK);
        else if (strcmp(dir, "BRAKE") == 0) tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        else                                tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.printf("Motor: %-5s %-4d%%", dir, motorPct);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);

        // Live GPIO levels of the TB6612FNG control pins (0/1)
        tft.setCursor(10, 96);
        tft.printf("AIN1:%d AIN2:%d PWM:%d STBY:%d",
                   motor.getPinAIN1(), motor.getPinAIN2(),
                   motor.getPinPWMA(), motor.getPinSTBY());

        // --- Debug Info (Receiver Frame Count) ---
        tft.setCursor(10, 112);
        tft.printf("Frame Loss: %-3s", sbus.isFrameLost() ? "YES" : "NO ");
        tft.setCursor(130, 112);
        tft.printf("Up: %-6lu s", millis() / 1000);
    }
}
