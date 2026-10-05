#include "Tracker.h"

#include "PCH.h"
#include "Settings.h"

#include <cstdio>
#include <optional>
#include <vector>

namespace Tracker
{
    namespace
    {
        // ------------------------------------------------------------------ quest data

        struct QuestData
        {
            std::string title;
            std::string type;  // frame label understood by QuestItemList.swf
            std::vector<std::string> objectives;

            bool operator==(const QuestData&) const = default;
        };

        // Maps the quest's category to the frame labels used by QuestItemList.swf.
        const char* SwfTypeFor(int a_questType)
        {
            switch (a_questType) {
            case 1: return "Main";
            case 2: return "MagesGuild";
            case 3: return "ThievesGuild";
            case 4: return "DarkBrotherhood";
            case 5: return "Companion";
            case 6: return "Misc";
            case 7: return "Daedric";
            case 8: return "Favor";
            case 9: return "CivilWar";
            case 10: return "DLC01";
            case 11: return "DLC02";
            default: return "Misc";
            }
        }

        std::optional<QuestData> ReadActiveQuest()
        {
            const auto& cfg = Settings::Get();

            auto* dataHandler = RE::TESDataHandler::GetSingleton();
            if (!dataHandler) {
                return std::nullopt;
            }

            RE::TESQuest* active = nullptr;
            for (auto* quest : dataHandler->GetFormArray<RE::TESQuest>()) {
                // VERIFY IN-GAME: IsActive() should be true only for the quest the player
                // has selected as active in the journal (same as Papyrus Quest.IsActive()).
                if (quest && quest->IsActive() && quest->IsRunning()) {
                    active = quest;
                    break;
                }
            }
            if (!active) {
                return std::nullopt;
            }

            QuestData out;
            out.title = active->GetFullName() ? active->GetFullName() : "";
            // VERIFY AT COMPILE TIME: the quest category accessor name can differ by CommonLibSSE-NG version
            // (GetType() vs data.questType). Values: 1 Main ... 8 Side/Favor ... 11 Dragonborn.
            out.type = SwfTypeFor(static_cast<int>(active->GetType()));

            for (auto* obj : active->objectives) {
                if (!obj || static_cast<int>(out.objectives.size()) >= cfg.maxObjectives) {
                    continue;
                }
                // Only objectives that are displayed and not completed or failed.
                if (obj->state.get() != RE::QUEST_OBJECTIVE_STATE::kDisplayed) {
                    continue;
                }
                // KNOWN LIMITATION: displayText can contain alias tokens such as <Alias=Target>
                // that the game resolves itself. They are passed through raw for now.
                const char* text = obj->displayText.c_str();
                if (text && *text) {
                    out.objectives.emplace_back(text);
                }
            }

            if (out.objectives.empty()) {
                return std::nullopt;
            }
            return out;
        }

        // ------------------------------------------------------------------ shared helpers

        std::atomic_bool g_running{ false };

        std::string Escape(const std::string& a_text)
        {
            std::string out;
            for (const char c : a_text) {
                switch (c) {
                case '&': out += "&amp;"; break;
                case '<': out += "&lt;"; break;
                case '>': out += "&gt;"; break;
                default: out += c; break;
                }
            }
            return out;
        }

        std::string HexColor(std::uint32_t a_rgb)
        {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "#%06X", a_rgb & 0xFFFFFF);
            return buf;
        }

        // ------------------------------------------------------------------ swf mode

        constexpr auto kListName = "QHT_List";
        constexpr auto kListPath = "_root.QHT_List";
        constexpr double kListDepth = 9876.0;
        constexpr int kMaxLoadTicks = 60;

        enum class SwfStage
        {
            kCreate,
            kLoading,
            kReady,
            kFailed
        };

        SwfStage g_swfStage{ SwfStage::kCreate };
        int g_loadTicks{ 0 };
        bool g_hasShown{ false };
        std::optional<QuestData> g_shown;

