# UDS Diagnostic Tester (Phase 4 — host side)

The "scan tool" side of the [ROADMAP](../ROADMAP.md) Phase 4 diagnostics work. It speaks
**UDS (ISO 14229) over ISO-TP over CAN** — the same diagnostic protocol a real automotive
ECU answers.

It runs today with **no hardware**: a mock ECU and the tester talk over python-can's
*virtual* bus. When the STM32 firmware exists, the mock ECU is replaced by the real board
and only one line in the tester changes.

## Why this exists before the hardware

Most of a diagnostic stack is transport and logic, not registers. Building and proving the
host side first means the evening hardware sessions are spent confirming behavior that's
*already been designed and tested*, not discovering it from scratch. It also keeps the ECU's
decision logic (`ecu_logic.py`) free of any CAN or hardware dependency, so it's unit-testable
on the desk — the "test before it reaches hardware" discipline this whole project is built on.

## Files

| File | Role | Maps to (real AUTOSAR) |
|---|---|---|
| `ecu_logic.py` | Pure diagnostic logic: DID reads, fault detection, DTC store, persist/restore | DCM read + DEM + NvM |
| `mock_ecu.py` | Wraps that logic in a UDS-over-ISO-TP server on (virtual) CAN | DCM + CanTp + CAN driver |
| `tester.py` | The scan-tool client: opens a session, reads the VIN and a sensor | external diagnostic tester |
| `test_ecu_logic.py` | Host-side unit tests for the logic — no CAN, no board | — |

## Run it (no hardware)

```bash
pip install -r requirements.txt

# terminal 1 — the ECU stand-in
python mock_ecu.py

# terminal 2 — the scan tool
python tester.py
```

Expected: the tester enters an extended session (`0x10`), then reads the VIN and the distance
sensor (`0x22`), printing each round-trip.

Run the desk tests any time:

```bash
pytest -v
```

## Going to real hardware (evening)

In `tester.py`, `build_bus()` is the single seam between simulation and hardware. Swap the
`virtual` bus for a real interface (e.g. an `slcan`/serial-CAN adapter or an MCP2515-based
gateway) and the tester drives the actual STM32 ECU instead of the mock. Nothing else changes.

## Implemented so far

- UDS `0x10` DiagnosticSessionControl (with UDS-2020 P2/P2* timing)
- UDS `0x22` ReadDataByIdentifier (VIN + a live sensor value)
- A DTC fault store with raise / read (`0x19`) / clear (`0x14`) and snapshot-based persistence

## Next (as the firmware comes up)

- Port `ecu_logic.py`'s behavior to C on the STM32 (the DCM/DEM/NvM equivalents)
- Real ISO-TP multi-frame on the STM32 side (this host side already handles it via `can-isotp`)
- Persist the DTC to STM32 flash and restore it at startup (the NvM + EcuM step)
