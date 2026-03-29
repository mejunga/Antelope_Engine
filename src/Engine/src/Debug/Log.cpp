#include <Engine/Debug/Log.hpp>

#include <spdlog/sinks/stdout_color_sinks.h>


namespace Antelope
{
    std::shared_ptr<spdlog::logger> Log::s_CoreLogger;
    std::shared_ptr<spdlog::logger> Log::s_ClientLogger;

    void Log::Init()
    {
        s_CoreLogger = spdlog::stdout_color_mt("ENGINE");
        s_CoreLogger->set_level(spdlog::level::trace);
        s_CoreLogger->set_pattern("%^[%T]%$\033[1;33m[%n]\033[0m: %^%v%$");

        s_ClientLogger = spdlog::stdout_color_mt("CLIENT");
        s_ClientLogger->set_level(spdlog::level::trace);
        s_ClientLogger->set_pattern("%^[%T]%$\033[1;34m[%n]\033[0m: %^%v%$");

        AE_ENGINE_INFO("Log System initialized.");
    }
}