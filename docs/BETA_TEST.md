# Beta Test Checklist

Use this checklist before handing MOT hardware to a beta tester.

## Hardware

- ESP32-WROOM powered reliably
- CAN transceiver connected
- OBD2 wiring checked
- Device mounted securely
- No loose wiring
- USB power or regulated 5 V supply tested

## Firmware

- Firmware flashed via USB
- OTA update tested
- OTA password recorded
- Web UI reachable
- Fallback AP tested if possible

## MQTT

- Broker reachable
- Username/password tested
- Topic prefix set to `mot`
- Vehicle ID set correctly
- Dashboard receives data

## Dashboard

- Dashboard reachable from phone
- WSS connection tested
- SOC updates live
- Speed updates live
- Charging state tested if possible

## Before travel

- Give tester a simple LTE/WiFi hotspot if needed
- Avoid experimental GSM modules for non-technical testers
- Keep a known-good firmware binary available
- Keep OTA password available
