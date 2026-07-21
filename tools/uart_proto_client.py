#!/usr/bin/env python3
"""
Host-side test client for SentinelOS's framed UART command protocol
(firmware/components/protocol). Shares the same UART0 as the log console --
this client only reacts to bytes matching the frame magic and ignores
everything else (log lines included).

Frame: MAGIC(4) | LEN u16 LE | TYPE(1) | PAYLOAD(LEN) | CRC16 u16 LE
CRC16 is CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF) over TYPE+PAYLOAD.
"""

import argparse
import hashlib
import hmac
import os
import struct
import sys
import time

import serial

MAGIC = bytes([0xAA, 0x55, 0xC3, 0x3C])

CMD_PING = 0x01
CMD_GET_INFO = 0x02
CMD_OTA_CHECK = 0x03
CMD_IFF_CHALLENGE = 0x04
CMD_IFF_RESPONSE = 0x05

RESP_OK = 0x80
RESP_PONG = 0x81
RESP_INFO = 0x82
RESP_IFF_CHALLENGE = 0x83
RESP_IFF_RESULT = 0x84
RESP_ERROR = 0xFF

RESP_NAMES = {
    RESP_OK: "OK",
    RESP_PONG: "PONG",
    RESP_INFO: "INFO",
    RESP_IFF_CHALLENGE: "IFF_CHALLENGE",
    RESP_IFF_RESULT: "IFF_RESULT",
    RESP_ERROR: "ERROR",
}

# Doit matcher firmware/components/protocol/iff_secret.h -- démo uniquement,
# voir l'avertissement dans ce fichier sur pourquoi ce n'est pas un vrai
# schéma de prod (secret symétrique en dur des deux côtés).
IFF_SECRET_KEY = bytes(
    [
        0x66, 0x85, 0x0F, 0x96, 0xEE, 0x48, 0xB4, 0xC8,
        0x66, 0xE4, 0x53, 0x06, 0xF8, 0x4F, 0x0A, 0xE4,
        0xA4, 0x02, 0x16, 0xBC, 0xD8, 0x3A, 0xD0, 0x1C,
        0x57, 0x91, 0xB6, 0x54, 0xD8, 0x84, 0xA7, 0x59,
    ]
)


def crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if (crc & 0x8000) else (crc << 1) & 0xFFFF
    return crc


def build_frame(cmd_type: int, payload: bytes = b"") -> bytes:
    body = bytes([cmd_type]) + payload
    crc = crc16_ccitt(body)
    return MAGIC + struct.pack("<H", len(payload)) + body + struct.pack("<H", crc)


def read_frame(ser: serial.Serial, timeout_s: float = 3.0):
    """Scan the stream for one valid frame, ignoring interleaved log text."""
    deadline = time.time() + timeout_s
    magic_idx = 0

    while time.time() < deadline:
        byte = ser.read(1)
        if not byte:
            continue
        b = byte[0]

        if b == MAGIC[magic_idx]:
            magic_idx += 1
            if magic_idx == len(MAGIC):
                break
        else:
            magic_idx = 1 if b == MAGIC[0] else 0
    else:
        return None

    header = ser.read(3)
    if len(header) < 3:
        return None
    payload_len = header[0] | (header[1] << 8)
    resp_type = header[2]

    payload = ser.read(payload_len)
    crc_bytes = ser.read(2)
    if len(payload) < payload_len or len(crc_bytes) < 2:
        return None

    crc_received = crc_bytes[0] | (crc_bytes[1] << 8)
    crc_computed = crc16_ccitt(bytes([resp_type]) + payload)
    if crc_received != crc_computed:
        print("! CRC mismatch on response, dropping", file=sys.stderr)
        return None

    return resp_type, payload


def decode_info(payload: bytes) -> str:
    uptime, free_heap, min_heap = struct.unpack_from("<III", payload, 0)
    version_len = payload[12]
    version = payload[13 : 13 + version_len].decode("ascii", errors="replace")
    return (
        f"version={version} uptime={uptime}s "
        f"free_heap={free_heap}B min_free_heap={min_heap}B"
    )


def send_command(ser: serial.Serial, cmd_type: int) -> None:
    ser.write(build_frame(cmd_type))
    result = read_frame(ser)
    if result is None:
        print("! no response (timeout)")
        return

    resp_type, payload = result
    name = RESP_NAMES.get(resp_type, f"0x{resp_type:02x}")

    if resp_type == RESP_INFO:
        print(f"{name}: {decode_info(payload)}")
    elif resp_type in (RESP_OK, RESP_ERROR) and payload:
        print(f"{name}: status={payload[0]}")
    else:
        print(f"{name}: payload={payload.hex()}")


def do_iff(ser: serial.Serial, wrong_key: bool) -> None:
    """IFF challenge/response demo: request a nonce, prove knowledge of
    IFF_SECRET_KEY (or deliberately don't, with --wrong-key) via
    HMAC-SHA256, and print whether the device classifies us FRIEND or
    UNKNOWN."""
    ser.write(build_frame(CMD_IFF_CHALLENGE))
    result = read_frame(ser)
    if result is None or result[0] != RESP_IFF_CHALLENGE:
        print("! no/invalid challenge response")
        return

    nonce = result[1]
    print(f"IFF_CHALLENGE: nonce={nonce.hex()}")

    key = os.urandom(32) if wrong_key else IFF_SECRET_KEY
    mac = hmac.new(key, nonce, hashlib.sha256).digest()

    ser.write(build_frame(CMD_IFF_RESPONSE, mac))
    result = read_frame(ser)
    if result is None or result[0] != RESP_IFF_RESULT:
        print("! no/invalid IFF result")
        return

    status = result[1][0] if result[1] else 0
    print(f"IFF_RESULT: {'FRIEND' if status else 'UNKNOWN'}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="/dev/ttyUSB0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument(
        "--wrong-key",
        action="store_true",
        help="iff command only: answer with a bogus key to demo the UNKNOWN case",
    )
    parser.add_argument(
        "command",
        choices=["ping", "info", "ota-check", "iff"],
        help="Command to send",
    )
    args = parser.parse_args()

    cmd_map = {"ping": CMD_PING, "info": CMD_GET_INFO, "ota-check": CMD_OTA_CHECK}

    with serial.Serial(args.port, args.baud, timeout=1) as ser:
        if args.command == "iff":
            do_iff(ser, args.wrong_key)
        else:
            send_command(ser, cmd_map[args.command])

    return 0


if __name__ == "__main__":
    sys.exit(main())
