# Status

## Stages

| Stage | Scope | Status |
|---|---|---|
| 0 | Scaffold | Done (validated 2026-11-01) |
| 1 | Board and base UI | Done (validated 2026-11-01) |
| 2 | Storage and settings | Done (validated 2026-11-01) |
| 3 | Wi-Fi and provisioning | Done (validated 2026-11-01) |
| 4 | Bridge base | Done (validated 2026-11-01) |
| 5a | Recording: local capture | Next |
| 5b | Recording: upload and result | Pending |
| 6 | Notifications and sound | Pending |
| 7 | Modules: read | Pending |
| 8 | Modules: actions | Pending |
| 9 | Full acceptance | Pending |
| 10 | Battery (optional) | Pending |

## Active stage: 5a — Recording: local capture

- **Status:** planning not started; no stage document or approved mini plan yet.
- **Goal:** reliable audio capture without depending on the bridge.
- **Scope (macro plan):** ES7210/I2S, mono PCM16 in the audio partition and WAV synthesized on read; minimum and maximum limits with overrun handling; states through READY_TO_SEND, CANCELLING and local failure; cleanup and blocking on release failures; panel on during capture; recording screen and home entry.
- **Open:** audio arbitration detail.
- **Carry-overs from stage 4:** connect the durable origin guard to recording and notification records in 5b/6. Triage these deferred 4R findings at the next stage and fix only those that are real defects: stalled close-delimited GET reported as OK with a truncated body; TLS/Bearer transport duplicated between `bridge_http` and `bridge_stream`; long `bridge_http_get`; long `bridge_stream_run` and `stream_worker`; unused `BRIDGE_SSE_HEARTBEAT_EXPECTED_MS`; missing host test for the SSE clock gate; missing host test for SSE `Content-Encoding` and duplicate `Content-Type`; unexplained equal-generation tie-break; silent path when NTP never synchronizes. Also in `.local/follow-up.md`: on-device IP-without-SSE and reconnection, and setup open with TLS.
- **Workflow:** commits, tests, builds and `standard-code-review` stay local per step; push and remote verification happen at stage close. Deeper step reviews need a reason based on size, complexity or risk; stage close uses `4r-review` by default or `judgment-day` when blind corroboration is more useful.

## Closed stages

- **0 — Scaffold:** repository conventions, ESP-IDF project, partition table, memory diagnostics, host tests and CI. Validated on `esp32-02` at `3cad47c`.
- **1 — Board and base UI:** I2C bus, PMIC and battery, KEY, panel and touch drivers; a UI task that owns LVGL and sleeps when idle; theme, home, section screens and navigation; screen off by timeout and KEY with a wake-gesture gate; PNG preview. Validated on `esp32-02` at `448b534`.
- **2 — Storage and settings:** NVS storage that never erases, with one write worker; settings for screen timeout, volume and brightness with defaults and per-row errors; Settings screen with live brightness and timeout. Validated on `esp32-02` at `aebffa7`.
- **3 — Wi-Fi and provisioning:** saved networks (5, most recent first), station selection with the V4 timings, USB provisioning with the host tool, phone setup over a temporary access point with a QR and a small form page, Wi-Fi screen with forget and add dialogs, and the Settings Wi-Fi button. Validated on `esp32-02` at `01ff8b4`; cases deferred by the user are listed in the stage 3 evidence.
- **4 — Bridge base:** HTTPS client with validated CA/hostname/validity and Bearer only in the header, a clock gate before TLS, the authenticated SSE stream with heartbeat and dead-channel detection plus 5/10/30 s backoff, the status-bar icon driven by IP + SSE, bounded logs and diagnostics for memory and one finite read. Validated on `esp32-02` at `1eb669c`; IP-without-SSE and reconnection were deferred by the user.

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

### Stage 2 — 2026-11-01

- **Revision:** `aebffa7`, integrated profile (`sdkconfig.defaults`).
- **ELF SHA-256:** `f107a8ca63602839f8b48fd1a003ba27f3d450af1467390c6f891831882b425b`. Application image 594352 bytes, 86% of the 4 MB app partition free.
- **build_verified:** yes. Local build from the committed revision with the documented command; host tests 8/8; `dependencies.lock` unchanged.
- **hardware_verified:** yes.
  - Unit `esp32-02` (ESP32-C6 QFN40 v0.2), battery present, USB-C data cable on `/dev/cu.usbmodem101`. Flashed with verified hashes; boot log shows app version `aebffa7` and the ELF SHA-256 above.
  - First boot on empty NVS: defaults without load errors.
  - Smoke test by the user: Settings layout, brightness drag (including 10%), volume 0 and 100, each screen timeout and always-on, panel off with KEY in the middle of a brightness drag, touches during a save. All passed.
  - Persistence: after a reset the user saw the values set before it. NVS then held 4 entries (3 keys and the namespace) of 2016.
  - NVS commits: 21 writes, 0.6–12.8 ms each, with no visible effect on touch or drawing.
  - Memory: internal heap free 344976 B, largest block 327680 B at boot. Minimum free stack after the smoke test: `ui` 1576 B of 6144, `storage` 1376 B of 3072, `battery` 1392 B. LVGL pool peak 15.5 KB of 64 KB.
  - Not run: a failed save or load on the device (covered by host tests and the preview); volume has no audible effect until stage 6.

### Stage 3 — 2026-11-01

