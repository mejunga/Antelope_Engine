#pragma once

#include <Engine/Scripting/ScriptMacros.hpp>

#include <entt/entt.hpp>
#include <yaml-cpp/yaml.h>

#include <stdint.h>


namespace Antelope
{
    struct ScriptFieldDescriptorC
    {
        const char* Name;
        int32_t Type;
        size_t Offset;
    };

    struct ComponentDescriptorC
    {
        const char* Name;
        ScriptFieldDescriptorC* Fields;
        uint32_t FieldCount;

        void (*Add) (entt::registry*, entt::entity);
        void (*Remove) (entt::registry*, entt::entity);
        bool (*Has) (entt::registry*, entt::entity);
        void* (*Get) (entt::registry*, entt::entity);
        void (*Serialize) (YAML::Emitter*, entt::registry*, entt::entity);
        void (*Deserialize) (const YAML::Node*, entt::registry*, entt::entity);
    };
}

extern "C"
{
    ANTELOPE_SCRIPT_EXPORT Antelope::ComponentDescriptorC* GetComponentDescriptors(uint32_t* outCount);
    ANTELOPE_SCRIPT_EXPORT const char** GetRegisteredScripts (uint32_t* outCount);
    ANTELOPE_SCRIPT_EXPORT const char** GetRegisteredSystems (uint32_t* outCount);
}