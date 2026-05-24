#include <Engine/ECS/System/AudioSystem.hpp>
#include <Engine/Audio/AudioContext.hpp>
#include <Engine/ECS/World.hpp>
#include <Engine/ECS/BaseComponents.hpp>
#include <Engine/Debug/Log.hpp>
#include <Engine/Asset/AssetManager.hpp>

#include <miniaudio.h>

namespace Antelope
{
    void AudioSystem::OnRuntimeStart(World& world, AudioContext& context)
    {
        if (!context.IsInitialized()) { return; }

        auto& registry { world.GetRegistry() };
        for (auto [entity, player] : registry.view<AudioPlayerComponent>().each())
        {
            for (uint32_t i { 0 }; i < static_cast<uint32_t>(player.Clips.size()); ++i)
            {
                if (player.Clips[i].PlayOnStart)
                {
                    PlayClip(world, context, entity, i);
                }
            }
        }
    }

    void AudioSystem::OnRuntimeStop(World& world, AudioContext& context)
    {
        auto& registry { world.GetRegistry() };
        for (auto [entity, player] : registry.view<AudioPlayerComponent>().each())
        {
            for (auto& clip : player.Clips)
            {
                if (clip.RuntimeSound)
                {
                    ma_sound_stop(static_cast<ma_sound*>(clip.RuntimeSound.get()));
                    clip.RuntimeSound.reset();
                }
            }
        }
    }

    void AudioSystem::PlayClip(World& world, AudioContext& context, entt::entity entity, uint32_t clipIndex)
    {
        if (!context.IsInitialized()) { return; }

        auto& registry { world.GetRegistry() };
        if (!registry.all_of<AudioPlayerComponent>(entity)) { return; }

        auto& player { registry.get<AudioPlayerComponent>(entity) };
        if (clipIndex >= static_cast<uint32_t>(player.Clips.size())) { return; }

        auto& clip { player.Clips[clipIndex] };
        if ((uint64_t)clip.AudioAssetUUID == 0) { return; }

        if (clip.RuntimeSound)
        {
            ma_sound_stop(static_cast<ma_sound*>(clip.RuntimeSound.get()));
            clip.RuntimeSound.reset();
        }

        const auto& meta { AssetManager::GetMetadata(clip.AudioAssetUUID) };

        if (!meta.IsValid())
        {
            AE_ENGINE_WARN("AudioSystem: Audio asset {0} not found in registry.", (uint64_t)clip.AudioAssetUUID);
            return;
        }

        std::string resolvedStr { meta.FilePath.string() };

        auto* sound { new ma_sound() };
        if (ma_sound_init_from_file(&context.GetEngine(), resolvedStr.c_str(), 0, nullptr, nullptr, sound) != MA_SUCCESS)
        {
            AE_ENGINE_WARN("AudioSystem: Failed to load '{0}'.", resolvedStr);
            delete sound;
            return;
        }

        ma_sound_set_volume(sound, clip.Volume);
        ma_sound_set_looping(sound, clip.Loop ? MA_TRUE : MA_FALSE);
        ma_sound_start(sound);

        clip.RuntimeSound = std::shared_ptr<void>(sound, [](void* p)
        {
            auto* s { static_cast<ma_sound*>(p) };
            ma_sound_uninit(s);
            delete s;
        });
    }

    void AudioSystem::StopClip(World& world, entt::entity entity, uint32_t clipIndex)
    {
        auto& registry { world.GetRegistry() };
        if (!registry.all_of<AudioPlayerComponent>(entity)) { return; }

        auto& player { registry.get<AudioPlayerComponent>(entity) };
        if (clipIndex >= static_cast<uint32_t>(player.Clips.size())) { return; }

        auto& clip { player.Clips[clipIndex] };
        if (clip.RuntimeSound)
        {
            ma_sound_stop(static_cast<ma_sound*>(clip.RuntimeSound.get()));
            clip.RuntimeSound.reset();
        }
    }
}