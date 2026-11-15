
#include "session_inspector.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace sixerc::session {
namespace {

constexpr char BASE64_TABLE[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789-_";

std::optional<std::string> json_string(
    const std::string& json,
    const std::string& key
) {
    const std::string needle = "\"" + key + "\"";

    std::size_t position = json.find(needle);

    if (position == std::string::npos) {
        return std::nullopt;
    }

    position = json.find(':', position + needle.size());

    if (position == std::string::npos) {
        return std::nullopt;
    }

    ++position;

    while (position < json.size() &&
           std::isspace(
               static_cast<unsigned char>(json[position]))) {
        ++position;
    }

    if (position >= json.size() || json[position] != '"') {
        return std::nullopt;
    }

    ++position;

    std::string result;
    bool escaped = false;

    while (position < json.size()) {
        const char current = json[position++];

        if (escaped) {
            switch (current) {
                case '"': result.push_back('"'); break;
                case '\\': result.push_back('\\'); break;
                case '/': result.push_back('/'); break;
                case 'b': result.push_back('\b'); break;
                case 'f': result.push_back('\f'); break;
                case 'n': result.push_back('\n'); break;
                case 'r': result.push_back('\r'); break;
                case 't': result.push_back('\t'); break;
                default:
                    return std::nullopt;
            }

            escaped = false;
            continue;
        }

        if (current == '\\') {
            escaped = true;
            continue;
        }

        if (current == '"') {
            return result;
        }

        if (static_cast<unsigned char>(current) < 0x20) {
            return std::nullopt;
        }

        result.push_back(current);
    }

    return std::nullopt;
}

std::optional<std::int64_t> json_integer(
    const std::string& json,
    const std::string& key
) {
    const std::string needle = "\"" + key + "\"";

    std::size_t position = json.find(needle);

    if (position == std::string::npos) {
        return std::nullopt;
    }

    position = json.find(':', position + needle.size());

    if (position == std::string::npos) {
        return std::nullopt;
    }

    ++position;

    while (position < json.size() &&
           std::isspace(
               static_cast<unsigned char>(json[position]))) {
        ++position;
    }

    const std::size_t start = position;

    if (position < json.size() && json[position] == '-') {
        ++position;
    }

    while (position < json.size() &&
           std::isdigit(
               static_cast<unsigned char>(json[position]))) {
        ++position;
    }

    if (position == start ||
        (position == start + 1 && json[start] == '-')) {
        return std::nullopt;
    }

    try {
        return std::stoll(json.substr(start, position - start));
    } catch (...) {
        return std::nullopt;
    }
}

bool valid_json_object_shape(const std::string& value) {
    if (value.size() < 2 ||
        value.front() != '{' ||
        value.back() != '}') {
        return false;
    }

    bool quoted = false;
    bool escaped = false;
    int depth = 0;

    for (const char c : value) {
        if (escaped) {
            escaped = false;
            continue;
        }

        if (quoted && c == '\\') {
            escaped = true;
            continue;
        }

        if (c == '"') {
            quoted = !quoted;
            continue;
        }

        if (quoted) {
            continue;
        }

        if (c == '{') {
            ++depth;
        } else if (c == '}') {
            --depth;

            if (depth < 0) {
                return false;
            }
        }
    }

    return !quoted && depth == 0;
}

} // namespace

std::string decode_base64url(const std::string& input) {
    std::string output;
    output.reserve((input.size() * 3) / 4);

    unsigned int accumulator = 0;
    int bits = 0;

    for (const unsigned char c : input) {
        if (c == '=') {
            break;
        }

        const char* found = std::find(
            std::begin(BASE64_TABLE),
            std::end(BASE64_TABLE) - 1,
            static_cast<char>(c)
        );

        if (found == std::end(BASE64_TABLE) - 1) {
            return {};
        }

        accumulator =
            (accumulator << 6) |
            static_cast<unsigned int>(
                found - std::begin(BASE64_TABLE)
            );

        bits += 6;

        if (bits >= 8) {
            bits -= 8;

            output.push_back(
                static_cast<char>(
                    (accumulator >> bits) & 0xFF
                )
            );
        }
    }

    return output;
}

SessionInspection inspect_jwt(const std::string& token) {
    SessionInspection result;
    result.token_format = "JWT";
    result.signature_verified = false;

    if (token.empty() || token.size() > 65536) {
        result.message = "Token is empty or exceeds the inspection limit.";
        return result;
    }

    const std::size_t first_dot = token.find('.');
    const std::size_t second_dot =
        first_dot == std::string::npos
            ? std::string::npos
            : token.find('.', first_dot + 1);

    if (first_dot == std::string::npos ||
        second_dot == std::string::npos ||
        token.find('.', second_dot + 1) != std::string::npos) {
        result.message = "Input does not have the three-part JWT structure.";
        return result;
    }

    const std::string header_encoded =
        token.substr(0, first_dot);

    const std::string payload_encoded =
        token.substr(
            first_dot + 1,
            second_dot - first_dot - 1
        );

    if (header_encoded.empty() || payload_encoded.empty()) {
        result.message = "JWT header or payload is empty.";
        return result;
    }

    const std::string header =
        decode_base64url(header_encoded);

    const std::string payload =
        decode_base64url(payload_encoded);

    if (!valid_json_object_shape(header) ||
        !valid_json_object_shape(payload)) {
        result.message = "JWT header or payload could not be decoded.";
        return result;
    }

    result.structurally_valid = true;

    result.claims.issuer =
        json_string(payload, "iss").value_or("");

    result.claims.audience =
        json_string(payload, "aud").value_or("");

    result.claims.subject =
        json_string(payload, "sub").value_or("");

    result.claims.tenant_id =
        json_string(payload, "tid").value_or("");

    result.claims.object_id =
        json_string(payload, "oid").value_or("");

    result.claims.client_id =
        json_string(payload, "appid").value_or(
            json_string(payload, "azp").value_or("")
        );

    result.claims.issued_at = json_integer(payload, "iat");
    result.claims.not_before = json_integer(payload, "nbf");
    result.claims.expires_at = json_integer(payload, "exp");

    const auto now =
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();

    if (result.claims.expires_at) {
        result.claims.expired =
            now >= *result.claims.expires_at;
    }

    if (result.claims.not_before) {
        result.claims.not_yet_valid =
            now < *result.claims.not_before;
    }

    result.claims.signature_verified = false;
    result.claims_read = true;

    if (!result.claims.expires_at) {
        result.findings.push_back({
            "JWT-001",
            "Missing expiry claim",
            "The inspected token has no readable exp claim.",
            AuditSeverity::Medium,
            "Confirm the issuer's token lifetime policy."
        });
    } else if (result.claims.expired) {
        result.findings.push_back({
            "JWT-002",
            "Token is expired",
            "The exp claim indicates that the token has expired.",
            AuditSeverity::Informational,
            "Obtain a new token through the application's authorized sign-in flow."
        });
    }

    if (result.claims.not_yet_valid) {
        result.findings.push_back({
            "JWT-003",
            "Token is not yet valid",
            "The nbf claim is later than the current time.",
            AuditSeverity::Low,
            "Check system clock synchronization and token issuance."
        });
    }

    result.findings.push_back({
        "JWT-004",
        "Signature not verified",
        "This module only decodes claims. It does not validate the JWT signature.",
        AuditSeverity::Medium,
        "Validate signatures against trusted issuer metadata before relying on claims."
    });

    result.message =
        "JWT structure decoded. Claims are informational and unverified.";

    return result;
}

} // namespace sixerc::session
