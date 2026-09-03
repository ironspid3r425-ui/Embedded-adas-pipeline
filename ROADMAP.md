# Roadmap

This project grows in one direction on purpose: from **bare-metal peripheral bring-up** toward a small but real **automotive diagnostics stack**, all on the same NUCLEO-F411RE hardware. Each phase is verified against real hardware before the next begins.

Phases marked ✅ are done and in `main`; the rest are planned, with enough of a spec to build against.

---

## Phase 1 — Peripheral bring-up ✅

GPIO, UART, I2C (SSD1306 OLED), and SPI, each brought up incrementally and verified against hardware (serial output, OLED readout, byte-for-byte SPI loopback), plus a register-level GPIO rewrite bypassing the HAL. See the day-by-day log in the [README](README.md).

**Skills shown:** C, STM32 HAL + register level, GPIO/UART/I2C/SPI, on-target debugging, incremental verification.

---

## Phase 2 — CAN bus 🔜

Bring the vehicle-network protocol onto the board.

- Wire an **MCP2515** CAN controller over the already-validated SPI1 pins.
- Send a fixed CAN frame; receive it on a second node (a second MCP2515 on an ESP32-S3 or a USB-CAN adapter on a PC).
- Verify the frame on the wire with the logic analyzer (PulseView).
- Add timeout/retry handling and prove graceful behavior when the bus is disconnected mid-transmission.

**Skills shown:** CAN framing (IDs, DLC, arbitration), SPI-attached controller bring-up, bus-level debugging.

---

## Phase 3 — RTOS structure 🔜

Replace the single polling loop with a proper concurrent structure once two nodes are talking.

- Move CAN receive, the alert logic, and a heartbeat into separate **FreeRTOS** tasks with distinct priorities.
- Protect the shared OLED/UART with a mutex; prove the failure mode without it.

**Skills shown:** FreeRTOS tasks, scheduling, priority, synchronization primitives.

---

## Phase 4 — Automotive diagnostics: a hand-built UDS/DTC stack 🔜

The capstone, and the reason this repo is framed as automotive rather than a generic STM32 demo. Real AUTOSAR tooling isn't individually accessible, but the *protocols* it speaks are — so this phase implements a miniature of a real diagnostic ECU by hand. Each piece maps to a named AUTOSAR module.

- **ISO-TP (ISO 15765-2)** message segmentation over CAN — single-frame first, then multi-frame. *↔ CanTp.*
- A small **UDS (ISO 14229) responder**: service `0x10` (session control) and `0x22` (read data by identifier — e.g. a fake VIN and a live sensor value). *↔ DCM (Diagnostic Communication Manager).*
- A **fault store**: detect an out-of-range sensor condition, raise a **DTC** with a status byte, and expose it via `0x19` (read DTCs) and `0x14` (clear DTCs). *↔ DEM (Diagnostic Event Manager).*
- **Persist the DTC to flash** so it survives a power cycle and is restored at startup. *↔ NvM + EcuM restore.*

A Python tester (`python-can` + `udsoncan`) plays the scan tool against the hand-built ECU.

**Why this phase matters:** building even a crude DEM by hand is how you understand *from the inside* why the real one separates fault detection from DTC storage, why status bytes exist, and why startup sequencing matters. It also demonstrates automotive diagnostics on real hardware — the one thing a bring-up demo doesn't show.

**Skills shown:** CAN-TP/ISO-TP, UDS services, DTC lifecycle, flash-backed persistence — the working substance behind DCM/DEM/NvM.

---

## Optional extensions

- A real I2C/SPI sensor (IMU or distance sensor) as a genuine ADAS input, replacing the loopback stand-in.
- CI: a GitHub Actions workflow that cross-compiles the firmware on push.

---

*Companion learning notes for the diagnostics phase live outside this repo (author's private study material). This roadmap is the buildable spec; the firmware is written and verified on real NUCLEO-F411RE hardware phase by phase.*
