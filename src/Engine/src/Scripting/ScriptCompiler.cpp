#ifdef ANTELOPE_EDITOR_MODE
#include <Engine/Scripting/ScriptCompiler.hpp>
#include <Engine/Debug/Log.hpp>

#include <yaml-cpp/yaml.h>
#include <filesystem>
#include <sstream>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace Antelope
{
    bool ScriptCompiler::LoadConfig(const std::string& configPath, Config& outConfig)
    {
        if (!std::filesystem::exists(configPath))
        {
            AE_ENGINE_ERROR("ScriptCompiler: Config not found at '{0}'.", configPath);
            return false;
        }

        try
        {
            YAML::Node node { YAML::LoadFile(configPath) };
            outConfig.CompilerPath = node["Compiler"].as<std::string>();
            outConfig.Flags = node["Flags"].as<std::string>("");

            for (const auto& dir : node["IncludeDirs"]) { outConfig.IncludeDirs.push_back(dir.as<std::string>()); }
            for (const auto& dir : node["LibDirs"]) { outConfig.LibDirs.push_back(dir.as<std::string>()); }
            for (const auto& lib : node["Libs"]) { outConfig.Libs.push_back(lib.as<std::string>()); }
        }
        catch (const YAML::Exception& e)
        {
            AE_ENGINE_ERROR("ScriptCompiler: Failed to parse config: {0}.", e.what());
            return false;
        }

        return true;
    }

    std::string ScriptCompiler::BuildCommand(const Config& config, const std::string& scriptsDir, const std::string& generatedDir, const std::string& outputDir, const std::vector<std::string>& extraIncludeDirs)
    {
        std::ostringstream cmd;
        cmd << "\"" << config.CompilerPath << "\"";
        cmd << " /LD /nologo " << config.Flags;

        for (const auto& dir : extraIncludeDirs)  { cmd << " /I\"" << dir << "\""; }
        for (const auto& dir : config.IncludeDirs) { cmd << " /I\"" << dir << "\""; }
        cmd << " /I\"" << scriptsDir   << "\"";
        cmd << " /I\"" << generatedDir << "\"";

        for (const auto& entry : std::filesystem::recursive_directory_iterator(scriptsDir))
        {
            if (entry.path().extension() == ".cpp")
            {
                cmd << " \"" << entry.path().string() << "\"";
            }
        }

        std::string generatedCpp { (std::filesystem::path(generatedDir) / "Scripts.generated.cpp").string() };
        if (std::filesystem::exists(generatedCpp)) { cmd << " \"" << generatedCpp << "\""; }

    #ifdef NDEBUG
        std::string outputDll { (std::filesystem::path(outputDir) / "Scripts.dll").string() };
    #else
        std::string outputDll { (std::filesystem::path(outputDir) / "Scripts_d.dll").string() };
    #endif

        std::replace(outputDll.begin(), outputDll.end(), '\\', '/');
        std::string outputObj { outputDir };
        std::replace(outputObj.begin(), outputObj.end(), '\\', '/');
        outputObj += '/';
        cmd << " /Fo\"" << outputObj << "\"";
        cmd << " /Fe\"" << outputDll << "\"";
        cmd << " /link";

        for (const auto& dir : config.LibDirs) { cmd << " /LIBPATH:\"" << dir << "\""; }
        for (const auto& lib : config.Libs) { cmd << " " << lib; }

        return cmd.str();
    }

    std::string ScriptCompiler::ExecuteProcess(const std::string& command, bool& outSuccess)
    {
    #ifdef _WIN32
        SECURITY_ATTRIBUTES sa {};
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = nullptr;

        HANDLE hRead { nullptr };
        HANDLE hWrite { nullptr };
        CreatePipe(&hRead, &hWrite, &sa, 0);
        SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOA si {};
        si.cb = sizeof(STARTUPINFOA);
        si.hStdOutput = hWrite;
        si.hStdError = hWrite;
        si.dwFlags = STARTF_USESTDHANDLES;

        PROCESS_INFORMATION pi {};
        std::string cmd { command };

        bool created { CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi) != 0 };
        CloseHandle(hWrite);

        std::string output;
        if (created)
        {
            char buf[4096];
            DWORD read { 0 };
            while (ReadFile(hRead, buf, sizeof(buf) - 1, &read, nullptr) && read > 0)
            {
                buf[read] = '\0';
                output += buf;
            }

            WaitForSingleObject(pi.hProcess, INFINITE);
            DWORD exitCode { 0 };
            GetExitCodeProcess(pi.hProcess, &exitCode);
            outSuccess = (exitCode == 0);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
        else
        {
            outSuccess = false;
            output = "Failed to launch compiler process.";
        }

        CloseHandle(hRead);
        return output;
    #else
        std::string cmd { command + " 2>&1" };
        FILE* pipe { popen(cmd.c_str(), "r") };
        if (!pipe) { outSuccess = false; return "Failed to launch compiler process."; }

        std::string output;
        char buf[256];
        while (fgets(buf, sizeof(buf), pipe)) { output += buf; }

        outSuccess = (pclose(pipe) == 0);
        return output;
    #endif
    }

    ScriptCompiler::Result ScriptCompiler::Compile(
        const std::string& scriptsDir,
        const std::string& generatedDir,
        const std::string& outputDir,
        const std::string& configPath,
        const std::vector<std::string>& extraIncludeDirs)
    {
        Config config;
        if (!LoadConfig(configPath, config)) { return { false, "Failed to load compile config." }; }

        std::filesystem::create_directories(outputDir);

        std::string command { BuildCommand(config, scriptsDir, generatedDir, outputDir, extraIncludeDirs) };
        AE_ENGINE_TRACE("ScriptCompiler: {0}", command);

        bool success { false };
        std::string output { ExecuteProcess(command, success) };

        if (success) { AE_ENGINE_INFO("ScriptCompiler: Build succeeded."); }
        else { AE_ENGINE_ERROR("ScriptCompiler: Build failed.\n{0}", output); }

        return { success, output };
    }
}
#endif