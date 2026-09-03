"""UDS diagnostic tester ("scan tool") — the client side of Phase 4.

Talks UDS over ISO-TP over CAN. By default uses the python-can 'virtual' bus so
it runs against mock_ecu.py with no hardware. To point it at the real STM32 ECU
later, change build_bus() to a real interface (e.g. slcan / an MCP2515 adapter);
nothing else changes.

Usage:
    # terminal 1
    python mock_ecu.py
    # terminal 2
    python tester.py
"""
import isotp
import can
import udsoncan
from udsoncan.connections import PythonIsoTpConnection
from udsoncan.client import Client
import udsoncan.services as services
from ecu_logic import VIN_DID, SENSOR_DID

# Mirror of the ECU's IDs, swapped: tester transmits on 0x7E0, receives 0x7E8.
TESTER_TX_ID = 0x7E0
TESTER_RX_ID = 0x7E8


def build_bus(channel="adasbus"):
    # --- swap THIS line to go from simulation to real hardware ---
    return can.Bus(interface="virtual", channel=channel, receive_own_messages=False)


class RawVinCodec(udsoncan.DidCodec):
    """The VIN is variable-length ASCII; decode it straight to a string."""
    def decode(self, payload): return payload.decode("ascii")
    def encode(self, val): return val.encode("ascii")
    def __len__(self): raise udsoncan.DidCodec.ReadAllRemainingData


class SensorCodec(udsoncan.DidCodec):
    def decode(self, payload): return int.from_bytes(payload, "big")
    def encode(self, val): return int(val).to_bytes(2, "big")
    def __len__(self): return 2


def main(channel="adasbus"):
    bus = build_bus(channel)
    addr = isotp.Address(isotp.AddressingMode.Normal_11bits,
                         txid=TESTER_TX_ID, rxid=TESTER_RX_ID)
    stack = isotp.NotifierBasedCanStack(bus=bus, notifier=can.Notifier(bus, []), address=addr)
    conn = PythonIsoTpConnection(stack)

    config = dict(udsoncan.configs.default_client_config)
    config["data_identifiers"] = {VIN_DID: RawVinCodec, SENSOR_DID: SensorCodec}

    print("[TEST] connecting to ECU...\n")
    with Client(conn, config=config, request_timeout=2) as client:
        # 0x10 — enter an extended diagnostic session
        client.change_session(services.DiagnosticSessionControl.Session.extendedDiagnosticSession)
        print("[TEST] 0x10 extended session  -> OK")

        # 0x22 — read the VIN
        vin = client.read_data_by_identifier(VIN_DID)
        print(f"[TEST] 0x22 read VIN          -> {vin.service_data.values[VIN_DID]!r}")

        # 0x22 — read the distance sensor
        dist = client.read_data_by_identifier(SENSOR_DID)
        print(f"[TEST] 0x22 read sensor (cm)  -> {dist.service_data.values[SENSOR_DID]}")

    print("\n[TEST] done — the scan tool round-tripped UDS with the ECU.")
    bus.shutdown()


if __name__ == "__main__":
    main()
