#include <Editor/Core/AntelopeApp.hpp>

#include <Engine/AssetImport/ModelLoader.hpp>
#include <Engine/AssetImport/TextureManager.hpp>
#include <Engine/Renderer/Graphics/Renderer.hpp>
#include <Engine/Renderer/Vulkan/RenderTexture.hpp>
#include <Engine/ECS/World.hpp>
#include <Engine/ECS/BaseComponents.hpp>
#include <Engine/Platform/Input.hpp>
#include <Engine/Debug/Log.hpp>
#include <Engine/Renderer/UI/UIContext.hpp>

#include <imgui.h>
#include <GLFW/glfw3.h>


AntelopeApp::AntelopeApp() 
    : m_EditorCamera(glm::vec3(0.0f, 2.0f, 20.0f))
{
    AE_CLIENT_INFO("Antelope Editor instance created.");
}

AntelopeApp::~AntelopeApp()
{
    AE_CLIENT_INFO("Antelope Editor instance destroyed.");
}

void AntelopeApp::SetupMockData()
{
    m_BearTexID = GetTextureManager()->LoadTexture("Assets/Bear.png");
    m_BearMesh = Antelope::ModelLoader::Load("Assets/Bear_DEMO.fbx");

    m_GorillaTexID = GetTextureManager()->LoadTexture("Assets/gorilla.png");
    m_GorillaMesh = Antelope::ModelLoader::Load("Assets/Gorilla_hd.fbx");

    GetRenderer()->UpdateTextureDescriptors(GetTextureManager()->GetGlobalTextures());
}

void AntelopeApp::OnInit()
{
    SetupMockData();
}

void AntelopeApp::OnUpdate(float deltaTime)
{
    if (m_DebounceTimer > 0.0f)
    {
        m_DebounceTimer -= deltaTime;
    }

    if (Antelope::Input::IsKeyPressed(GLFW_KEY_SPACE) && m_DebounceTimer <= 0.0f)
    {
        m_RenderState = (m_RenderState + 1) % 3;
        m_DebounceTimer = DEBOUNCE_DELAY;
        AE_CLIENT_TRACE("Space Key Pressed! Switching to State: {0}", m_RenderState);

        if (!m_ActiveEntities.empty())
        {
            for (auto entity : m_ActiveEntities)
            {
                auto& meshComponent { entity.GetComponent<Antelope::MeshComponent>() };
                GetRenderer()->FreeMesh(meshComponent.Handle);
                GetWorld()->DestroyEntity(entity);
            }
            m_ActiveEntities.clear();
        }

        if (m_RenderState == 1) 
        {
            AE_CLIENT_INFO("Creating Bear Entities...");
            for (const auto& subMesh : m_BearMesh.SubMeshes)
            {
                Antelope::MeshHandle handle { GetRenderer()->UploadMesh(subMesh.Data) };
                handle.materialIndex = m_BearTexID;

                Antelope::Entity entity { GetWorld()->CreateEntity(subMesh.Name.empty() ? "BearPart" : subMesh.Name) };
                entity.AddComponent<Antelope::MeshComponent>(handle);
                
                auto& transform { entity.GetComponent<Antelope::TransformComponent>() };
                transform.Scale = glm::vec3(0.005f);
                
                m_ActiveEntities.push_back(entity);
            }
        }
        else if (m_RenderState == 2)
        {
            AE_CLIENT_INFO("Creating Gorilla Entities...");
            for (const auto& subMesh : m_GorillaMesh.SubMeshes)
            {
                Antelope::MeshHandle handle { GetRenderer()->UploadMesh(subMesh.Data) };
                handle.materialIndex = m_GorillaTexID;

                Antelope::Entity entity { GetWorld()->CreateEntity(subMesh.Name.empty() ? "GorillaPart" : subMesh.Name) };
                entity.AddComponent<Antelope::MeshComponent>(handle);
                
                auto& transform { entity.GetComponent<Antelope::TransformComponent>() };
                transform.Translation = glm::vec3(0.0f, 0.0f, 0.0f);
                transform.Scale = glm::vec3(7.5f);
                
                m_ActiveEntities.push_back(entity);
            }
        }
    }

    m_EditorCamera.OnUpdate(deltaTime);
    GetWorld()->OnUpdateEditor(deltaTime, m_EditorCamera);
}

void AntelopeApp::OnUIRender()
{
    m_ScenePanel.OnUIRender();
}

void AntelopeApp::OnShutdown()
{
    for (auto entity : m_ActiveEntities) 
    { 
        auto& meshComponent { entity.GetComponent<Antelope::MeshComponent>() };
        GetRenderer()->FreeMesh(meshComponent.Handle); 
    }
    m_ActiveEntities.clear();
}