#!/usr/bin/env python3
"""Verify the SDK HTTP client's compile-time protection for private headers."""

import json
from pathlib import Path
import shlex
import sys


def main(build_dir):
    root = Path(build_dir)
    entries = json.loads((root / "compile_commands.json").read_text(encoding="utf-8"))
    component_files = ("esp_http_client.c", "http_auth.c", "http_header.c", "http_utils.c")
    for name in component_files:
        matches = [entry for entry in entries if Path(entry["file"]).name == name
                   and "/esp_http_client/" in entry["file"]]
        if len(matches) != 1:
            raise ValueError("HTTP client source missing or ambiguous in build metadata")
        definitions = [arg for arg in shlex.split(matches[0]["command"]) if arg.startswith("-DLOG_LOCAL_LEVEL=")]
        if definitions != ["-DLOG_LOCAL_LEVEL=ESP_LOG_INFO"]:
            raise ValueError("HTTP client log cap missing or overridden")

    archive = (root / "esp-idf/esp_http_client/libesp_http_client.a").read_bytes()
    if b"Write header[" in archive or b"auth_response=" in archive:
        raise ValueError("HTTP client includes a private-header debug format")
    print("HTTP client private-header debug logging is compiled out.")


if __name__ == "__main__":
    try:
        if len(sys.argv) != 2:
            raise ValueError("Expected one build directory")
        main(sys.argv[1])
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as error:
        print(f"HTTP client log check failed: {error}", file=sys.stderr)
        sys.exit(1)
