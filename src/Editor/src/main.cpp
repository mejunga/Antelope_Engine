#include <Editor/Core/AntelopeApp.hpp>

#include <Engine/Core/Application.hpp>
#include <Engine/Core/EntryPoint.hpp>


Antelope::Application* Antelope::CreateApplication(int argc, char** argv)
{
    std::string projectRoot { argc > 1 ? argv[1] : "." };
    return new Antelope::Editor::AntelopeApp(projectRoot);
}