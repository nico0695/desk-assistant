# Desk Assistant

Firmware for a desk voice and notification terminal. It records a voice message and sends it to an HTTPS bridge, shows the reply, lists tasks, notes and alerts, and receives durable notifications.

## Hardware

Waveshare ESP32-C6-Touch-AMOLED-2.16: ESP32-C6, 16 MB flash, 480×480 touch AMOLED, AXP2101 PMIC, ES7210 microphone codec and ES8311 speaker codec. No PSRAM.

## Requirements

- ESP-IDF 5.5.2 exported in the shell (`. $IDF_PATH/export.sh`).
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

Runs the host tests. It does not need the ESP-IDF environment exported.

## Flash

```sh
idf.py -B build-app -D SDKCONFIG="$PWD/build-app/sdkconfig" \
  -D SDKCONFIG_DEFAULTS="sdkconfig.defaults" -p PORT flash monitor
```

## Provisioning

Wi-Fi and bridge provisioning will be documented with the connectivity stage.

## License

MIT. Third-party notices are listed in `THIRD_PARTY_NOTICES.md` when third-party code or assets are included.
