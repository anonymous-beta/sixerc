
#pragma once

#include "core/types.hpp"

#include <string>

namespace sixerc::session {

SessionInspection inspect_jwt(
    const std::string& token
);

std::string decode_base64url(
    const std::string& input
);

} // namespace sixerc::session
