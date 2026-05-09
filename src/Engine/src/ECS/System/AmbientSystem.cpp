#include <Engine/ECS/System/AmbientSystem.hpp>
#include <Engine/ECS/BaseComponents.hpp>
#include <Engine/ECS/World.hpp>

#include <glm/gtc/constants.hpp>
#include <glm/common.hpp>
#include <cmath>

namespace Antelope
{
    static constexpr float kLatitude = 38.0f * (3.14159265f / 180.0f);
    static constexpr float kDeclination = 15.0f * (3.14159265f / 180.0f);
    static constexpr float kMoonInclination = 0;
    static constexpr float kSynodicMonth = 29.5f;
    static constexpr float kEclipseSeasonDays = 173.0f;

    void AmbientSystem::OnUpdate(World& world, float timeStep)
    {
        auto& registry { world.GetRegistry() };
        auto ambView { registry.view<AmbientComponent, TimeCycleComponent>(entt::exclude<DisabledComponent>) };

        for (auto [entity, ambient, timeCycle] : ambView.each())
        {
            timeCycle.TimeOfDay += timeStep * timeCycle.TimeScale / 3600.0f;

            if (timeCycle.TimeOfDay >= 24.0f) { timeCycle.TimeOfDay -= 24.0f; timeCycle.CurrentDay++; }

            float t { (timeCycle.TimeOfDay - 6.0f) / 12.0f };
            float hourAngle { (t - 0.5f) * glm::pi<float>() };

            float sinLat { sinf(kLatitude) }, cosLat { cosf(kLatitude) };
            float sinDec { sinf(kDeclination) }, cosDec { cosf(kDeclination) };
            float cosH { cosf(hourAngle) }, sinH { sinf(hourAngle) };

            float sinSunElev { sinLat * sinDec + cosLat * cosDec * cosH };
            float sunElevation { asinf(glm::clamp(sinSunElev, -1.0f, 1.0f)) };
            float sunAzimuth { atan2f(-cosDec * sinH, cosLat * sinDec - sinLat * cosDec * cosH) };

            float totalHours { static_cast<float>(timeCycle.CurrentDay) * 24.0f + timeCycle.TimeOfDay };
            float moonPhase { (totalHours / (kSynodicMonth * 24.0f)) * glm::two_pi<float>() };
            timeCycle.MoonPhase = moonPhase; 
            float ascNodeAngle { (totalHours / (kEclipseSeasonDays * 24.0f)) * glm::two_pi<float>() };

            float moonDec { kDeclination + kMoonInclination * sinf(moonPhase - ascNodeAngle) };
            float moonHA { hourAngle - moonPhase };

            float moonSinDec { sinf(moonDec) }, moonCosDec { cosf(moonDec) };
            float moonCosH { cosf(moonHA) }, moonSinH { sinf(moonHA) };

            float sinMoonElev { sinLat * moonSinDec + cosLat * moonCosDec * moonCosH };
            float moonElevation { asinf(glm::clamp(sinMoonElev, -1.0f, 1.0f)) };
            float moonAzimuth { atan2f(-moonCosDec * moonSinH, cosLat * moonSinDec - sinLat * moonCosDec * moonCosH) };

            if (ambient.SunEntity != entt::null && registry.valid(ambient.SunEntity))
            {
                if (auto* tr { registry.try_get<TransformComponent>(ambient.SunEntity) })
                {
                    tr->Rotation = glm::vec3(sunElevation, sunAzimuth, 0.0f);
                    registry.emplace_or_replace<DirtyTransform>(ambient.SunEntity);
                }

                if (auto* light { registry.try_get<DirectionalLightComponent>(ambient.SunEntity) })
                {
                    float warmth { 1.0f - glm::smoothstep(-0.1f, 0.35f, sunElevation) };
                    light->Color = glm::mix(glm::vec3(1.0f, 0.98f, 0.92f), glm::vec3(1.0f, 0.45f, 0.15f), warmth);
                    light->Intensity = glm::clamp(sinSunElev, 0.05f, 1.0f) * ambient.SunMaxIntensity;
                }
            }

            if (ambient.MoonEntity != entt::null && registry.valid(ambient.MoonEntity))
            {
                if (auto* tr { registry.try_get<TransformComponent>(ambient.MoonEntity) })
                {
                    tr->Rotation = glm::vec3(moonElevation, moonAzimuth, 0.0f);
                    registry.emplace_or_replace<DirtyTransform>(ambient.MoonEntity);
                }

                if (auto* light { registry.try_get<DirectionalLightComponent>(ambient.MoonEntity) })
                {
                    float phaseBrightness { (1.0f - cosf(moonPhase)) * 0.5f };
                    light->Color = glm::vec3(0.7f, 0.8f, 1.0f);
                    light->Intensity = glm::max(sinMoonElev, 0.0f) * phaseBrightness * ambient.MoonMaxIntensity;
                }
            }

            break;
        }
    }
}
