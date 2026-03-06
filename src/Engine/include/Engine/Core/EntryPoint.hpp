#pragma once

#include <Engine/Core/Application.hpp>
#include <Engine/Debug/Log.hpp>

extern Antelope::Application* Antelope::CreateApplication();

int main(int argc, char** argv)
{
    Antelope::Log::Init();    
    auto app { Antelope::CreateApplication() };
    app->Run();
    delete app;
    
    return 0;
}