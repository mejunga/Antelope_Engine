#include <Engine/Core/Application.hpp>
#include <Engine/Core/EntryPoint.hpp>
#include <Engine/Debug/Log.hpp>

class EditorApp : public Antelope::Application
{
    public:
        EditorApp()
        {
            AE_CLIENT_INFO("Antelope Editor instance created.");
        }

        ~EditorApp()
        {
            AE_CLIENT_INFO("Antelope Editor instance destroyed.");
        }
};

Antelope::Application* Antelope::CreateApplication() { return new EditorApp(); }