#include <Engine/Debug/Log.hpp>
#include <Engine/Renderer/VulkanContext.hpp>
#include <GLFW/glfw3.h>
#include <cstring>

namespace Antelope
{
    VkResult CreateDebugUtilsMessengerEXT(
        VkInstance instance, 
        const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo, 
        const VkAllocationCallbacks *pAllocator,
        VkDebugUtilsMessengerEXT *pDebugMessenger
    )
    {
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

        if(func != nullptr)
        {
            return(func(instance, pCreateInfo, pAllocator, pDebugMessenger));
        }
        else
        {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }

    void DestroyDebugUtilsMessengerEXT(
        VkInstance instance,
        VkDebugUtilsMessengerEXT debugMessenger,
        const VkAllocationCallbacks *pAllocator
    )
    {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");

        if(func != nullptr)
        {
            func(instance, debugMessenger, pAllocator);
        }
    }

    VulkanContext::VulkanContext() {}
    VulkanContext::~VulkanContext() 
    {
        if(m_EnableValidationLayers)
        {
            DestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);
        }

        vkDestroyInstance(m_Instance, nullptr);
        AE_ENGINE_INFO("Vulkan Instance destroyed.");
    }

    VKAPI_ATTR VkBool32 VKAPI_CALL VulkanContext::DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
        void *pUserData
    )
    {
        if(messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        {
            AE_ENGINE_ERROR("VULKAN VALIDATION: {0}", pCallbackData->pMessage);
        }
        else if(messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        {
            AE_ENGINE_WARN("VULKAN VALIDATION: {0}", pCallbackData->pMessage);
        }
        else if(messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
        {
            AE_ENGINE_TRACE("VULKAN VALIDATION: {0}", pCallbackData->pMessage);
        }

        return VK_FALSE;
    }

    void VulkanContext::PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
    {
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT 
                                   | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT 
                                   | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT 
                               | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT 
                               | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = DebugCallback;
    }

    void VulkanContext::SetupDebugMessenger()
    {
        if(m_EnableValidationLayers) { return; }

        VkDebugUtilsMessengerCreateInfoEXT createInfo;
        PopulateDebugMessengerCreateInfo(createInfo);

        if(CreateDebugUtilsMessengerEXT(m_Instance, &createInfo, nullptr, &m_DebugMessenger) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to setup debug messenger!");
        }
        else
        {
            AE_ENGINE_INFO("Vulkan Debug Messenger connected to Engine Logger.");
        }
    }

    bool VulkanContext::CheckValidationLayerSupport()
    {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> avaliableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, avaliableLayers.data());
    
        for(const char* layerName : m_ValidationLayers)
        {
            bool layerFound { false };

            for(const auto& layerProperties : avaliableLayers)
            {
                if(strcmp(layerName, layerProperties.layerName) == 0) 
                {
                    layerFound = true;
                    break;
                }
            }

            if(!layerFound)
            {
                return false;
            }
        }

        return true;
    }

    void VulkanContext::Init() 
    {
        if(m_EnableValidationLayers && !CheckValidationLayerSupport())
        {
            AE_ENGINE_CRITICAL("Validation layers requested, but not available!");
            return;
        }

        VkApplicationInfo appInfo {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Antelope Editor";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "Antelope Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_3;

        uint32_t glfwExtensionCount = 0;
        const char **glfwExtensions { glfwGetRequiredInstanceExtensions(&glfwExtensionCount) };
        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if(m_EnableValidationLayers)
        {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
        
        VkInstanceCreateInfo createInfo {};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo {};

        if(m_EnableValidationLayers)
        {
            createInfo.enabledLayerCount = static_cast<uint32_t>(m_ValidationLayers.size());
            createInfo.ppEnabledLayerNames = m_ValidationLayers.data();
            AE_ENGINE_INFO("Vulkan Validation Layers enabled.");

            PopulateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
        }
        else
        {
            createInfo.enabledLayerCount = 0;
            createInfo.pNext = nullptr;
            AE_ENGINE_INFO("Vulkan Validation Layers disabled (Release Mode).");
        }

        VkResult result { vkCreateInstance(&createInfo, nullptr, &m_Instance) };

        if(result != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create Vulkan Instance! Error Code: {0}", (int)result);
            return;
        }
        else
        {
            AE_ENGINE_INFO("Vulkan Instance created succesfully!");
        }

        SetupDebugMessenger();
    }
} 