#pragma once

#include "PCH.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <unordered_map>

// Tiny INI reader: "key = value" lines, ';' or '#' comments, [sections] ignored (keys are global).
struct Settings
{
    // "swf"  = load the vanilla-style quest list SWF (default)
    // "text" = fallback: plain text field, no SWF needed
    std::string mode{ "swf" };

    // ---- swf mode ----
    std::string swfPath{ "QHT_QuestItemList.swf" };  // relative to Data/Interface
    // The SWF multiplies these by the stage size, so they are ratios from 0 to 1.
    float swfX{ 0.03f };
    float swfY{ 0.25f };
    float swfMaxHeight{ 0.5f };
    // Registers the list with the HUD's element list so the HUD hides it in the same
    // situations as other HUD parts (dialogue, swimming, horse, ...).
    bool registerHudElement{ true };

    // ---- text mode ----
    // Position and size are in HUD stage units (the HUD stage is 1280x720).
    float x{ 40.0f };
    float y{ 190.0f };
    float width{ 360.0f };
    float height{ 220.0f };
    float titleSize{ 24.0f };
    float objectiveSize{ 22.0f };
    float opacity{ 100.0f };  // 0-100
    std::uint32_t titleColor{ 0xDCC979 };
    std::uint32_t objectiveColor{ 0xEBEBEB };

    // ---- both ----
    int maxObjectives{ 4 };
    int refreshMs{ 500 };
    bool enabled{ true };

    static Settings& Get()
    {
        static Settings s;
        return s;
    }

    void Load(const std::string& a_path)
    {
        std::ifstream in(a_path);
        if (!in) {
            logger::warn("Settings file not found, using defaults: {}", a_path);
            return;
        }

        std::unordered_map<std::string, std::string> kv;
        std::string line;
        while (std::getline(in, line)) {
            Trim(line);
            if (line.empty() || line[0] == ';' || line[0] == '#' || line[0] == '[') {
                continue;
            }
            const auto eq = line.find('=');
            if (eq == std::string::npos) {
                continue;
            }
            std::string key = line.substr(0, eq);
            std::string val = line.substr(eq + 1);
            Trim(key);
            Trim(val);
            std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            kv[key] = val;
        }

        GetString(kv, "mode", mode);
        std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        GetString(kv, "swfpath", swfPath);
        GetFloat(kv, "swfx", swfX);
        GetFloat(kv, "swfy", swfY);
        GetFloat(kv, "swfmaxheight", swfMaxHeight);
        GetBool(kv, "registerhudelement", registerHudElement);

        GetFloat(kv, "x", x);
        GetFloat(kv, "y", y);
        GetFloat(kv, "width", width);
        GetFloat(kv, "height", height);
        GetFloat(kv, "titlesize", titleSize);
        GetFloat(kv, "objectivesize", objectiveSize);
        GetFloat(kv, "opacity", opacity);
        GetColor(kv, "titlecolor", titleColor);
        GetColor(kv, "objectivecolor", objectiveColor);

        GetInt(kv, "maxobjectives", maxObjectives);
        GetInt(kv, "refreshms", refreshMs);
        GetBool(kv, "enabled", enabled);

        opacity = std::clamp(opacity, 0.0f, 100.0f);
        refreshMs = std::max(refreshMs, 100);
        maxObjectives = std::max(maxObjectives, 1);
    }

private:
    static void Trim(std::string& s)
    {
        const auto notSpace = [](unsigned char c) { return !std::isspace(c); };
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
        s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    }

    static void GetString(const std::unordered_map<std::string, std::string>& kv, const char* key, std::string& out)
    {
        if (auto it = kv.find(key); it != kv.end() && !it->second.empty()) {
            out = it->second;
        }
    }

    static void GetFloat(const std::unordered_map<std::string, std::string>& kv, const char* key, float& out)
    {
        if (auto it = kv.find(key); it != kv.end()) {
            out = std::strtof(it->second.c_str(), nullptr);
        }
    }

    static void GetInt(const std::unordered_map<std::string, std::string>& kv, const char* key, int& out)
    {
        if (auto it = kv.find(key); it != kv.end()) {
            out = static_cast<int>(std::strtol(it->second.c_str(), nullptr, 10));
        }
    }

    static void GetBool(const std::unordered_map<std::string, std::string>& kv, const char* key, bool& out)
    {
        if (auto it = kv.find(key); it != kv.end()) {
            out = std::strtol(it->second.c_str(), nullptr, 10) != 0;
        }
    }

    // Accepts RRGGBB hex, with or without a leading '#'.
    static void GetColor(const std::unordered_map<std::string, std::string>& kv, const char* key, std::uint32_t& out)
    {
        if (auto it = kv.find(key); it != kv.end()) {
            std::string v = it->second;
            if (!v.empty() && v[0] == '#') {
                v.erase(0, 1);
            }
            out = static_cast<std::uint32_t>(std::strtoul(v.c_str(), nullptr, 16)) & 0xFFFFFF;
        }
    }
};
