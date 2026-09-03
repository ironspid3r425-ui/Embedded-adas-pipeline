"""Mock ECU server: answers UDS requests over (virtual) CAN + ISO-TP.

Stands in for the STM32 until the firmware exists, so the tester can be
developed and proven with zero hardware. Later, point tester.py at a real
CAN adapter instead of the virtual bus and this file is no longer needed —
the STM32 plays this role.

Run this in one terminal, then run tester.py in another. Both default to the
'virtual' python-can bus on channel 'adasbus', so they talk to each other
in-process-group with no hardware.
"""
import isotp
import can
from udsoncan import DidCodec
import udsoncan.services as services
from udsoncan import Response

from ecu_logic import (
    EcuLogic, VIN_DID, SENSOR_DID, SENSOR_FAULT_DTC,
)

# ISO-TP addressing: tester sends on 0x7E0 (request), ECU replies on 0x7E8.
# These are the conventional OBD-II diagnostic CAN IDs.
ECU_RX_ID = 0x7E0
ECU_TX_ID = 0x7E8


def build_stack(bus):
    addr = isotp.Address(isotp.AddressingMode.Normal_11bits,
                         txid=ECU_TX_ID, rxid=ECU_RX_ID)
    return isotp.NotifierBasedCanStack(
        bus=bus, notifier=can.Notifier(bus, []), address=addr)


def serve(channel="adasbus"):
    import time
    ecu = EcuLogic()
    bus = can.Bus(interface="virtual", channel=channel, receive_own_messages=False)
    stack = build_stack(bus)
    stack.start()  # NotifierBasedCanStack runs its own background thread; don't call process()
    print(f"[ECU]  online — VIN={ecu.vin!r}, listening on ISO-TP {hex(ECU_RX_ID)}->{hex(ECU_TX_ID)}")

    try:
        while True:
            if stack.available():
                req_bytes = stack.recv()
                resp = handle_request(ecu, req_bytes)
                if resp is not None:
                    stack.send(resp)
            time.sleep(0.005)
    except KeyboardInterrupt:
        print("\n[ECU]  shutting down")
    finally:
        stack.stop()
        bus.shutdown()


def handle_request(ecu: EcuLogic, data: bytes) -> bytes | None:
    """Minimal UDS request router. Returns raw response bytes."""
    if not data:
        return None
    sid = data[0]

    # 0x10 DiagnosticSessionControl -> positive echo of the sub-function
    if sid == services.DiagnosticSessionControl.request_id():
        sub = data[1] if len(data) > 1 else 0x01
        print(f"[ECU]  0x10 session control -> session {sub}")
        # UDS-2020 requires 4 timing bytes: P2_server_max (ms) + P2*_server_max (10 ms units)
        p2 = (50).to_bytes(2, "big")        # 50 ms
        p2_star = (500).to_bytes(2, "big")  # 500 * 10 ms = 5 s
        return bytes([sid + 0x40, sub]) + p2 + p2_star

    # 0x22 ReadDataByIdentifier
    if sid == services.ReadDataByIdentifier.request_id():
        did = int.from_bytes(data[1:3], "big")
        value = ecu.read_did(did)
        if value is None:
            print(f"[ECU]  0x22 read DID {hex(did)} -> requestOutOfRange")
            return _nrc(sid, 0x31)  # requestOutOfRange
        print(f"[ECU]  0x22 read DID {hex(did)} -> {value!r}")
        return bytes([sid + 0x40]) + data[1:3] + value

    # 0x19 ReadDTCInformation (sub 0x02: report DTCs by status mask)
    if sid == services.ReadDTCInformation.request_id():
        dtcs = ecu.read_dtcs()
        print(f"[ECU]  0x19 read DTCs -> {[hex(d) for d in dtcs]}")
        body = bytes([sid + 0x40, 0x02, 0xFF])
        for dtc, status in dtcs.items():
            body += dtc.to_bytes(3, "big") + bytes([status])
        return body

    # 0x14 ClearDiagnosticInformation
    if sid == services.ClearDiagnosticInformation.request_id():
        ecu.clear_dtcs()
        print("[ECU]  0x14 clear DTCs -> ok")
        return bytes([sid + 0x40])

    print(f"[ECU]  unsupported service {hex(sid)} -> serviceNotSupported")
    return _nrc(sid, 0x11)  # serviceNotSupported


def _nrc(sid: int, code: int) -> bytes:
    """Negative response: 0x7F, echoed SID, negative response code."""
    return bytes([0x7F, sid, code])


if __name__ == "__main__":
    serve()
