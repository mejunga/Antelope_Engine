#pragma once

#ifdef ANTELOPE_EDITOR_MODE
#include <string>
#include <vector>


namespace Antelope
{
    class ScriptCompiler
    {
        public:
            struct Result
            {
                bool Success { false };
                std::string Output;
            };

            static Result Compile(
                const std::string& scriptsDir,
                const std::string& generatedDir,
                const std::string& outputDir,
                const std::string& configPath,
                const std::vector<std::string>& extraIncludeDirs = {});

        private:
            struct Config
            {
                std::string CompilerPath;
                std::vector<std::string> IncludeDirs;
                std::vector<std::string> LibDirs;
                std::vector<std::string> Libs;
                std::string  Flags;
            };

            static bool LoadConfig (const std::string& configPath, Config& outConfig);
            static std::string BuildCommand (const Config& config, const std::string& scriptsDir, const std::string& generatedDir, const std::string& outputDir, const std::vector<std::string>& extraIncludeDirs);
            static std::string ExecuteProcess (const std::string& command, bool& outSuccess);
    };
}
#endif