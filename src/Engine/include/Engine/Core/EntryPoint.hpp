#pragma once

#include <Engine/Core/Application.hpp>
#include <Engine/Core/Log.hpp>

extern Antelope::Application* Antelope::CreateApplication();

int main(int argc, char** argv)
{
    Antelope::Log::Init();
    AE_CORE_WARN("Engine is working");

    auto app = Antelope::CreateApplication();
    AE_APP_INFO("Editor is working");

    app->Run();
    delete app;
    return 0;
}