# ESP32 T-Display Radio Controlled Car (SBUS & MX1508)

This project implements a complete, feature-rich firmware for a Radio Controlled (RC) Car using the **LilyGO T-Display (ESP32)**, the **MX1508 H-bridge motor driver**, and the **Radiolink R9DS receiver** (using the high-performance 1-wire SBUS protocol).

It features:
- **1-Wire SBUS telemetry**: Zero-delay control of up to 16 channels with built-in signal-loss **failsafe** protection (stops the car if connection is lost).
- **Proportional DC Motor drive**: Using a portable H-bridge speed mapping with deadband support.
- **Proportional Servo steering**: Safe angle constraints to prevent mechanical binding of steering linkages.
- **On-screen color dashboard**: Displays active throttle (with a bidirectional progress bar), steering angle, receiver frame-loss, failsafe states, and system uptime.
- **Real-time battery monitoring**: Reads and displays the current battery voltage of the onboard LiPo.

---

## 🔌 Connection Diagram (Pinout)

Ensure that all grounds are connected together (**Common Ground**)!

| Component | Pin / Port | ESP32 GPIO | Description |
| :--- | :--- | :--- | :--- |
| **Radiolink R9DS** | SBUS Signal | **GPIO 21** | Connect to the SBUS pin on the receiver |
| **Radiolink R9DS** | VCC | **5V** | Powers the receiver (supports 4.8V - 10V) |
| **Radiolink R9DS** | GND | **GND** | Ground connection |
| **Steering Servo** | Signal (Orange/Yellow) | **GPIO 15** | Servo PWM control signal |
| **Steering Servo** | Power (Red) | **5V** or Ext | Power line for the servo |
| **Steering Servo** | Ground (Brown/Black) | **GND** | Ground connection |
| **MX1508 Driver** | IN1 | **GPIO 12** | Motor direction and speed (PWM) |
| **MX1508 Driver** | IN2 | **GPIO 13** | Motor direction and speed (PWM) |
| **MX1508 Driver** | MOTOR A | *DC Motor* | Connect directly to the drive motor terminals |
| **MX1508 Driver** | VCC / V+ | *Battery (+)* | High-current power for the motor (2V - 10V) |
| **MX1508 Driver** | GND | *Battery (-) & GND* | Common ground connection |

### Visual Wiring Diagram

![ESP32 RC Car Connection Diagram](connection-diagram.svg)

> **Legend:** 🔵 Signal (GPIO) · 🔴 5V power · ⚫ Ground · 🟣 Motor wires.
> Note: the Radiolink R9DS SBUS output is 5V logic — step it down to ~3.3V
> (1kΩ + 2kΩ divider) before GPIO 21, since ESP32 pins are **not 5V-tolerant**.

---

## ⚠️ Important: SBUS Voltage Logic Levels (5V vs 3.3V)

**ESP32 GPIO pins are not 5V-tolerant!**
Since the Radiolink R9DS receiver is powered by 5V, its SBUS output signal operates at 5V logic levels. Connecting this directly to **GPIO 21** of the ESP32 can damage or degrade the ESP32 microcontroller over time.

To protect your ESP32, you **must** use one of the following simple level-shifting methods:

### Method A: Voltage Divider (Recommended & Most Reliable)
Step down the 5V signal to a safe ~3.3V using two standard resistors:
- **Resistor R1 (1 kOhm)**: Connect in series between the receiver's SBUS output and GPIO 21.
- **Resistor R2 (2 kOhm)**: Connect between GPIO 21 and ESP32 GND.

```text
R9DS SBUS Out ───────[ 1 kOhm ]───────┬─────── GPIO 21 (ESP32)
                                     │
                                 [ 2 kOhm ]
                                     │
GND ─────────────────────────────────┴─────── GND
```
*(You can use other resistor values with a similar 1:2 ratio, such as 10 kOhm and 20 kOhm).*

### Method B: Current Limiting Resistor (Quick DIY Workaround)
If you don't have two resistors, place a single **1 kOhm to 4.7 kOhm resistor** in series between the SBUS Output and the GPIO 21 pin:

```text
R9DS SBUS Out ───────[ 1 kOhm ... 4.7 kOhm ]─────── GPIO 21 (ESP32)
```
This forces the ESP32's internal clamping ESD diodes to safely handle the excess voltage by limiting the current to a tiny fraction of a milliampere.

---

## 📡 Configuring the Radiolink R9DS Receiver

The Radiolink R9DS receiver supports both PWM and SBUS modes. By default, it may be set to PWM. You **must** switch it to SBUS mode:
1. Power up the receiver.
2. Look at the receiver LED. If it is **Red**, it is in PWM mode.
3. **Double-press the small button** on the side of the receiver.
4. The LED should turn **Blue / Purple**. This indicates that the SBUS mode is active on the SBUS channel (bottom pin on the far-right row).

---

## ⚙️ Control Customization

In `src/main.cpp`, you can easily configure the SBUS channel assignments to match your specific transmitter layout:
```cpp
// Channel 1 (index 0) usually controls Steering (Aileron/Roll)
#define STEERING_CHANNEL 0

// Channel 3 (index 2) usually controls Throttle (Pitch/Throttle)
#define THROTTLE_CHANNEL 2
```
If your transmitter uses different channels, the **T-Display dashboard** will show active real-time SBUS values for Channel 1 and 3, allowing you to troubleshoot and map them instantly.

To prevent physical damage to your steering linkage, you can also limit the maximum servo movement:
```cpp
#define SERVO_MIN_DEG 45   // Max left steering angle
#define SERVO_CENTER_DEG 90 // Perfectly straight steering
#define SERVO_MAX_DEG 135  // Max right steering angle
```

---

## 🚀 How to Compile and Flash

This project is built using **PlatformIO** (the modern ecosystem for embedded development).

### 1. Install PlatformIO
We highly recommend installing the **PlatformIO IDE** extension inside [VS Code](https://code.visualstudio.com/).

### 2. Open Project
Open the `/home/stas/rc-car` folder in VS Code / PlatformIO.

### 3. Build & Upload
- To compile the firmware, click the **Build** checkmark icon in the bottom status bar, or run:
  ```bash
  pio run
  ```
- Connect your LilyGO T-Display via USB-C.
- Click the **Upload** arrow icon, or run:
  ```bash
  pio run --target upload
  ```
- To view logs and telemetry from your computer, open the Serial Monitor (115200 baud):
  ```bash
  pio device monitor
  ```

---

## 🛠️ Project Structure

- `platformio.ini` - Project configuration, automated library dependencies, and seamless TFT build-flags.
- `src/main.cpp` - Core loop, safety watchdog/failsafe, state-machine, and color UI dashboard.
- `src/sbus.h` - Lightweight, non-blocking custom SBUS parser using hardware UART signal inversion.
- `src/motor.h` - Clean motor driver abstraction supporting forward/reverse/brake mapping and analog PWM control.
