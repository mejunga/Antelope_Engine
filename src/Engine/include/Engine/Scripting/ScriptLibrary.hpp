#pragma once

#include <Engine/Scripting/Script.hpp>
#include <Engine/Scripting/GameSystem.hpp>

#include <string>
#include <vector>


namespace Antelope
{
    class ScriptLibrary
    {
        public:
            using ScriptCreateFn = Script*(*)();
            using ScriptDestroyFn = void(*)(Script*);
            using SystemCreateFn = GameSystem*(*)();
            using SystemDestroyFn = void(*)(GameSystem*);

            static ScriptLibrary& Get();

            void Load(const std::string& dllPath);
            void Unload();

            bool IsLoaded() const { return m_Handle != nullptr; }
            const std::string& GetPath() const { return m_DllPath; }
            void* GetSymbol(const std::string& name);

            std::vector<std::string> GetRegisteredScripts();
            std::vector<std::string> GetRegisteredSystems();

            ScriptCreateFn GetScriptCreateFn(const std::string& className);
            ScriptDestroyFn GetScriptDestroyFn(const std::string& className);
            SystemCreateFn GetSystemCreateFn(const std::string& className);
            SystemDestroyFn GetSystemDestroyFn(const std::string& className);

        private:
            void CopyToTemp();

        private:
            void *m_Handle { nullptr };
            
            std::string m_DllPath;
            std::string m_TempDllPath;
    };
}