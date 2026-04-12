#include <Engine/Debug/Log.hpp>
#include <Engine/Debug/EditorConsoleSink.hpp>

#include <spdlog/sinks/stdout_color_sinks.h>

#include <vector>


namespace Antelope
{
    std::shared_ptr<spdlog::logger> Log::s_CoreLogger;
    std::shared_ptr<spdlog::logger> Log::s_ClientLogger;
    std::vector<LogMessage> EditorConsoleSink::s_Messages;

    void Log::Init()
    {
        auto editorSink { std::make_shared<EditorConsoleSink>() };
        
        auto engineTerminalSink { std::make_shared<spdlog::sinks::stdout_color_sink_mt>() };
        engineTerminalSink->set_pattern("%^[%T]%$\033[1;33m[%n]\033[0m: %^%v%$");
        
        std::vector<spdlog::sink_ptr> coreSinks { engineTerminalSink, editorSink };
        s_CoreLogger = std::make_shared<spdlog::logger>("ENGINE", coreSinks.begin(), coreSinks.end());
        s_CoreLogger->set_level(spdlog::level::trace);

        auto clientTerminalSink { std::make_shared<spdlog::sinks::stdout_color_sink_mt>() };
        clientTerminalSink->set_pattern("%^[%T]%$\033[1;34m[%n]\033[0m: %^%v%$");

        std::vector<spdlog::sink_ptr> clientSinks { clientTerminalSink, editorSink };
        s_ClientLogger = std::make_shared<spdlog::logger>("CLIENT", clientSinks.begin(), clientSinks.end());
        s_ClientLogger->set_level(spdlog::level::trace);

        AE_ENGINE_INFO("Log System initialized.");
    }
}