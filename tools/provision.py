#!/usr/bin/env python3
"""Private USB provisioning of the bridge origin and, optionally, a Wi-Fi network.

Secrets are never accepted as arguments: they are typed at hidden prompts or read from an owner-only file outside
the repository. The device validates every value; this tool only rejects empty fields.
"""

import argparse
import getpass
import json
import os
from pathlib import Path
import stat
import sys
import time
import warnings

ROOT = Path(__file__).resolve().parents[1]
FIELDS = {"WIFI_SSID", "WIFI_PASSWORD", "BRIDGE_URL", "DEVICE_TOKEN", "SCREEN_TIMEOUT"}
BRIDGE_FIELDS = {"BRIDGE_URL", "DEVICE_TOKEN"}
DEFAULT_SCREEN_TIMEOUT = "30"
REPLY_PREFIX = '{"v":1,'
REPLY_TIMEOUT_S = 12
MAX_LINE_BYTES = 4096


class DeviceRejectedError(ValueError):
    """The device replied with a negative acknowledgement."""


def private_environment(path):
    resolved = Path(path).expanduser().resolve(strict=True)
    if resolved == ROOT or ROOT in resolved.parents:
        raise ValueError("Private configuration must be outside the repository")
    info = resolved.stat()
    if not stat.S_ISREG(info.st_mode) or info.st_mode & 0o077:
        raise ValueError("Private configuration must be a regular owner-only file")
    if hasattr(os, "getuid") and info.st_uid != os.getuid():
        raise ValueError("Private configuration must belong to the current user")
    values = {}
    for line in resolved.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        key, separator, value = line.partition("=")
        key = key.strip()
        if not separator or key not in FIELDS or key in values:
            raise ValueError("Invalid private configuration format")
        value = value.strip()
        if len(value) >= 2 and value[0] == value[-1] and value[0] in "\"'":
            value = value[1:-1]
        values[key] = value
    return values


def prompt_values(bridge_only):
    if not sys.stdin.isatty():
        raise ValueError("Private terminal required")
    values = {}
    with warnings.catch_warnings():
        warnings.simplefilter("error", getpass.GetPassWarning)
        if not bridge_only:
            values["WIFI_SSID"] = getpass.getpass("Wi-Fi network (hidden): ")
            values["WIFI_PASSWORD"] = getpass.getpass("Wi-Fi password (empty for an open network): ")
            values["SCREEN_TIMEOUT"] = (input("Screen timeout in seconds, 15, 30, 60 or 0 for always on [30]: ")
                                        or DEFAULT_SCREEN_TIMEOUT)
        values["BRIDGE_URL"] = getpass.getpass("HTTPS bridge base URL (hidden): ")
        values["DEVICE_TOKEN"] = getpass.getpass("Device token: ")
    return values


def build_request(values, bridge_only):
    if bridge_only:
        if set(values) != BRIDGE_FIELDS:
            raise ValueError("Bridge-only configuration takes only the URL and the token")
    elif not values.get("WIFI_SSID"):
        raise ValueError("Empty Wi-Fi network")
    if not values.get("BRIDGE_URL") or not values.get("DEVICE_TOKEN"):
        raise ValueError("Empty bridge URL or token")
    request = {"v": 1, "op": "configure_bridge" if bridge_only else "configure",
               "bridge_url": values["BRIDGE_URL"], "device_token": values["DEVICE_TOKEN"]}
    if not bridge_only:
        request.update({"wifi_ssid": values["WIFI_SSID"], "wifi_password": values.get("WIFI_PASSWORD", ""),
                        "screen_timeout_s": int(values.get("SCREEN_TIMEOUT", DEFAULT_SCREEN_TIMEOUT))})
    return request


def device_reply(line):
    """The protocol reply in a line that may also carry log text, or None."""
    start = line.find(REPLY_PREFIX)
    if start < 0:
        return None
    try:
        reply = json.loads(line[start:])
    except ValueError:
        return None
    if isinstance(reply, dict) and reply.get("v") == 1 and isinstance(reply.get("ok"), bool):
        return reply
    return None


def exchange(serial_port, request):
    # Default modem lines: lowering DTR before RTS after opening is the USB Serial/JTAG reset sequence.
    serial_port.open()
    try:
        serial_port.reset_input_buffer()
        payload = json.dumps(request, ensure_ascii=True, separators=(",", ":")).encode() + b"\n"
        serial_port.write(payload)
        serial_port.flush()
        deadline = time.monotonic() + REPLY_TIMEOUT_S
        while time.monotonic() < deadline:
            raw = serial_port.readline(MAX_LINE_BYTES + 1)
            if len(raw) > MAX_LINE_BYTES:
                continue
            reply = device_reply(raw.decode("utf-8", errors="replace"))
            if reply is not None:
                if not reply["ok"]:
                    raise DeviceRejectedError("Device rejected configuration")
                return
        raise TimeoutError("Configuration confirmation not received")
    finally:
        serial_port.close()


def failure(message):
    # Messages are fixed at call sites. Never include exception text or payloads.
    print(message, file=sys.stderr)
    return 1


def main(argv=None):
    class PrivateParser(argparse.ArgumentParser):
        def error(self, message):
            self.exit(2, "Invalid arguments. Use --port, optional --env-file and --bridge-only only.\n")

    parser = PrivateParser(description="Configure the device privately over USB")
    parser.add_argument("--port", required=True, help="Verified serial device path")
    parser.add_argument("--env-file", help="Owner-only configuration file outside the repository")
    parser.add_argument("--bridge-only", action="store_true", help="Update only the bridge URL and device token")
    args = parser.parse_args(argv)
    try:
        values = private_environment(args.env_file) if args.env_file else prompt_values(args.bridge_only)
    except (ValueError, OSError, UnicodeError, EOFError, getpass.GetPassWarning):
        return failure("Private input unavailable; use a hidden terminal or an owner-only file outside the "
                       "repository.")
    try:
        request = build_request(values, args.bridge_only)
    except ValueError:
        return failure("Invalid configuration; the network (unless --bridge-only), URL and token are required and "
                       "the timeout is a number.")
    try:
        import serial  # Provided by the ESP-IDF Python environment.
    except ImportError:
        return failure("pyserial unavailable; export the ESP-IDF environment first.")
    try:
        port = serial.Serial(port=None, baudrate=115200, timeout=0.5, write_timeout=5, exclusive=True)
        port.port = args.port
        exchange(port, request)
    except DeviceRejectedError:
        return failure("Device rejected the configuration or could not store it.")
    except TimeoutError:
        return failure("No confirmation from the device; it may have stored the configuration. Check it before "
                       "retrying.")
    except (ValueError, OSError):
        return failure("USB communication failed; check the port and that no serial monitor is using it.")
    print("Configuration stored by the device.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
