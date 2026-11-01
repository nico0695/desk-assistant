# Status

## Stages

| Stage | Scope | Status |
|---|---|---|
| 0 | Scaffold | In progress |
| 1 | Board and base UI | Pending |
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

## Active stage: 0 — Scaffold

- **Goal:** repository conventions, ESP-IDF project, partition table, memory diagnostics, host tests and CI.
- **Progress:** repository conventions; ESP-IDF project, partition table and boot log; memory diagnostics.
- **Validation:** pending.
- **Next:** host test harness.

## Evidence

Each checkpoint records one entry per image:

- Date, revision, build profile and ELF SHA-256 of the application (printed at boot).
- `build_verified`: yes or no, with the command used.
- `hardware_verified`: yes, no or partial, with unit, power, port, executed cases, observations, failures and cases not run.
