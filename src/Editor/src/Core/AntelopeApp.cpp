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

    m_BearRoot = GetWorld()->CreateEntity("BearRoot");

    for (const auto& subMesh : m_BearMesh.SubMeshes)
    {
        Antelope::MeshHandle handle { GetRenderer()->UploadMesh(subMesh.Data) };
        handle.materialIndex = m_BearTexID;

        Antelope::Entity part { GetWorld()->CreateEntity(subMesh.Name.empty() ? "BearPart" : subMesh.Name) };
        part.AddComponent<Antelope::MeshComponent>(handle);
        part.GetComponent<Antelope::TransformComponent>().Scale = glm::vec3(0.005f);
        part.SetParent(m_BearRoot);
    }

    m_GorillaRoot = GetWorld()->CreateEntity("GorillaRoot");
    m_GorillaRoot.SetParent(m_BearRoot); 
    m_GorillaRoot.GetComponent<Antelope::TransformComponent>().Translation = glm::vec3(10.0f, 0.0f, 0.0f);
    GetWorld()->MarkTransformDirty(m_GorillaRoot);

    for (const auto& subMesh : m_GorillaMesh.SubMeshes)
    {
        Antelope::MeshHandle handle { GetRenderer()->UploadMesh(subMesh.Data) };
        handle.materialIndex = m_GorillaTexID;

        Antelope::Entity part { GetWorld()->CreateEntity(subMesh.Name.empty() ? "GorillaPart" : subMesh.Name) };
        part.AddComponent<Antelope::MeshComponent>(handle);
        part.GetComponent<Antelope::TransformComponent>().Scale = glm::vec3(7.5f);
        part.SetParent(m_GorillaRoot);
        m_GorillaParts.push_back(part);
    }
}

void AntelopeApp::OnUpdate(float timeStep)
{
    if (m_DebounceTimer > 0.0f) { m_DebounceTimer -= timeStep; }

    if (Antelope::Input::IsKeyPressed(GLFW_KEY_SPACE) && m_DebounceTimer <= 0.0f)
    {
        m_DebounceTimer = DEBOUNCE_DELAY;
        bool isActive { m_GorillaRoot.IsActive() };
        
        m_GorillaRoot.SetActive(!isActive); 
        
        for (auto& part : m_GorillaParts)
        {
            part.SetActive(!isActive);
        }
        
        AE_CLIENT_TRACE("Gorilla Active State: {0}", !isActive);
    }

    m_EditorCamera.OnUpdate(timeStep);
    GetWorld()->OnUpdateEditor(timeStep, m_EditorCamera);
}

void AntelopeApp::OnUIRender()
{
    m_ScenePanel.OnUIRender(m_EditorCamera);
}

void AntelopeApp::OnShutdown()
{
    auto& registry { GetWorld()->GetRegistry() };
    auto view { registry.view<Antelope::MeshComponent>() };

    for (auto [entity, mesh] : view.each()) 
    {
        GetRenderer()->FreeMesh(mesh.Handle); 
    }

    AE_CLIENT_INFO("All meshes freed from GPU.");
}