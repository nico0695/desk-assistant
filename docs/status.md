# Status

## Stages

| Stage | Scope | Status |
|---|---|---|
| 0 | Scaffold | Done (validated 2026-11-01) |
| 1 | Board and base UI | In progress |
| 2 | Storage and settings | Pending |
| 3 | Wi-Fi and provisioning | Pending |
| 4 | Bridge base | Pending |
| 5a | Recording: local capture | Pending |
| 5b | Recording: upload and result | Pending |
| 6 | Notifications and sound | Pending |
| 7 | Modules: read | Pending |
| 8 | Modules: actions | Pending |
| 9 | Full acceptance | Pending |
| 10 | Battery (optional) | Pending |

## Active stage: 1 — Board and base UI

- **Goal:** interaction hardware working and a reusable visual base.
- **Decisions:** V4 look with larger type (20/24/28 px) and 72 px buttons; icons with labels; Latin-1 fallback fonts regenerated; one UI task that owns LVGL and sleeps when idle; touch by interrupt; no physical power measurement (idle checked in software); panel off with display OFF, no sleep-in.
- **Progress:**
  - [x] 1. Pure board logic with host tests.
  - [x] 2. UI base on the host with PNG preview (design review pending).
  - [x] 3. Board drivers.
  - [x] 4. UI on the board (boots clean; image, touch and navigation pending the smoke test).
  - [x] 5. Screen power, KEY and wake gesture (timeout and idle verified; KEY and touch pending the smoke test).
  - [x] 6. Stage review and fixes; checkpoint waiting for the smoke test and user validation.

## Closed stages

- **0 — Scaffold:** repository conventions, ESP-IDF project, partition table, memory diagnostics, host tests and CI. Validated on `esp32-02` at `3cad47c`.

## Evidence

Each checkpoint records one entry per image:

- Date, revision, build profile and ELF SHA-256 of the application (printed at boot).
- `build_verified`: yes or no, with the command used.
- `hardware_verified`: yes, no or partial, with unit, power, port, executed cases, observations, failures and cases not run.

### Stage 0 — 2026-11-01

- **Revision:** `3cad47c`, integrated profile (`sdkconfig.defaults`).
- **ELF SHA-256:** `2a098538b64a2814e3094b7354eb3c7c095eaa52ba9e3cc948dfcf6362bc2971`. Application image 143184 bytes, 97% of the 4 MB app partition free.
- **build_verified:** yes. Local build with the documented command; CI run on the same revision passed (host tests, build, lock unchanged).
- **hardware_verified:** yes, boot only.
  - Unit `esp32-02`: ESP32-C6 (QFN40) revision v0.2, 16 MB flash, battery present. USB-C data cable to the development host.
  - Cases: full flash erase, flash with verified hashes, boot from the new partition table, app version and ELF SHA-256 in the log, memory report.
  - Memory at boot: internal heap free 464936 B, minimum 464936 B, largest block 434176 B. Minimum free stack: `main` 2304 B, `IDLE` 1364 B, `esp_timer` 3868 B.
  - Not run: display, touch, KEY, battery, audio and network; they are not part of this stage.

### Stage 1 — 2026-11-01 (checkpoint, pending validation)

- **Revision:** `448b534`, integrated profile (`sdkconfig.defaults`).
- **ELF SHA-256:** `4c8c3309522b86e0ef9ef030b8f72de427a6885e7f81f41cf49a8b2f3b8a1981`. Application image 566304 bytes, 86% of the 4 MB app partition free.
- **build_verified:** yes. Local build with the documented command; CI run on the same revision passed.
- **hardware_verified:** no for this image yet.
  - Unit `esp32-02`, USB-C data cable. Flash completed with verified hashes.
  - The boot log could not be read afterwards: the unit enumerates on USB but does not answer serial or esptool. Needs a physical check before the smoke test.
  - Not run: the whole smoke test (image, colors, touch, wake gesture, KEY, battery, navigation).

