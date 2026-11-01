# Status

## Stages

| Stage | Scope | Status |
|---|---|---|
| 0 | Scaffold | Done (validated 2026-11-01) |
| 1 | Board and base UI | Done (validated 2026-11-01) |
| 2 | Storage and settings | Next |
| 3 | Wi-Fi and provisioning | Pending |
| 4 | Bridge base | Pending |
| 5a | Recording: local capture | Pending |
| 5b | Recording: upload and result | Pending |
| 6 | Notifications and sound | Pending |
| 7 | Modules: read | Pending |
| 8 | Modules: actions | Pending |
| 9 | Full acceptance | Pending |
| 10 | Battery (optional) | Pending |

## Active stage: 2 — Storage and settings

- **Goal:** reliable persistence and the complete Settings screen.
- **Progress:** spec, design and plan approved. Steps 1–4 committed and reviewed (setting values and row state, NVS storage with a write worker, Settings view and preview, Settings control). Board validation pending: the unit was not connected.

## Closed stages

- **0 — Scaffold:** repository conventions, ESP-IDF project, partition table, memory diagnostics, host tests and CI. Validated on `esp32-02` at `3cad47c`.
- **1 — Board and base UI:** I2C bus, PMIC and battery, KEY, panel and touch drivers; a UI task that owns LVGL and sleeps when idle; theme, home, section screens and navigation; screen off by timeout and KEY with a wake-gesture gate; PNG preview. Validated on `esp32-02` at `448b534`.

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

### Stage 1 — 2026-11-01

- **Revision:** `448b534`, integrated profile (`sdkconfig.defaults`).
- **ELF SHA-256:** `4c8c3309522b86e0ef9ef030b8f72de427a6885e7f81f41cf49a8b2f3b8a1981`. Application image 566304 bytes, 86% of the 4 MB app partition free.
- **build_verified:** yes. Local build with the documented command; CI run on the same revision passed.
- **hardware_verified:** yes.
  - Unit `esp32-02`, battery present, USB-C data cable. Boot log shows app version `448b534` and the ELF SHA-256 above.
  - Smoke test by the user: home and status bar, colors and edges, touch and navigation, panel off after 30 s, wake touch without clicks, KEY short and held, battery percentage. All passed.
  - Observation: screens that change almost entirely are drawn top to bottom in strips (partial buffers, no full framebuffer). Accepted for now; to revisit.
  - Memory: internal heap free 357848 B, largest block 335872 B after the UI starts. Minimum free stack: `ui` 1976–2100 B of 6144, `battery` 1396 B, `IDLE` 1300 B. LVGL pool peak 9.2 KB of 64 KB without navigation.
  - Idle: 4–5 UI task wakeups per minute with the panel off.
  - During development the unit once stopped answering on USB after a flash until it was reconnected.