        void SwfCreate(RE::GFxMovieView* a_movie)
        {
            const auto& cfg = Settings::Get();

            RE::GFxValue root;
            if (!a_movie->GetVariable(&root, "_root")) {
                return;
            }

            RE::GFxValue args[2];
            args[0] = kListName;
            args[1] = kListDepth;
            RE::GFxValue clip;
            if (!root.Invoke("createEmptyMovieClip", &clip, args, 2) || !clip.IsObject()) {
                logger::warn("createEmptyMovieClip failed");
                g_swfStage = SwfStage::kFailed;
                return;
            }

            RE::GFxValue path(cfg.swfPath.c_str());
            clip.Invoke("loadMovie", nullptr, &path, 1);

            g_swfStage = SwfStage::kLoading;
            g_loadTicks = 0;
            g_hasShown = false;
            g_shown.reset();
            logger::info("Loading {}", cfg.swfPath);
        }

        // Called once the SWF has loaded and defined its functions on the clip.
        void SwfInit(RE::GFxValue& a_clip)
        {
            const auto& cfg = Settings::Get();

            // The SWF scales its entries by a global named SCALE that the original HUD defines.
            a_clip.SetMember("SCALE", 100.0);

            RE::GFxValue args[3];
            args[0] = static_cast<double>(cfg.swfX);
            args[1] = static_cast<double>(cfg.swfY);
            args[2] = static_cast<double>(cfg.swfMaxHeight);
            a_clip.Invoke("QuestItemList", nullptr, args, 3);

            if (cfg.registerHudElement) {
                a_clip.Invoke("AddToHudElements");
            }

            g_swfStage = SwfStage::kReady;
            logger::info("Quest list SWF ready");
        }

        void SwfSync(RE::GFxMovieView* a_movie, RE::GFxValue& a_clip)
        {
            const auto current = ReadActiveQuest();

            if (g_hasShown && current == g_shown) {
                a_clip.Invoke("Update");
                return;
            }

            a_clip.Invoke("RemoveAllQuests");

            if (current) {
                RE::GFxValue objectives;
                a_movie->CreateArray(&objectives);
                for (const auto& line : current->objectives) {
                    RE::GFxValue entry(line.c_str());
                    objectives.PushBack(entry);
                }

                RE::GFxValue args[5];
                args[0] = current->type.c_str();
                args[1] = current->title.c_str();
                args[2] = false;  // a_isInSameLocation
                args[3] = objectives;
                args[4] = 0.0;  // a_ageIndex
                a_clip.Invoke("AddQuest", nullptr, args, 5);
                a_clip.Invoke("ShowQuest");
            }

            a_clip.Invoke("Update");
            g_shown = current;
            g_hasShown = true;
        }

        void UpdateSwf(RE::GFxMovieView* a_movie)
        {
            if (g_swfStage == SwfStage::kFailed) {
                return;
            }

            // The HUD movie is rebuilt on load; if our clip is gone, start over.
            RE::GFxValue clip;
            const bool exists = a_movie->GetVariable(&clip, kListPath) && clip.IsObject();
            if (!exists) {
                g_swfStage = SwfStage::kCreate;
            }

            switch (g_swfStage) {
            case SwfStage::kCreate:
                SwfCreate(a_movie);
                break;

            case SwfStage::kLoading: {
                RE::GFxValue fn;
                if (clip.GetMember("AddQuest", &fn) && !fn.IsUndefined() && !fn.IsNull()) {
                    SwfInit(clip);
                } else if (++g_loadTicks > kMaxLoadTicks) {
                    logger::error("QuestItemList SWF did not load (is {} in Data/Interface?). Giving up.", Settings::Get().swfPath);
                    g_swfStage = SwfStage::kFailed;
                }
                break;
            }

            case SwfStage::kReady:
                SwfSync(a_movie, clip);
                break;

            default:
                break;
            }
        }

        // ------------------------------------------------------------------ text mode (fallback)

        constexpr auto kFieldName = "QHT_Field";
        constexpr auto kFieldPath = "_root.QHT_Field";
        constexpr double kFieldDepth = 9877.0;