- **Revision:** `01ff8b4`, integrated profile (`sdkconfig.defaults`).
- **ELF SHA-256:** `9e475848807c26665ead68ee434a52f2565a50b87e333399035b578b77f9bd12` (the boot log prints its first 9 digits). Application image 1348048 bytes, 68% of the 4 MB app partition free.
- **build_verified:** yes. Clean build from the committed revision with the documented command; host tests 16/16; `dependencies.lock` unchanged. CI run on the same revision (`01ff8b4`) passed: host tests, build, lock unchanged.
- **hardware_verified:** partial.
  - Validation by the user (2026-11-01): connected the unit to a real Wi-Fi network and saw "Connected" in Settings → Wi-Fi; the status bar icon stays crossed out by design until stage 4. The user accepted the other smoke cases as correct without a separate run, except those deferred below.
  - Deferred by the user, not run: two real networks, hidden network, temporary router outage.
  - Unit `esp32-02` (ESP32-C6 QFN40 v0.2, base MAC `d4:05:92:b6:ed:28`), battery present, USB-C data cable on `/dev/cu.usbmodem101`. Flashed with verified hashes; boot log shows app version `01ff8b4` and the ELF digits above.
  - Observed on this image: boot with one saved test network (fake credentials): scan hears 11–18 networks, tries network 0, disconnects with reason 201 (AP not found), new round every ~30 s; no SSID, password or URL in the log.
  - Memory at boot with Wi-Fi started: internal heap free 184936 B, minimum 184524 B, largest block 167936 B. Minimum free stack: `ui` 4144 of 8192 B at the Settings screen, `sys_evt` 4004 B at boot (2452 B of 4096 after several rounds on an earlier image), `usb_provision` 3108 of 3584 B, `storage` 2756 B.
  - Verified on earlier images only, not repeated on this one: USB provisioning (`configure_bridge` and `configure` answer `ok`, invalid URL, malformed JSON, a 3100-byte line and an idle partial line are rejected, values survive a reset, nothing from the request in the log) at `122fc98`; the setup access point and the HTTP server starting without errors, with about 20 KB less heap (minimum 165 KB) and `httpd` at 3476 of 4096 B before any request, with a temporary build that opened the setup at boot (`76903d3`, not kept).
  - Accepted by the user without a separate run, so not evidenced here: wrong password in the setup, forget, last network, cancel and back, persistence after a reset, boot without saved networks, the live screen timeout change after `configure`, and the sixth network replacing the oldest. Follow-up items are in `.local/follow-up.md`.

### Stage 4 — 2026-11-01

- **Revision:** `1eb669c` (firmware content from `cb87b04`; later commits only change `docs/status.md`), integrated profile (`sdkconfig.defaults`).
- **ELF SHA-256:** `58edcd7a92a341432dc877823f090b4d1f2638b55aae9432bb2da2cdde89d9a4` (the boot log prints its first 9 digits). Application image 1569696 bytes, 63% of the 4 MB app partition free.
- **build_verified:** yes. Documented integrated build from the committed revision without warnings; host tests 26/26; `dependencies.lock` unchanged. A separate image with `CONFIG_BRIDGE_DIAGNOSTIC_PROBE=y` was built and flashed for step 5 (ELF `1fc4076441ec47d66ec4520a862d9c892e9ec7d65633c35b38b5289571ac8588`); it is not the final image.
- **hardware_verified:** yes, on unit `esp32-02` (ESP32-C6 QFN40 v0.2, base MAC `d4:05:92:b6:ed:28`), battery present, USB-C data cable on `/dev/cu.usbmodem101`; flashed with verified hashes.
  - Final image `1eb669c`: boot log shows the version and ELF SHA-256 above; Wi-Fi connected; SNTP synchronized; `Certificate validated` and `event stream open` about 10.8 s after boot (roughly 1.3 s of handshake after the clock was usable); the stream stayed open more than 25 s; the user confirmed the status-bar icon looks normal with IP + SSE; no host, URL, token or authorization appeared in the log.
  - Probe image: SSE open at 14.2 s; `GET /v1/device/events?limit=1` returned `status=200 bytes=13`; `bridge_probe` stack minimum 4716 of 8192 B; heap 149348 B free at probe start and 119396 B minimum with TLS + probe. After the probe, two periodic panel-off reports showed the probe task gone and heap stable at 162564 B free and 143360 B largest block; LVGL pool used 8288, peak 9320, 1% fragmentation; `ui` stack minimum 4020 B; `bridge_sse` stack minimum 2224 of 8192 B.
  - Not run, deferred by the user: IP without SSE and reconnection/backoff on the device; setup open together with TLS. Recorded in `.local/follow-up.md`.
- **4R review at close:** four lenses, no corroboration pass was needed; 0 blocking, 6 warnings and 4 suggestions. The warnings: a stalled close-delimited GET can be reported as OK with a truncated body; TLS/Bearer transport duplicated between `bridge_http` and `bridge_stream`; `bridge_http_get` grew past one responsibility; the SSE clock gate has no host test; SSE `Content-Encoding` and duplicate `Content-Type` rejection have no host test; the path where NTP never synchronizes is silent. The suggestions: long `bridge_stream_run` and `stream_worker`; unused `BRIDGE_SSE_HEARTBEAT_EXPECTED_MS`; unexplained equal-generation tie-break in the UI status. Triage deferred to stage 5a and recorded above; details in `.local/stages/04-bridge-base.md`.
