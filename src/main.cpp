#include "PCH.h"
#include "Settings.h"
#include "Tracker.h"

namespace
{
    void SetupLog()
    {
        auto path = SKSE::log::log_directory();
        if (!path) {
            return;
        }
        *path /= "QuestHUDTracker.log";
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
        auto log = std::make_shared<spdlog::logger>("global", std::move(sink));
        log->set_level(spdlog::level::info);
        log->flush_on(spdlog::level::info);
        spdlog::set_default_logger(std::move(log));
        spdlog::set_pattern("[%H:%M:%S] [%l] %v");
    }

    void OnMessage(SKSE::MessagingInterface::Message* a_msg)
    {
        if (a_msg->type == SKSE::MessagingInterface::kDataLoaded) {
            Tracker::Start();
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
    SetupLog();
    SKSE::Init(a_skse);

    Settings::Get().Load("Data/SKSE/Plugins/QuestHUDTracker.ini");

    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
    logger::info("QuestHUDTracker loaded");
    return true;
}
