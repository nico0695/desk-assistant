# Desk Assistant

Firmware for a desk voice and notification terminal. It records a voice message and sends it to an HTTPS bridge, shows the reply, lists tasks, notes and alerts, and receives durable notifications.

## Hardware

Waveshare ESP32-C6-Touch-AMOLED-2.16: ESP32-C6, 16 MB flash, 480×480 touch AMOLED, AXP2101 PMIC, ES7210 microphone codec and ES8311 speaker codec. No PSRAM.

## Requirements

- ESP-IDF 5.5.2 exported in the shell (`. <esp-idf>/export.sh`).
- Dependencies are pinned in `dependencies.lock` and fetched on the first build.

## Build

```sh
idf.py -B build-app -D SDKCONFIG="$PWD/build-app/sdkconfig" \
  -D SDKCONFIG_DEFAULTS="sdkconfig.defaults" build
```

## Tests

```sh
tools/test.sh
```

Runs the host tests with CMake, Ninja and the host C compiler. It does not need the ESP-IDF environment exported: Unity is taken from `IDF_PATH`, or from `~/esp/esp-idf-v5.5.2` when it is not set.

## UI preview

```sh
tools/preview.sh
```

Renders the screens to PNG in `build-preview/screens/` with the pinned LVGL on the host. It needs `managed_components/`, created by the first build.

The Latin-1 fallback fonts are generated C files. Regenerate them with `tools/generate_fonts.py` (Pillow 11.3.0 and FreeType 2.13.3), or verify them with `--check`.

## Flash

```sh
idf.py -B build-app -D SDKCONFIG="$PWD/build-app/sdkconfig" \
  -D SDKCONFIG_DEFAULTS="sdkconfig.defaults" -p PORT flash monitor
```

## Provisioning

Export ESP-IDF first to use its Python and pyserial. Verify the port and chip before operating the device, and close any serial monitor using that port.

```sh
python tools/provision.py --port PORT
```

The tool prompts privately for the Wi-Fi network, password, HTTPS bridge base URL and device token, and asks for the screen timeout. To update only the bridge URL and token, preserving Wi-Fi and settings:

```sh
python tools/provision.py --port PORT --bridge-only
```

`--env-file PATH` reads an owner-only regular file outside the repository instead of prompting. Full configuration accepts `WIFI_SSID`, `WIFI_PASSWORD`, `BRIDGE_URL`, `DEVICE_TOKEN` and `SCREEN_TIMEOUT` as `KEY=value` lines. Bridge-only files contain exactly `BRIDGE_URL` and `DEVICE_TOKEN`. Keep credentials out of command arguments and Git. A successful reply confirms storage, not bridge availability; remote bridge communication starts in stage 4.

For Wi-Fi setup from a phone, open Settings → Wi-Fi → Add network. Join the temporary access point using the QR code or the credentials shown on the device, then open `http://192.168.4.1` and submit the network name and password. This form changes Wi-Fi only. Setup opens automatically on boot without saved networks; Back leaves it running and Cancel setup closes it.

## License

MIT. Third-party notices are listed in `THIRD_PARTY_NOTICES.md` when third-party code or assets are included.
