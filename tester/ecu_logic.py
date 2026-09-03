"""Pure diagnostic logic for the mock ECU — no CAN, no hardware, no I/O.

This is the part that later becomes C on the STM32 (the DCM/DEM/NvM equivalents).
Keeping it free of transport and hardware is deliberate: it makes the logic
host-unit-testable, which is exactly the discipline the ADAS pipeline is built
around. See test_ecu_logic.py.
"""

# --- Data Identifiers (DIDs) this ECU answers, à la a UDS 0x22 read ---
VIN_DID = 0xF190          # standard DID for the Vehicle Identification Number
SENSOR_DID = 0x0100       # a made-up "front distance sensor (cm)" reading

# --- DTC status bits (a simplified slice of the ISO 14229 status byte) ---
DTC_TEST_FAILED = 0x01            # bit 0: fault active right now
DTC_CONFIRMED = 0x08             # bit 3: fault stored/confirmed

SENSOR_FAULT_DTC = 0xC10100      # our one trouble code: "sensor out of range"
SENSOR_MIN, SENSOR_MAX = 5, 400  # valid distance window, in cm


class EcuLogic:
    """Holds ECU state and answers diagnostic questions. Transport-agnostic."""

    def __init__(self, vin="NIKHILADASECU00001"):
        self.vin = vin
        self.sensor_cm = 120            # current sensor reading
        self.dtc_status = {}            # dtc_number -> status byte (the DEM's job)

    # ---- 0x22 Read Data By Identifier (DCM front door) ----
    def read_did(self, did: int) -> bytes | None:
        if did == VIN_DID:
            return self.vin.encode("ascii")
        if did == SENSOR_DID:
            return self.sensor_cm.to_bytes(2, "big")
        return None  # unknown DID -> caller responds with a negative response

    # ---- fault monitoring (the DEM's event logic) ----
    def update_sensor(self, value_cm: int) -> None:
        """Feed a new reading; raise or confirm a DTC if it's out of range."""
        self.sensor_cm = value_cm
        if value_cm < SENSOR_MIN or value_cm > SENSOR_MAX:
            prev = self.dtc_status.get(SENSOR_FAULT_DTC, 0)
            self.dtc_status[SENSOR_FAULT_DTC] = prev | DTC_TEST_FAILED | DTC_CONFIRMED

    # ---- 0x19 Read DTC Information ----
    def read_dtcs(self) -> dict[int, int]:
        return dict(self.dtc_status)

    # ---- 0x14 Clear Diagnostic Information ----
    def clear_dtcs(self) -> None:
        self.dtc_status.clear()

    # ---- NvM stand-in: what survives a power cycle ----
    def snapshot(self) -> dict:
        """Persist-on-shutdown; on the STM32 this becomes a flash write."""
        return {"dtc_status": dict(self.dtc_status)}

    def restore(self, snap: dict) -> None:
        """Restore-at-startup; on the STM32, EcuM triggers this from flash."""
        self.dtc_status = dict(snap.get("dtc_status", {}))
