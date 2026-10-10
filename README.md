# ESP32 Gesture-Controlled Robot Car

A wireless, two-wheel differential-drive robot controlled by tilting a handheld ESP32 + GY-521 MPU6050 transmitter.

## Features

- Two ESP32 DevKit boards communicate directly using ESP-NOW (no Wi-Fi router required).
- MPU6050 accelerometer/gyroscope measures hand tilt.
- Neutral reference is captured at startup; a button can recalibrate the reference.
- Proportional differential steering maps forward/backward tilt and sideways tilt to left/right motor commands.
- Green LEDs indicate recent communication.
- Receiver stops the motors when control packets time out.

## Hardware

### Car / receiver
- ESP32 DevKit
- L298N dual H-bridge motor driver
- Two TT geared DC motors and a rear caster
- 3 × 18650 cells in series (3S pack)
- Toggle power switch
- Green LED and 220–330 Ω series resistor
- A suitable 3S BMS, charger, fuse, and regulated supply as required

### Handheld transmitter
- ESP32 DevKit
- GY-521 MPU6050 module
- One 18650 cell
- Boost converter set to 5 V for the ESP32 board (added to resolve rebooting at low input voltage)
- Toggle switch
- Green LED and 220–330 Ω series resistor
- Optional momentary recalibration button

## Pinout

### Receiver — fixed hardwired connections

| Signal | ESP32 pin |
|---|---|
| L298N IN1 | GPIO 26 |
| L298N IN2 | GPIO 27 |
| L298N IN3 | GPIO 14 |
| L298N IN4 | GPIO 25 |
| Green LED anode (+) | GPIO 23 through 220–330 Ω resistor |
| Green LED cathode (−) | GND |
| L298N GND | ESP32 GND (common ground) |

**PWM speed control:** The receiver sketch additionally uses ENA → GPIO 32 and ENB → GPIO 33. Connect these wires and remove the ENA/ENB jumpers on the L298N board for proportional speed control. If you have not wired these, do not expect the supplied receiver sketch to run correctly.

### Transmitter

| Signal | ESP32 pin |
|---|---|
| GY-521 VCC | 3V3 |
| GY-521 GND | GND |
| GY-521 SDA | GPIO 21 |
| GY-521 SCL | GPIO 22 |
| Recalibration button | GPIO 18 to GND (configured as `INPUT_PULLUP`) |
| Green LED anode (+) | GPIO 23 through 220–330 Ω resistor |
| Green LED cathode (−) | GND |
| GY-521 XDA, XCL, AD0, INT | Unconnected for this build |

The button is optional in the hardware, but the supplied transmitter sketch expects it on GPIO 18. If omitted, update the sketch or leave GPIO 18 pulled high and do not press it.

## Power and safety

- The 3-cell series pack is nominally 11.1 V and reaches 12.6 V when fully charged (assuming typical 3.7 V lithium-ion cells).
- Use a suitable 3S BMS and charger, a fuse near the battery, and secure insulation against shorts.
- Typical yellow TT motors are often rated for 3–6 V. **Do not feed the 3S pack directly to motors unless their rating and driver/supply arrangement are verified.** Use a suitable regulated motor supply if required.
- The L298N has a significant voltage drop and can heat up; verify motor stall current against the driver module's limits.
- Transmitter: the single 3.7 V nominal 18650 cell is boosted to 5 V for the ESP32 board. Verify the boost converter output with a multimeter before connecting it. Do not connect the cell directly to 3V3.
- Receiver ESP32 and L298N logic must share ground. Do not assume the L298N module's onboard 5 V regulator can safely power the ESP32.
- Test with the wheels raised, keep hands clear of wheels, and have an accessible power switch.
- The code uses broadcast ESP-NOW packets without encryption. This is convenient for a prototype, not a security-hardened control link.

## How the two ESP32s communicate

They use **ESP-NOW**, a connectionless wireless protocol built into Espressif's Wi-Fi radio. It does not require a router or internet connection.

1. The transmitter reads the MPU6050 and broadcasts a small packet containing a sequence number, relative pitch, relative roll, and calibration state.
2. The receiver accepts control packets, mixes forward/reverse and steering into left/right motor commands, and drives the L298N.
3. The receiver broadcasts an acknowledgement packet. The transmitter's green LED lights when recent acknowledgements arrive.
4. The receiver turns its LED on while valid control packets are recent and stops the motors if no control packet arrives within the timeout.

Both sketches set Wi-Fi channel 1. Keep the packet structures and channel settings the same on both boards.

## Build / upload

1. Install the **ESP32 by Espressif Systems** board package in Arduino IDE. The receiver sketch uses the Arduino-ESP32 **3.x LEDC API** (`ledcAttach` and `ledcWrite(pin, duty)`).
2. Open `transmitter/gesture_transmitter.ino` and upload it to the handheld ESP32.
3. Open `receiver/gesture_receiver.ino` and upload it to the car ESP32.
4. Set Serial Monitor to 115200 baud.
5. Keep the transmitter still during startup gyroscope bias calibration. Hold the hand at the desired neutral angle at startup; press the GPIO 18 button to reset neutral later.
6. Test communication and sensor values first. Then, with the car wheels raised, test motor direction at low speed.

## Tuning

- `DEADZONE_DEG` in receiver: neutral tilt range.
- `MAX_TILT_DEG` in receiver: tilt needed for full command.
- `MAX_PWM` in receiver: maximum PWM command (0–255).
- `PITCH_SIGN` and `ROLL_SIGN` in transmitter: reverse an axis if gesture direction is inverted.
- `LEFT_MOTOR_INVERTED` / `RIGHT_MOTOR_INVERTED` in receiver: correct individual motor direction.

The current basic complementary filter estimates roll and pitch. The MPU6050 has no magnetometer, so absolute yaw/heading is not measured and gyroscope yaw drifts. This project does not need absolute heading for simple hand-tilt control.

## Project photos and CAD

Completed-build photos are in [`media/photos/`](media/photos/). Add the SolidWorks `.sldprt` and STL files to `cad/` before committing them. The CAD files were not included in the uploaded photo set, so this package contains a placeholder note rather than fabricated CAD files.

## Known implementation notes

- The transmitter expects the MPU6050 at I²C address `0x68`; AD0 high may change it to `0x69`.
- The supplied receiver requires ENA and ENB connected to GPIO 32 and GPIO 33 with their jumpers removed. The four direction pins and LED match the already-hardwired receiver pinout.
- For production-quality safety, improve packet validation, use unicast peers and acknowledgements, validate packet freshness/sequence numbers, and consider a physical motor-enable/stop mechanism.
- Test failsafe behaviour deliberately with wheels raised before normal use.

## Repository placement

Suggested destination in the existing repository:

`projects/esp32-gesture-controlled-car/`
