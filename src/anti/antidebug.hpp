// ============================================================================
// SIXERC — Anti-Debug Module
// Detects and evades debugging/analysis environments
// ============================================================================

#pragma once

#include <windows.h>
#include <string>

namespace sixerc::anti {

// Check if a debugger is attached
bool is_debugger_present();
bool check_remote_debugger();
bool check_nt_global_flag();
bool check_heap_flags();
bool check_debug_registers();

// Timing-based detection
bool timing_check();

// Process/artifact checks
bool check_sandbox_processes();
bool check_vm_artifacts();

// Combined check
bool is_being_analyzed();

// Evasive action
void evade_if_detected();

} // namespace sixerc::anti
