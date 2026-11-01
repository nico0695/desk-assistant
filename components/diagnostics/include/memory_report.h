#pragma once

/* Logs heap and per-task stack usage under the given checkpoint label.
 * Does nothing when CONFIG_DESK_MEMORY_DIAGNOSTICS is disabled. */
void memory_report_log(const char *checkpoint);
