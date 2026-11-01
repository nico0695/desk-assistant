# Agent instructions

Desk Assistant is ESP32-C6 firmware for a voice and notification terminal that works with an HTTPS bridge. These rules apply to every change in this repository.

## Sources

- The product specification lives in `.local/requeriments-idea/` (local, not versioned). Read its README and the documents for the affected area before changing firmware. Each rule has one responsible document.
- The stage plan is `.local/plan/01-macro-plan.md`; each stage has its own document in `.local/stages/`.
- If `.local/` is missing, say so instead of assuming its content.
- When a decision changes a product rule, update the specification in the same task.

## Toolchain

- ESP-IDF 5.5.2, target `esp32c6`, LVGL 9.2.2. Versions are pinned in the manifests and `dependencies.lock`; never update them incidentally.
- Build: `idf.py -B build-app -D SDKCONFIG="$PWD/build-app/sdkconfig" -D SDKCONFIG_DEFAULTS="sdkconfig.defaults" build`
- Host tests: `tools/test.sh`
- There is one integrated build profile. Add another only for a concrete need, with its own build directory.

## Architecture

- `main/` only boots and composes. Features live in `components/features/<name>/`; `ui`, `bridge`, `storage`, `connectivity` and `board` are one component each.
- Each component declares the minimum `REQUIRES`. Public headers go in `include/`, private code in `src/`. No dependency cycles and no global header.
- Create a component or folder only when it has code. A component does not imply a task; no resident task per feature.
- State transitions do not depend on LVGL, NVS, HTTP or task creation, and are tested on the host.
- UI callbacks dispatch commands and never wait on network, scans, audio or persistence. Only the UI owner touches LVGL objects.
- Every buffer and string has a defined owner and lifetime. Results that can go stale carry a generation ticket and identity.
- Finite HTTP requests are serialized; the SSE stream is independent.
- Persist before the effect that depends on durability. Never erase storage automatically on errors, corruption or full space.

## Code standards

- Declarative names; functions with one concrete responsibility; explicit states, results and errors.
- No hardcoded values. Contract values (bridge limits and times, setting ranges) are named constants in the owning component's public header. Build-tunable values (stacks, pools, log levels, diagnostics) go in that component's `Kconfig`. UI text is centralized per feature.
- Strict validation at external boundaries; bounded buffers; cleanup on every exit path.
- No duplicated code: reuse, or extract only when the pieces share semantics.
- Comments only when the code cannot say it, and brief.
- Everything in the repository is in English.
- Before each task, check the best approach, side effects, reusable code and small refactors that improve what exists.
- No over-engineering: no layers, abstractions or cases the specification or a real defect does not require.

## Contract rules

- A notification is persisted before its `displayed` ACK. Notification reads use `limit=1`. Close and Next are local.
- No `has_more`, ACK on dismiss, replay, fallback polling or inferred retries of mutations.
- One PUT per recording attempt. Results are recovered only by GET; an incomplete upload is classified as `unsent`.

## Stage workflow

1. Stage spec with its open definitions closed.
2. Validated design: the simplest solution that meets the spec.
3. Mini plan with small, verifiable steps.
4. Each step has its tests and one commit, followed by QA review. Valid findings go to a fix substep that reruns the affected tests.
5. Checkpoint: stage review and fixes, `docs/status.md`, smoke test guide, then stop and wait for user validation.

QA review:

- After each step: `standard-code-review` on the step's commits.
- Steps over 600 changed lines or 15 files, and every stage close: `4r-review`.
- Reviews are read-only. Besides defects, they check fit with the plan and spec, contracts, architecture, standards, duplication, side effects, memory and concurrency, and test value.
- Before applying a step or finding, confirm it is correct, required by the spec or a real defect, and not adding needless complexity. Discard the rest with the reason noted.

## Git

- One-line Conventional Commits with the component as scope, for example `feat(wifi): add network selection`. No author or co-author trailers.
- Work directly on `main`. Commit and push freely during a stage; after pushing, read the remote SHA and CI result.

## Documentation

- Simple and direct. No code references unless needed.
- `docs/status.md` is the only tracking document: stage list, active stage detail and evidence. `build_verified` and `hardware_verified` are recorded separately and never inherited from another image.

## Security

- No passwords, tokens, URLs with credentials or private configuration in Git, documentation, arguments, logs or published artifacts. Secrets are entered through provisioning.
- Never log audio or user content. Technical logs stay bounded.
- NVS is not encrypted: integrity checks are not confidentiality.

## Physical operations

- Flashing the app, resetting and reading the serial log are authorized on test unit `esp32-02`. Verify the port and chip before each operation.
- Flash erases, partition table changes and tool installation require explicit confirmation.
- Never modify or deploy the bridge or the Slack bot from this repository's tasks.

## Third-party code

Keep the license, authorship and source revision of every reused piece of code or asset, and list them in `THIRD_PARTY_NOTICES.md`.
