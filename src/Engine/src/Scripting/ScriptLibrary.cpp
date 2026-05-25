#include <Engine/Scripting/ScriptLibrary.hpp>
#include <Engine/Debug/Log.hpp>

#include <filesystem>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define AE_LIB_LOAD(path) reinterpret_cast<void*>(LoadLibraryA(path))
#define AE_LIB_PROC(lib, name) GetProcAddress(reinterpret_cast<HMODULE>(lib), name)
#define AE_LIB_FREE(lib) FreeLibrary(reinterpret_cast<HMODULE>(lib))
#else
#include <dlfcn.h>
#define AE_LIB_LOAD(path) dlopen(path, RTLD_NOW | RTLD_LOCAL)
#define AE_LIB_PROC(lib, name) dlsym(lib, name)
#define AE_LIB_FREE(lib) dlclose(lib)
#endif


namespace Antelope
{
    ScriptLibrary& ScriptLibrary::Get()
    {
        static ScriptLibrary instance;
        return instance;
    }

    void ScriptLibrary::CopyToTemp()
    {
        std::filesystem::path src { m_DllPath };
        std::filesystem::path dst { src.parent_path() / (src.stem().string() + "_hot" + src.extension().string()) };
        
        try
        {
            std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing);
            m_TempDllPath = dst.string();
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            AE_ENGINE_ERROR("ScriptLibrary: Failed to copy DLL: {0}", e.what());
        }
    }

    void ScriptLibrary::Load(const std::string& dllPath)
    {
        if (m_Handle) { Unload(); }

        m_DllPath = dllPath;
        CopyToTemp();

        m_Handle = AE_LIB_LOAD(m_TempDllPath.c_str());

        if (!m_Handle)
        {
            AE_ENGINE_ERROR("ScriptLibrary: Failed to load '{0}'.", m_TempDllPath);
            return;
        }

        AE_ENGINE_INFO("ScriptLibrary: Loaded '{0}'.", m_TempDllPath);
    }

    void ScriptLibrary::Unload()
    {
        if (!m_Handle) { return; }

        AE_LIB_FREE(m_Handle);
        m_Handle = nullptr;
        AE_ENGINE_INFO("ScriptLibrary: Unloaded.");
    }

    void* ScriptLibrary::GetSymbol(const std::string& name)
    {
        if (!m_Handle) { return nullptr; }
        return reinterpret_cast<void*>(AE_LIB_PROC(m_Handle, name.c_str()));
    }

    std::vector<std::string> ScriptLibrary::GetRegisteredScripts()
    {
        using GetNamesFn = const char**(*)(uint32_t*);
        auto fn { reinterpret_cast<GetNamesFn>(GetSymbol("GetRegisteredScripts")) };

        if (!fn) { return {}; }

        uint32_t count { 0 };
        const char** names { fn(&count) };

        std::vector<std::string> result;
        result.reserve(count);

        for (uint32_t i { 0 }; i < count; ++i) { result.push_back(names[i]); }

        return result;
    }

    std::vector<std::string> ScriptLibrary::GetRegisteredSystems()
    {
        using GetNamesFn = const char**(*)(uint32_t*);
        auto fn { reinterpret_cast<GetNamesFn>(GetSymbol("GetRegisteredSystems")) };

        if (!fn) { return {}; }

        uint32_t count { 0 };
        const char** names { fn(&count) };

        std::vector<std::string> result;
        result.reserve(count);

        for (uint32_t i { 0 }; i < count; ++i) { result.push_back(names[i]); }

        return result;
    }

    ScriptLibrary::ScriptCreateFn ScriptLibrary::GetScriptCreateFn(const std::string& className)
    {
        return reinterpret_cast<ScriptCreateFn>(GetSymbol("CreateScript_" + className));
    }

    ScriptLibrary::ScriptDestroyFn ScriptLibrary::GetScriptDestroyFn(const std::string& className)
    {
        return reinterpret_cast<ScriptDestroyFn>(GetSymbol("DestroyScript_" + className));
    }

    ScriptLibrary::SystemCreateFn ScriptLibrary::GetSystemCreateFn(const std::string& className)
    {
        return reinterpret_cast<SystemCreateFn>(GetSymbol("CreateSystem_" + className));
    }

    ScriptLibrary::SystemDestroyFn ScriptLibrary::GetSystemDestroyFn(const std::string& className)
    {
        return reinterpret_cast<SystemDestroyFn>(GetSymbol("DestroySystem_" + className));
    }
}