#!/usr/bin/env python3
"""Reads the serial log of the device, resetting it first unless --no-reset is given.

Needs pyserial, which the ESP-IDF environment provides (. $IDF_PATH/export.sh).
The only line driven on the device is the reset line. Exits with an error when no data arrives.
"""
from __future__ import annotations

import argparse
import glob
import sys
import time

import serial

PORT_PATTERNS = ("/dev/cu.usbmodem*", "/dev/ttyACM*")
BAUD_RATE = 115200
READ_TIMEOUT_S = 0.2
READ_CHUNK_BYTES = 4096
RESET_PULSE_S = 0.1
REOPEN_DELAY_S = 0.3
DEFAULT_SECONDS = 8.0


def find_port() -> str:
    ports = [port for pattern in PORT_PATTERNS for port in glob.glob(pattern)]
    if len(ports) != 1:
        sys.exit(f"Expected one device port, found {ports or 'none'}. Use --port.")
    return ports[0]


def open_port(port: str) -> serial.Serial | None:
    try:
        return serial.Serial(port, BAUD_RATE, timeout=READ_TIMEOUT_S)
    except (serial.SerialException, OSError):
        return None


def reset(connection: serial.Serial) -> None:
    connection.dtr = False
    connection.rts = True
    time.sleep(RESET_PULSE_S)
    connection.rts = False


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="serial port; by default the only device port found")
    parser.add_argument("--seconds", type=float, default=DEFAULT_SECONDS, help="how long to read")
    parser.add_argument("--no-reset", action="store_true", help="read without resetting the device")
    args = parser.parse_args()

    port = args.port or find_port()
    connection = open_port(port)
    if connection is None:
        sys.exit(f"Cannot open {port}.")
    if not args.no_reset:
        reset(connection)

    received_bytes = 0
    deadline = time.monotonic() + args.seconds
    while time.monotonic() < deadline:
        if connection is None:
            time.sleep(REOPEN_DELAY_S)
            connection = open_port(port)
            continue
        try:
            data = connection.read(READ_CHUNK_BYTES)
        except (serial.SerialException, OSError):
            # The USB device enumerates again after a reset.
            connection.close()
            connection = None
            continue
        received_bytes += len(data)
        sys.stdout.write(data.decode(errors="replace"))
        sys.stdout.flush()

    if received_bytes == 0:
        sys.exit(f"No data received from {port}. Reconnect the USB cable and try again.")


if __name__ == "__main__":
    main()
