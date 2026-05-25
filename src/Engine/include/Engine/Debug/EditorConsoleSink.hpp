#pragma once

#ifndef ANTELOPE_API
    #ifdef ANTELOPE_BUILD_DLL
        #define ANTELOPE_API __declspec(dllexport)
    #else
        #define ANTELOPE_API __declspec(dllimport)
    #endif
#endif

#include <spdlog/sinks/base_sink.h>

#include <mutex>
#include <vector>
#include <string>


namespace Antelope
{
    struct LogMessage
    {
        std::string LoggerName;
        std::string Payload;
        spdlog::level::level_enum Level;
    };

    class EditorConsoleSink : public spdlog::sinks::base_sink<std::mutex>
    {
        public:
            static ANTELOPE_API std::vector<LogMessage> s_Messages;
            
        protected:
            void sink_it_(const spdlog::details::log_msg& msg) override
            {
                LogMessage message;
                message.LoggerName = std::string(msg.logger_name.data(), msg.logger_name.size());
                message.Payload = std::string(msg.payload.data(), msg.payload.size());
                message.Level = msg.level;
                s_Messages.push_back(message);
            }
            
            void flush_() override {}
    };
}