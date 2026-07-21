# Hardware Bill of Materials

This is the minimum final hardware set. Bench hardware is reused in the desk;
there is no provisional driver or custom PCB.

| Qty | Component | Firmware interaction |
| ---: | --- | --- |
| 1 | ESP32-S3-DevKitC-1U-N8R8 | Runs the 10 ms controller; WiFi/BLE reserved |
| 1 | 2.4 GHz external antenna | Future connectivity beside a metal frame |
| 1 | DevKit terminal-block adapter | Reliable low-voltage termination |
| 3 | Identical 12 V, 1000 N, 500 mm Hall actuators | One fixed axis and PCNT counter each |
| 3 | Matching BTS7960/IBT-2 modules | RPWM, LPWM, and tied R_EN/L_EN per axis |
| 6 | NC limit switches | Upper/lower fail-safe input per axis |
| 3 | Momentary buttons | Active-low UP, DOWN, CALIBRATE |
| 1 | Dual-contact latching NC E-stop | Physical contactor drop plus GPIO monitor |
| 1 | DC-rated contactor/relay | Physically removes motor power |
| 1 | 12 V PSU | Sized from measured actuator current, not label guesswork |
| 1 | 12 V to regulated 5 V converter | ESP32 5 V input and BTS logic supply |
| 1 | Fuse distribution block | Main, three motor branches, one logic branch |
| 1 | Ventilated enclosure | Touch protection, strain relief, cooling |
| 6 | 4.7-10 kohm Hall pull-ups if required | Open-collector Hall to 3.3 V |
| as needed | Terminal blocks, ferrules, locking connectors, cable glands | No loose Dupont wiring |

## Purchase Gates

Before buying actuators, obtain a real datasheet or vendor confirmation for:

- rated and no-load current, start/stall current, duty cycle, and lead polarity;
- Hall channel voltage, output topology, pulses per motor/output revolution, and
  connector pinout;
- whether both Hall channels tolerate direct 3.3 V pull-up;
- built-in limit behavior, even though external NC limits remain required;
- identical model/revision availability for all three units.

If Hall is 5 V push-pull, add one proper multichannel 3.3 V buffer. Do not use
ad hoc dividers on pulse lines. If Hall is open-collector, use 3.3 V pull-ups.

## Power Sizing

PSU continuous current must cover three measured running currents plus margin.
Transient capability must survive simultaneous soft-start without brownout.
Fuses protect wire and branches; they are selected from measured current,
conductor ampacity, and manufacturer data. The design must never rely on the PSU
or firmware to tolerate three indefinitely stalled actuators.

The PC USB port may power only the DevKit during logic tests. It cannot power a
12 V actuator, BTS7960 motor stage, contactor, or the final three-axis demo.

## Mechanical Requirements

- A rigid steel frame and guided columns carry lateral loads.
- Actuator rods are vertical force elements, not anti-racking guides.
- Mechanical hard stops remain effective if firmware and limits fail.
- The passive L-wing support must not bind or amplify torsion.
- Load testing progresses from frame-only to the expected working load.
