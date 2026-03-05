#pragma once

#include <Engine/Core/Application.hpp>
#include <Engine/Debug/Log.hpp>

extern Antelope::Application* Antelope::CreateApplication();

int main(int argc, char** argv)
{
    Antelope::Log::Init();
    AE_ENGINE_INFO("Engine is working");

    auto app { Antelope::CreateApplication() };
    AE_ENGINE_INFO("Editor is working");

    app->Run();
    delete app;
    return 0;
}