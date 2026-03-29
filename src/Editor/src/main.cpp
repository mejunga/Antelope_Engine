#include <Engine/Core/Application.hpp>
#include <Engine/Core/EntryPoint.hpp>
#include <Editor/Core/AntelopeApp.hpp>


Antelope::Application* Antelope::CreateApplication() 
{ 
    return new AntelopeApp(); 
}