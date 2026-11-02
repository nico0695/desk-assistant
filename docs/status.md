# Status

## Stages

| Stage | Scope | Status |
|---|---|---|
| 0 | Scaffold | Done (validated 2026-11-01) |
| 1 | Board and base UI | Done (validated 2026-11-01) |
| 2 | Storage and settings | Done (validated 2026-11-01) |
| 3 | Wi-Fi and provisioning | Done (validated 2026-11-01) |
| 4 | Bridge base | Done (validated 2026-11-01) |
| 5a | Recording: local capture | Done (validated 2026-11-02) |
| 5b | Recording: upload and result | Done (validated 2026-11-02) |
| 6 | Notifications and sound | Done (validated 2026-11-02) |
| 7 | Modules: read | Next |
| 8 | Modules: actions | Pending |
| 9 | Full acceptance | Pending |
| 10 | Battery (optional) | Pending |

## Active stage: 7 — Modules: read

- **Status:** not started; the stage document and its open definitions come next.
- **Goal:** Tasks, Notes and Alerts browsable with recoverable memory.
- **Scope (macro plan):** paged lists, Refresh, a valid empty page, detail by ID, Back with the list kept once; generation tickets for stale results; dates in UTC-3, read errors distinct from empty, busy during capture and upload; memory released on exit.
- **Open:** body budget (4096 B provisional) to measure.
- **Carry-overs from stage 6:** Snooze from the Alerts detail belongs to stage 8; the tone-volume intensity and the stage 6 gaps are in `.local/follow-up.md`.

## Closed stages

- **0 — Scaffold:** repository conventions, ESP-IDF project, partition table, memory diagnostics, host tests and CI. Validated on `esp32-02` at `3cad47c`.
- **1 — Board and base UI:** I2C bus, PMIC and battery, KEY, panel and touch drivers; a UI task that owns LVGL and sleeps when idle; theme, home, section screens and navigation; screen off by timeout and KEY with a wake-gesture gate; PNG preview. Validated on `esp32-02` at `448b534`.
- **2 — Storage and settings:** NVS storage that never erases, with one write worker; settings for screen timeout, volume and brightness with defaults and per-row errors; Settings screen with live brightness and timeout. Validated on `esp32-02` at `aebffa7`.
- **3 — Wi-Fi and provisioning:** saved networks (5, most recent first), station selection with the V4 timings, USB provisioning with the host tool, phone setup over a temporary access point with a QR and a small form page, Wi-Fi screen with forget and add dialogs, and the Settings Wi-Fi button. Validated on `esp32-02` at `01ff8b4`; cases deferred by the user are listed in the stage 3 evidence.
- **4 — Bridge base:** HTTPS client with validated CA/hostname/validity and Bearer only in the header, a clock gate before TLS, the authenticated SSE stream with heartbeat and dead-channel detection plus 5/10/30 s backoff, the status-bar icon driven by IP + SSE, bounded logs and diagnostics for memory and one finite read. Validated on `esp32-02` at `1eb669c`; IP-without-SSE and reconnection were deferred by the user.
- **5a — Recording: local capture:** ES7210 microphone over I2S (stereo reads keeping MIC1) with the ALDO1 rail saved and restored; PCM16 clip in the audio partition with the WAV header synthesized on read, erased after each attempt and scanned at boot; capture state machine with 0.5–30 s limits, overrun and cleanup failures; recording panel from Home with the screen held on during capture. Validated on `esp32-02` at `0eccb5e`; no 4R review or smoke guide at close by user decision.
- **5b — Recording: upload and result:** one TLS transport shared by GET, PUT and SSE; assistant codec; the single PUT with the `unsent` boundary and progress windows; a durable attempt record with an origin hold; one-job-at-a-time attempt phases; upload, query, close and discard jobs, the 180 s wait and restore after reboot; reply panel with scroll and notes; Home entry by state. Validated by the user on `esp32-02` at `6c30f19` (green CI on `0474820`); network cut, discard and Check on the board were skipped by the user and are in `.local/follow-up.md`.
- **6 — Notifications and sound:** a durable FIFO of eight and a pending-ACK journal in one NVS blob; `limit=1` drain on demand with persistence before the ACK and an idempotent ACK retry; an overlay card with local Next and Close, Snooze with presets and a durable close, panel wake-up and deferral during capture; an atomic audio arbiter between capture and playback and an ES8311 tone with a RAM accumulator. Validated by the user on `esp32-02` at `25af727` (green CI on `2396ba9`); the tone intensity does not follow the volume (only 0 mutes), and several cases are deferred in `.local/follow-up.md`.

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
- **CI:** the close push (`ee918f0`) failed host tests on Linux: `bridge_clock.c` used `NULL` without `<stddef.h>`, which macOS headers hid. Fixed in `7a3bafe` (include only, no functional change; not reflashed); CI on `7a3bafe` passed: host tests, build, log protection, lock unchanged.
- **hardware_verified:** yes, on unit `esp32-02` (ESP32-C6 QFN40 v0.2, base MAC `d4:05:92:b6:ed:28`), battery present, USB-C data cable on `/dev/cu.usbmodem101`; flashed with verified hashes.
  - Final image `1eb669c`: boot log shows the version and ELF SHA-256 above; Wi-Fi connected; SNTP synchronized; `Certificate validated` and `event stream open` about 10.8 s after boot (roughly 1.3 s of handshake after the clock was usable); the stream stayed open more than 25 s; the user confirmed the status-bar icon looks normal with IP + SSE; no host, URL, token or authorization appeared in the log.
  - Probe image: SSE open at 14.2 s; `GET /v1/device/events?limit=1` returned `status=200 bytes=13`; `bridge_probe` stack minimum 4716 of 8192 B; heap 149348 B free at probe start and 119396 B minimum with TLS + probe. After the probe, two periodic panel-off reports showed the probe task gone and heap stable at 162564 B free and 143360 B largest block; LVGL pool used 8288, peak 9320, 1% fragmentation; `ui` stack minimum 4020 B; `bridge_sse` stack minimum 2224 of 8192 B.
  - Not run, deferred by the user: IP without SSE and reconnection/backoff on the device; setup open together with TLS. Recorded in `.local/follow-up.md`.
