#pragma once

#include <Engine/Core/Application.hpp>
#include <Engine/Debug/Log.hpp>


extern Antelope::Application* Antelope::CreateApplication(int argc, char** argv);

int main(int argc, char** argv)
{
    Antelope::Log::Init();
    auto app { Antelope::CreateApplication(argc, argv) };
    app->Run();
    delete app;
    return 0;
}