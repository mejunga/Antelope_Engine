#ifdef ANTELOPE_EDITOR_MODE
#include <Engine/Renderer/UI/UIContext.hpp>
#include <Engine/Renderer/Vulkan/VulkanContext.hpp>
#include <Engine/Renderer/Vulkan/SwapChain.hpp>
#include <Engine/Renderer/Graphics/Renderer.hpp>
#include <Engine/Renderer/Vulkan/RenderTexture.hpp>
#include <Engine/Platform/Window.hpp>
#include <Engine/Debug/Log.hpp>

#include <imgui.h>
#include <ImGuizmo.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <GLFW/glfw3.h>


namespace Antelope
{
    ImGuiContext* UIContext::GetContext() const { return ImGui::GetCurrentContext(); }

    UIContext::UIContext(std::shared_ptr<VulkanContext> context, std::shared_ptr<SwapChain> swapChain, std::shared_ptr<Renderer> renderer, Window& window)
        : m_Context(context), m_SwapChain(swapChain), m_Renderer(renderer), m_Window(window)
    {
        InitImGui();
        AE_ENGINE_INFO("UIContext (ImGui) initialized.");
    }

    UIContext::~UIContext()
    {
        if (m_Context && m_Context->GetDevice() != VK_NULL_HANDLE)
        {
            vkDeviceWaitIdle(m_Context->GetDevice());
        }

        if (m_SceneTextureDescriptorSet)
        {
            ImGui_ImplVulkan_RemoveTexture(m_SceneTextureDescriptorSet);
        }

        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        if (m_ImGuiPool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(m_Context->GetDevice(), m_ImGuiPool, nullptr);
        }
    }