- **4R review at close:** four lenses, no corroboration pass was needed; 0 blocking, 6 warnings and 4 suggestions. The warnings: a stalled close-delimited GET can be reported as OK with a truncated body; TLS/Bearer transport duplicated between `bridge_http` and `bridge_stream`; `bridge_http_get` grew past one responsibility; the SSE clock gate has no host test; SSE `Content-Encoding` and duplicate `Content-Type` rejection have no host test; the path where NTP never synchronizes is silent. The suggestions: long `bridge_stream_run` and `stream_worker`; unused `BRIDGE_SSE_HEARTBEAT_EXPECTED_MS`; unexplained equal-generation tie-break in the UI status. Triage deferred to stage 5a and recorded above; details in `.local/stages/04-bridge-base.md`.

### Stage 5a — 2026-11-02

- **Revision:** `94de1ab` (firmware content from `926eb92`; the later commit only changes `docs/status.md`), integrated profile (`sdkconfig.defaults`).
- **ELF SHA-256:** `679331dd157b4eb92d75e63786b6413e87bd1f0c198475c3326b34452175f4de`. Application image 1599200 bytes, 62% of the 4 MB app partition free.
- **build_verified:** yes. Documented integrated build from the committed revision without warnings; host tests 28/28; preview built; `dependencies.lock` unchanged.
- **hardware_verified:** partial. This exact image was not flashed. Observed on unit `esp32-02` (ESP32-C6 QFN40 v0.2, base MAC `d4:05:92:b6:ed:28`, now on `/dev/cu.usbmodem1101`), flashed with verified hashes:
  - Image `bb03eb6-dirty` (same firmware sources as `926eb92` minus the cleanup fix): the user ran record and send, silence, short capture, the 30 s limit, cancel while recording and KEY during capture; all looked as expected. Overruns 0; slowest flash write 2 ms; the 30 s limit arrived after 30.1 s of wall time; voice `mean_abs` 95 against 22 in silence, so the left slot is MIC1. Clip erase 388 ms at boot, 81–469 ms after captures up to 10 s, 1358 ms after 30 s. ALDO1 was on at 3.3 V before each capture and was restored the same way. Heap 174792 B free with the UI ready, minimum 144200 B with captures and SSE open; recording worker minimum 1648 of 4096 B free. No audio content in the log.
  - Image `bb03eb6-dirty` with the cleanup fix (`926eb92` sources): boot cleanup `ESP_OK` in 388 ms, no errors.
  - An earlier image with I2S mono RX recorded both slots at twice the rate; it was replaced by stereo reads keeping the left slot (`504549f`).
  - Captures shorter than 500 ms could not be produced by hand (shortest 560 ms); the rule is covered by host tests.
- **Reviews:** `standard-code-review` on steps 6–7: CR-001 (cleanup did not retry a failed hardware release) fixed in `926eb92`; CR-002 (a view computed under the lock and published outside it can, in theory, arrive after a newer one) recorded without change. By user decision, no 4R review at close and no smoke guide.

### Stage 5b — 2026-11-02

