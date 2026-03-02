#include <Engine/Core/Application.hpp>
#include <Engine/Core/EntryPoint.hpp>
#include <Engine/Core/Log.hpp>

class EditorApp : public Antelope::Application
{
    public:
        EditorApp()
        {
            AE_CLIENT_TRACE("Editor is created");
        }

        ~EditorApp()
        {
            AE_CLIENT_TRACE("Editor is destroyed");
        }
};

Antelope::Application* Antelope::CreateApplication(){return new EditorApp();} 