    void UIContext::BeginFrame()
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame(); 
    }

    void UIContext::EndFrame()
    {
        ImGui::Render();
    }

    void UIContext::RecordCommands(VkCommandBuffer cmdBuffer, uint32_t imageIndex)
    {
        VkRenderPassBeginInfo renderPassInfo {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = m_SwapChain->GetRenderPass();
        renderPassInfo.framebuffer = m_SwapChain->GetFramebuffers()[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = m_SwapChain->GetExtent();

        std::array<VkClearValue, 2> clearValues {};
        clearValues[0].color = {{0.1f, 0.1f, 0.1f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};
        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(cmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuffer);
        vkCmdEndRenderPass(cmdBuffer);
    }

    void UIContext::RenderViewports()
    {
        ImGuiIO& io { ImGui::GetIO() };
        
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
    }

    void UIContext::UpdateSceneTextureID()
    {
        auto renderTex { m_Renderer->GetFinalLDRTexture() };

        VkDescriptorImageInfo imageInfo {};
        imageInfo.sampler = renderTex->GetSampler();
        imageInfo.imageView = renderTex->GetResolveImageView();
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet write {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = m_SceneTextureDescriptorSet;
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(m_Context->GetDevice(), 1, &write, 0, nullptr);
    }

    void UIContext::ApplyTheme(const EditorThemeProps& props)
    {
        auto& style { ImGui::GetStyle() };
        auto& colors { style.Colors };

        style.WindowRounding = props.WindowRounding;
        style.FrameRounding = props.FrameRounding;
        style.TabRounding = props.TabRounding;
        style.WindowBorderSize = props.BorderSize;
        style.FrameBorderSize = props.FrameBorderSize;

        colors[ImGuiCol_WindowBg] = props.WindowBg;
        colors[ImGuiCol_ChildBg] = props.ChildBg;
        colors[ImGuiCol_PopupBg] = props.PopupBg;

        colors[ImGuiCol_Header] = props.Header;
        colors[ImGuiCol_HeaderHovered] = props.HeaderHovered;
        colors[ImGuiCol_HeaderActive] = props.HeaderActive;

        colors[ImGuiCol_Tab] = props.Tab;
        colors[ImGuiCol_TabHovered] = props.TabHovered;
        colors[ImGuiCol_TabActive] = props.TabActive;
        colors[ImGuiCol_TabUnfocused] = props.TabUnfocused;
        colors[ImGuiCol_TabUnfocusedActive] = props.TabUnfocusedActive;

        colors[ImGuiCol_TitleBg] = props.TitleBg;
        colors[ImGuiCol_TitleBgActive] = props.TitleBgActive;
        colors[ImGuiCol_TitleBgCollapsed] = props.TitleBgCollapsed;

        colors[ImGuiCol_FrameBg] = props.FrameBg;
        colors[ImGuiCol_FrameBgHovered] = props.FrameBgHovered;
        colors[ImGuiCol_FrameBgActive] = props.FrameBgActive;

        colors[ImGuiCol_Button] = props.Button;
        colors[ImGuiCol_ButtonHovered] = props.ButtonHovered;
        colors[ImGuiCol_ButtonActive] = props.ButtonActive;

        colors[ImGuiCol_Border] = props.Border;
        colors[ImGuiCol_BorderShadow] = ImVec4{ 0.00f, 0.00f, 0.00f, 0.00f };
        colors[ImGuiCol_Separator] = props.Separator;
        colors[ImGuiCol_SeparatorHovered] = props.SeparatorHovered;
        colors[ImGuiCol_SeparatorActive] = props.SeparatorActive;

        colors[ImGuiCol_DockingPreview] = props.DockingPreview;
        colors[ImGuiCol_DockingEmptyBg] = props.DockingEmptyBg;

        colors[ImGuiCol_Text] = props.Text;
        colors[ImGuiCol_TextDisabled] = props.TextDisabled;
    }

    void UIContext::InitImGui()
    {
        uint32_t imageCount { static_cast<uint32_t>(m_SwapChain->GetFramebuffers().size()) };

        VkDescriptorPoolSize pool_sizes[]
        {
            { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
        };

        VkDescriptorPoolCreateInfo pool_info {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 1000;
        pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
        pool_info.pPoolSizes = pool_sizes;

        vkCreateDescriptorPool(m_Context->GetDevice(), &pool_info, nullptr, &m_ImGuiPool);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io { ImGui::GetIO() };
        
        ImFontConfig fontConfig;
        fontConfig.OversampleH = 3;
        fontConfig.OversampleV = 3;
        io.Fonts->AddFontFromFileTTF("Assets/Fonts/FiraMono-Regular.ttf", 24.0f, &fontConfig);
        io.Fonts->AddFontFromFileTTF("Assets/Fonts/FiraMono-Bold.ttf", 24.0f, &fontConfig);
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        io.ConfigWindowsMoveFromTitleBarOnly = true;

        ApplyTheme(ThemeProvider::GetDarkTheme());

        ImGui_ImplGlfw_InitForVulkan(m_Window.GetNativeWindow(), true);

        ImGui_ImplVulkan_InitInfo init_info {};
        init_info.ApiVersion = VK_API_VERSION_1_3;
        init_info.Instance = m_Context->GetInstance();
        init_info.PhysicalDevice = m_Context->GetPhysicalDevice();
        init_info.Device = m_Context->GetDevice();
        init_info.QueueFamily = m_Context->FindQueueFamilies(m_Context->GetPhysicalDevice()).GraphicsFamily.value();
        init_info.Queue = m_Context->GetGraphicsQueue();
        init_info.PipelineCache = VK_NULL_HANDLE;
        init_info.DescriptorPool = m_ImGuiPool;
        init_info.MinImageCount = 2;
        init_info.ImageCount = imageCount;
        init_info.Allocator = nullptr;
        init_info.CheckVkResultFn = nullptr;
        init_info.PipelineInfoMain.RenderPass = m_SwapChain->GetRenderPass();
        init_info.PipelineInfoMain.MSAASamples = m_Context->GetMsaaSamples();
        init_info.PipelineInfoMain.Subpass = 0;

        ImGui_ImplVulkan_Init(&init_info);

        auto renderTex { m_Renderer->GetFinalLDRTexture() };
        m_SceneTextureDescriptorSet = ImGui_ImplVulkan_AddTexture(
            renderTex->GetSampler(),
            renderTex->GetResolveImageView(),
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );
        m_SceneTexture = (void*)m_SceneTextureDescriptorSet;
    }
}
#endif