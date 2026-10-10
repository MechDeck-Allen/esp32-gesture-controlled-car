# Power and wiring notes

## Receiver
- The hardwired direction inputs are IN1→GPIO26, IN2→GPIO27, IN3→GPIO14, IN4→GPIO25.
- LED anode→GPIO23 through 220–330 Ω; cathode→GND.
- L298N GND and ESP32 GND must be common.
- The current receiver sketch also requires ENA→GPIO32 and ENB→GPIO33, with the L298N ENA/ENB jumpers removed.
- Use a fused, protected 3S battery pack. Verify the TT motors' rated voltage and stall current before powering them from the pack. Many TT motors are rated around 3–6 V; a 3S pack is 12.6 V when full.

## Transmitter
- One 18650 cell feeds a boost converter adjusted and measured at 5 V, which powers the ESP32 board through its 5V/VIN input as appropriate for that exact DevKit.
- MPU6050 uses 3V3, GND, SDA GPIO21, SCL GPIO22.
- Optional momentary button between GPIO18 and GND.
- LED anode GPIO23 through 220–330 Ω; cathode GND.

Never change wiring while powered. Insulate battery terminals and exposed joints to prevent shorts.