- **Revision:** `6c30f19` (firmware content; later commits only change documentation), integrated profile (`sdkconfig.defaults`). CI on the close push `0474820` passed (host tests, build, lock unchanged).
- **ELF SHA-256:** `8e37cd57c518c10f1499df1e0a080e96a686503cae62ecdead74527178187d2f` (the boot log prints its first 9 digits).
- **build_verified:** yes. Documented integrated build from the committed revision without warnings; host tests 30/30; preview built; `dependencies.lock` unchanged.
- **hardware_verified:** partial, on unit `esp32-02` (ESP32-C6 QFN40 v0.2, base MAC `d4:05:92:b6:ed:28`), battery present, USB-C data cable on `/dev/cu.usbmodem1101`; flashed with verified hashes.
  - Final image `6c30f19`: boot log shows the version and ELF digits above; an unclosed completed attempt was restored without its text, queried once the event stream opened (13 s after boot) and shown; no errors.
  - Image `0891c45-dirty` (sources of `0891c45` plus the memory checkpoints, old TCP buffer): event stream open (the deferred step 1 check); the user ran a real message with its reply, a long reply with scroll and a 30 s upload. Four attempts went 202 processing → query → completed → stored → shown → closed (clip cleared, record removed). Uploads of 178–960 KB at 10–12.5 KB/s (960 KB in 76.6 s); captures without overruns.
  - Image `0891c45-dirty` with the TCP send buffer at 23040 B (sources of `7d3305e`): 30 s upload in 28.4 s (about 34 KB/s); reboot while processing, done automatically right after the 202: the attempt was restored as pending, the clip cleared, the query after the stream opened returned completed, which was stored and shown.
  - Memory: heap minimum 63.6 KB with SSE and the upload together (83.5 KB with the old buffer); back to 151–159 KB free after closing. Recording worker minimum 4.2 KB free of 8192 B (fixed at 8192); `bridge_sse` minimum 2.8 KB of 8192 B. No audio, reply text, token, URL or request ID in the log.
  - Not run, by user decision: network cut during the upload, discard and Check on an unconfirmed attempt. Not run: maximum-size reply. Recorded in `.local/follow-up.md`.
  - Tooling: after a reset the USB log sometimes stops without an error; capturing without a reset or reconnecting the cable recovers it.
- **Validation:** the user ran the smoke guide on the final image and reported everything working (2026-11-02).
- **Reviews:** `standard-code-review` per step group (1, 3–4, 5–7), findings fixed in `0c8e030` and `0891c45`. 4R at close: four lenses, 0 severe, 8 warnings and 3 suggestions. Fixed in `6c30f19`: a discard queued during a query could erase a terminal that just arrived; an unreadable record lost its block if the boot cleanup worker failed to start; a held origin refused an identical save, which blocked Wi-Fi provisioning; an ambiguous upload armed no 180 s wait; a missing log on an allocation failure; misleading names and header comments. Details in `.local/stages/05b-grabacion-envio-resultado.md`.

### Stage 6 — 2026-11-02

- **Revision:** `25af727` (firmware content; later commits only change documentation), integrated profile (`sdkconfig.defaults`).
- **ELF SHA-256:** `39993ada406151b1ecfc37e3e92f12a2f3635c47887fa2e03d1ecbdadc482d16` (the boot log prints its first 9 digits). Application image 1639936 bytes, 61% of the 4 MB app partition free.
- **build_verified:** yes. Documented integrated build from the committed revision without warnings; host tests 37/37; `dependencies.lock` unchanged. CI on the close push `2396ba9` passed (build job).
- **hardware_verified:** partial, on unit `esp32-02` (ESP32-C6 QFN40 v0.2, base MAC `d4:05:92:b6:ed:28`), battery present, USB-C data cable on `/dev/cu.usbmodem101`; flashed with verified hashes.
  - Final image `25af727`: boot log shows the version and ELF digits above; clip cleanup `ESP_OK`; event stream open 12.5 s after boot; heap 167 KB free, minimum 154 KB at UI ready; no errors. The final fixes (worker exit races, tone volume read after the claim, shared I2S pins) were not exercised with real notifications on this image.
  - Image `f5b30eb` (the audio step, same notification and audio logic before the fixes), observed by the user: notifications arriving and shown, Next and Close, Snooze from the card, an alert arriving during a recording without interrupting it, then shown on Send and the upload flow continuing; the tone audible at 36% with the ES8311 answering at 0x18, silent at volume 0; a reset with an open alert restored it once, without a tone, and it closed normally. A real upload with SSE open reached a heap minimum of 57.8 KB.
  - Known gap: the tone intensity does not change between 5% and 36% (only 0 mutes); deferred in `.local/follow-up.md`.
  - Not run: full queue (8), an ACK interrupted by a network cut, a capture started during a tone, tone at 100%, volume 25%. Covered by host tests except the audio cases.
- **Reviews:** `standard-code-review` after steps 1–6 (findings fixed in the step substeps). 4R at close: four lenses, 0 severe, 17 warnings and suggestions. Fixed in `25af727`: a command sent while the notification worker exits could stay queued, and both workers could be notified after the lock was released; the tone worker polled NVS while waiting for a capture; the I2S pins were duplicated; the time zone label was a literal. Deferred: unbounded tone cleanup retry, an ACK answered with a permanent error retrying forever, a journal save failing on memory blocking notifications until reboot, single-slot queue in the notification tests, and readability suggestions. Details in `.local/follow-up.md`.
- **Validation:** the user ran the smoke guide on the final image and reported everything working (2026-11-02).
