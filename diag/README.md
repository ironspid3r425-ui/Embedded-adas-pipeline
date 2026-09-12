# Portable Diagnostic Logic + Host Unit Tests (Phase 4)

The firmware-side core of the [Phase 4 diagnostics](../ROADMAP.md) work, written as
**portable C99 with no hardware dependencies and no dynamic memory** — so the exact
same code is unit-tested on the host here and compiled into the STM32 firmware, where
it's driven by the ISO-TP / CAN receive path.

It's the C mirror of the Python reference in [`../tester/`](../tester/), and each piece
maps to a real AUTOSAR module:

| Function | Role | ↔ AUTOSAR |
|---|---|---|
| `diag_handle_request` | UDS request router (0x10 / 0x22 / 0x19 / 0x14) | DCM |
| `diag_update_sensor` + DTC store | fault detection, DTC raise/clear | DEM |
| `diag_snapshot` / `diag_restore` | persist across power cycle | NvM + EcuM restore |

## Run the tests (no hardware)

Needs only `gcc` + `make`:

```bash
cd diag
make test
```

Expected: `12 Tests 0 Failures 0 Ignored / OK`. The suite uses
[Unity](https://github.com/ThrowTheSwitch/Unity) (vendored under `vendor/unity/`),
the standard embedded C unit-test framework.

## Layout

```
diag/
├── src/diag.h, diag.c      portable logic under test
├── test/test_diag.c        Unity tests (host)
├── vendor/unity/           Unity v2.6.0 (committed so the build is self-contained)
└── Makefile                `make test`
```

## Why host tests, not just on-target checks

Most diagnostic logic — DID reads, the DTC state machine, the UDS response framing —
has no hardware dependency. Testing it on the desk in milliseconds means the evening
hardware sessions are spent confirming timing and wiring, not rediscovering logic bugs
on target. `diag.c` deliberately keeps all HAL/CAN calls out of this layer so it stays
testable; the firmware wires `diag_handle_request` into the ISO-TP receive callback.

## On target

`diag.c` compiles unchanged with the ARM toolchain. In the firmware, an incoming
UDS request (reassembled by ISO-TP from CAN frames) is passed to
`diag_handle_request`, and the returned bytes are sent back over ISO-TP — the same
contract the host tests exercise.
