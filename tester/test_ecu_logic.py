"""Host-side unit tests for the ECU's diagnostic logic — no CAN, no hardware.

This is the discipline the ADAS pipeline is built around: test the logic on the
desk in milliseconds, so only genuinely hardware-dependent behavior waits for the
board in the evening. Run: pytest -v
"""
from ecu_logic import (
    EcuLogic, VIN_DID, SENSOR_DID, SENSOR_FAULT_DTC,
    DTC_TEST_FAILED, DTC_CONFIRMED,
)


def test_read_vin_did():
    ecu = EcuLogic(vin="TESTVIN0001")
    assert ecu.read_did(VIN_DID) == b"TESTVIN0001"


def test_read_sensor_did_is_two_bytes_big_endian():
    ecu = EcuLogic()
    ecu.sensor_cm = 300
    assert ecu.read_did(SENSOR_DID) == (300).to_bytes(2, "big")


def test_unknown_did_returns_none():
    assert EcuLogic().read_did(0xDEAD) is None


def test_in_range_sensor_raises_no_dtc():
    ecu = EcuLogic()
    ecu.update_sensor(120)          # within [5, 400]
    assert ecu.read_dtcs() == {}


def test_out_of_range_sensor_raises_confirmed_dtc():
    ecu = EcuLogic()
    ecu.update_sensor(999)          # above max
    status = ecu.read_dtcs()[SENSOR_FAULT_DTC]
    assert status & DTC_TEST_FAILED
    assert status & DTC_CONFIRMED


def test_clear_dtcs_empties_the_store():
    ecu = EcuLogic()
    ecu.update_sensor(999)
    assert ecu.read_dtcs()          # non-empty
    ecu.clear_dtcs()
    assert ecu.read_dtcs() == {}


def test_dtc_survives_a_power_cycle_via_snapshot_restore():
    # models NvM: store on shutdown, restore at startup
    ecu = EcuLogic()
    ecu.update_sensor(999)
    saved = ecu.snapshot()

    rebooted = EcuLogic()           # fresh power-on, empty
    assert rebooted.read_dtcs() == {}
    rebooted.restore(saved)         # EcuM triggers NvM restore
    assert SENSOR_FAULT_DTC in rebooted.read_dtcs()
