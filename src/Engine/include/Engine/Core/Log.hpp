#pragma once

#include <memory>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

namespace Antelope
{
    class Log
    {
        public:
            static void Init();

            inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() {return s_CoreLogger;}
            inline static std::shared_ptr<spdlog::logger>& GetClientLogger() {return s_ClientLogger;}

        private:
            static std::shared_ptr<spdlog::logger> s_CoreLogger;
            static std::shared_ptr<spdlog::logger> s_ClientLogger;
    };
}

#define AE_ENGINE_TRACE(...)    ::Antelope::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define AE_ENGINE_INFO(...)     ::Antelope::Log::GetCoreLogger()->info(__VA_ARGS__)
#define AE_ENGINE_WARN(...)     ::Antelope::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define AE_ENGINE_ERROR(...)    ::Antelope::Log::GetCoreLogger()->error(__VA_ARGS__)
#define AE_ENGINE_CRITICAL(...) ::Antelope::Log::GetCoreLogger()->critical(__VA_ARGS__)

#define AE_CLIENT_TRACE(...)    ::Antelope::Log::GetClientLogger()->trace(__VA_ARGS__)
#define AE_CLIENT_INFO(...)     ::Antelope::Log::GetClientLogger()->info(__VA_ARGS__)
#define AE_CLIENT_WARN(...)     ::Antelope::Log::GetClientLogger()->warn(__VA_ARGS__)
#define AE_CLIENT_ERROR(...)    ::Antelope::Log::GetClientLogger()->error(__VA_ARGS__)
#define AE_CLIENT_CRITICAL(...) ::Antelope::Log::GetClientLogger()->critical(__VA_ARGS__)