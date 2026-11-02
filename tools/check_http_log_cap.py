#!/usr/bin/env python3
"""Verify SDK HTTP and TLS compile-time protection for private configuration."""

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
        if definitions != ["-DLOG_LOCAL_LEVEL=ESP_LOG_NONE"]:
            raise ValueError("HTTP client log cap missing or overridden")

    archive = (root / "esp-idf/esp_http_client/libesp_http_client.a").read_bytes()
    if any(value in archive for value in (b"Write header[", b"auth_response=", b"Error parse url %s",
                                           b"This authentication method is not supported: %s")):
        raise ValueError("HTTP client includes a private-header debug format")

    for name, level in (("esp_tls.c", "ESP_LOG_NONE"), ("esp_tls_mbedtls.c", "ESP_LOG_INFO")):
        matches = [entry for entry in entries if Path(entry["file"]).name == name
                   and "/esp-tls/" in entry["file"]]
        if len(matches) != 1:
            raise ValueError("TLS source missing or ambiguous in build metadata")
        definitions = [arg for arg in shlex.split(matches[0]["command"]) if arg.startswith("-DLOG_LOCAL_LEVEL=")]
        if definitions != [f"-DLOG_LOCAL_LEVEL={level}"]:
            raise ValueError("TLS log cap missing or overridden")
    tls_archive = (root / "esp-idf/esp-tls/libesp-tls.a").read_bytes()
    if any(value in tls_archive for value in (b"couldn't get hostname for", b"host:%s: strlen",
                                               b"Connecting to server. HOST:", b"verification info: %s")):
        raise ValueError("TLS library includes a private-host debug or error format")
    print("HTTP and TLS private-configuration logging is compiled out.")


if __name__ == "__main__":
    try:
        if len(sys.argv) != 2:
            raise ValueError("Expected one build directory")
        main(sys.argv[1])
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as error:
        print(f"HTTP client log check failed: {error}", file=sys.stderr)
        sys.exit(1)