        std::string g_lastHtml;

        std::string BuildHtml(const std::optional<QuestData>& a_quest)
        {
            if (!a_quest) {
                return {};
            }
            const auto& cfg = Settings::Get();

            // Styling copied from the vanilla QuestItemList.swf text fields.
            const auto fontAttrs = [&](float a_size, std::uint32_t a_color) {
                return "<font face=\"$EverywhereMediumFont\" size=\"" + std::to_string(static_cast<int>(a_size)) +
                       "\" color=\"" + HexColor(a_color) + "\" letterSpacing=\"1.2\" kerning=\"1\">";
            };

            std::string html = "<p align=\"left\">" + fontAttrs(cfg.titleSize, cfg.titleColor) + Escape(a_quest->title) + "</font></p>";
            for (const auto& line : a_quest->objectives) {
                html += "<p align=\"left\">" + fontAttrs(cfg.objectiveSize, cfg.objectiveColor) + Escape(line) + "</font></p>";
            }
            return html;
        }

        bool CreateField(RE::GFxMovieView* a_movie)
        {
            const auto& cfg = Settings::Get();

            RE::GFxValue root;
            if (!a_movie->GetVariable(&root, "_root")) {
                return false;
            }

            RE::GFxValue args[6];
            args[0] = kFieldName;
            args[1] = kFieldDepth;
            args[2] = static_cast<double>(cfg.x);
            args[3] = static_cast<double>(cfg.y);
            args[4] = static_cast<double>(cfg.width);
            args[5] = static_cast<double>(cfg.height);

            RE::GFxValue field;
            if (!root.Invoke("createTextField", &field, args, 6) || !field.IsObject()) {
                logger::warn("createTextField failed");
                return false;
            }

            field.SetMember("html", true);
            field.SetMember("multiline", true);
            field.SetMember("wordWrap", true);
            field.SetMember("selectable", false);
            field.SetMember("_alpha", static_cast<double>(cfg.opacity));

            RE::GFxValue format;
            a_movie->CreateObject(&format, "TextFormat");
            if (format.IsObject()) {
                format.SetMember("font", "$EverywhereMediumFont");
                field.Invoke("setNewTextFormat", nullptr, &format, 1);
            }

            g_lastHtml.clear();
            logger::info("HUD text field created");
            return true;
        }

        void UpdateText(RE::GFxMovieView* a_movie)
        {
            RE::GFxValue field;
            if (!a_movie->GetVariable(&field, kFieldPath) || !field.IsObject()) {
                if (!CreateField(a_movie)) {
                    return;
                }
                a_movie->GetVariable(&field, kFieldPath);
            }

            const auto html = BuildHtml(ReadActiveQuest());
            if (html == g_lastHtml) {
                return;
            }
            g_lastHtml = html;
            field.SetMember("htmlText", html.c_str());
        }

        // ------------------------------------------------------------------ loop

        // Runs on the UI thread.
        void Update()
        {
            auto* ui = RE::UI::GetSingleton();
            if (!ui || !ui->IsMenuOpen(RE::HUDMenu::MENU_NAME)) {
                return;
            }

            auto movie = ui->GetMovieView(RE::HUDMenu::MENU_NAME);
            if (!movie) {
                return;
            }

            if (Settings::Get().mode == "text") {
                UpdateText(movie.get());
            } else {
                UpdateSwf(movie.get());
            }
        }

        void Loop()
        {
            while (g_running.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(Settings::Get().refreshMs));
                if (auto* tasks = SKSE::GetTaskInterface()) {
                    tasks->AddUITask([] { Update(); });
                }
            }
        }
    }

    void Start()
    {
        if (!Settings::Get().enabled) {
            logger::info("Disabled in ini, not starting");
            return;
        }
        if (g_running.exchange(true)) {
            return;
        }
        std::thread(Loop).detach();
        logger::info("Tracker started (mode: {})", Settings::Get().mode);
    }
}
