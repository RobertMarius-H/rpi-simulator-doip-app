#!/usr/bin/env python3
"""Test rapid al conexiunii DoIP cu simulatorul (sau cu Pi-ul real).

Rulează pe Windows sau în WSL, doar cu biblioteca standard Python:
    python tools/doip_smoke_test.py [host] [port]

Pași verificați:
  1. conexiune TCP
  2. Routing Activation (0x0005 -> 0x0006, cod 0x10)
  3. UDS 10 01  (sesiune implicită)    -> 50 01 ...
  4. UDS 10 02  (sesiune de programare) -> 50 02 ...
  5. UDS 27 01  (cerere seed)          -> 67 01 <seed>
Nu trimite cheia de securitate și nu transferă fișiere; pentru asta folosiți aplicația RpiDiag.
"""

import socket
import struct
import sys

PROTOCOL_VERSION = 0x03
INVERSE_VERSION = 0xFC
TESTER_ADDRESS = 0x0E80
PI_ADDRESS = 0x1000

ROUTING_REQUEST = 0x0005
ROUTING_RESPONSE = 0x0006
ALIVE_REQUEST = 0x0007
ALIVE_RESPONSE = 0x0008
DIAGNOSTIC = 0x8001
DIAGNOSTIC_ACK = 0x8002
DIAGNOSTIC_NACK = 0x8003
GENERIC_NACK = 0x0000

TIMEOUT_S = 6.0


def frame(payload_type: int, payload: bytes) -> bytes:
    return struct.pack(">BBHI", PROTOCOL_VERSION, INVERSE_VERSION, payload_type, len(payload)) + payload


def recv_exact(sock: socket.socket, size: int) -> bytes:
    data = b""
    while len(data) < size:
        chunk = sock.recv(size - len(data))
        if not chunk:
            raise ConnectionError("Serverul a închis conexiunea.")
        data += chunk
    return data


def read_frame(sock: socket.socket):
    header = recv_exact(sock, 8)
    version, inverse, payload_type, length = struct.unpack(">BBHI", header)
    if version != PROTOCOL_VERSION or inverse != INVERSE_VERSION:
        raise ValueError(f"Antet DoIP neașteptat: {header.hex(' ')}")
    return payload_type, recv_exact(sock, length)


def read_relevant_frame(sock: socket.socket):
    """Răspunde automat la alive-check și returnează următorul cadru util."""
    while True:
        payload_type, payload = read_frame(sock)
        if payload_type == ALIVE_REQUEST:
            sock.sendall(frame(ALIVE_RESPONSE, struct.pack(">H", TESTER_ADDRESS)))
            print("   (alive-check primit, răspuns trimis)")
            continue
        return payload_type, payload


def uds(sock: socket.socket, request: bytes) -> bytes:
    sock.sendall(frame(DIAGNOSTIC, struct.pack(">HH", TESTER_ADDRESS, PI_ADDRESS) + request))
    payload_type, payload = read_relevant_frame(sock)
    if payload_type == DIAGNOSTIC_NACK:
        raise RuntimeError(f"Diagnostic NACK: {payload.hex(' ')}")
    if payload_type != DIAGNOSTIC_ACK:
        raise RuntimeError(f"Așteptam ACK 0x8002, am primit 0x{payload_type:04X}")
    while True:
        payload_type, payload = read_relevant_frame(sock)
        if payload_type != DIAGNOSTIC:
            raise RuntimeError(f"Așteptam răspuns 0x8001, am primit 0x{payload_type:04X}")
        response = payload[4:]
        # 7F <SID> 78 = "response pending"; așteptăm răspunsul final.
        if len(response) >= 3 and response[0] == 0x7F and response[2] == 0x78:
            continue
        return response


def check(label: str, condition: bool, detail: str) -> None:
    status = "OK " if condition else "EROARE"
    print(f"[{status}] {label}: {detail}")
    if not condition:
        sys.exit(1)


def main() -> int:
    host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 13400
    print(f"Conectare la {host}:{port} ...")
    try:
        sock = socket.create_connection((host, port), timeout=TIMEOUT_S)
    except OSError as error:
        print(f"[EROARE] Conexiune TCP eșuată: {error}")
        print("Verificați că simulatorul rulează (./sim.sh status) și adresa/portul.")
        return 1

    with sock:
        sock.settimeout(TIMEOUT_S)
        check("TCP", True, f"conectat la {host}:{port}")

        sock.sendall(frame(ROUTING_REQUEST, struct.pack(">HB4s", TESTER_ADDRESS, 0x00, b"\x00" * 4)))
        payload_type, payload = read_relevant_frame(sock)
        code = payload[4] if payload_type == ROUTING_RESPONSE and len(payload) >= 5 else None
        check("Routing activation", code == 0x10,
              f"tip 0x{payload_type:04X}, payload {payload.hex(' ')}")

        response = uds(sock, bytes([0x10, 0x01]))
        check("UDS 10 01", response[:2] == bytes([0x50, 0x01]), response.hex(" "))

        response = uds(sock, bytes([0x10, 0x02]))
        check("UDS 10 02", response[:2] == bytes([0x50, 0x02]), response.hex(" "))

        response = uds(sock, bytes([0x27, 0x01]))
        check("UDS 27 01", response[:2] == bytes([0x67, 0x01]),
              f"seed de {max(0, len(response) - 2)} octeți")

        # Revenim în sesiunea implicită ca să lăsăm serverul curat.
        uds(sock, bytes([0x10, 0x01]))

    print("\nSimulatorul răspunde corect. Puteți folosi aplicația RpiDiag.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
