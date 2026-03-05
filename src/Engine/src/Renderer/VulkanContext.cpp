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
        if(m_Device != VK_NULL_HANDLE)
        {
            vkDestroyDevice(m_Device, nullptr);
            AE_ENGINE_TRACE("Vulkan Logical Device destroyed.");
        }

        if(m_Surface != VK_NULL_HANDLE)
        {
            vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
            AE_ENGINE_TRACE("Vulkan Surface destroyed.");
        }

        if(m_EnableValidationLayers)
        {
            DestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);
        }

        if(m_Instance != VK_NULL_HANDLE)
        {
            vkDestroyInstance(m_Instance, nullptr);
            AE_ENGINE_INFO("Vulkan Instance destroyed.");
        }
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

    void VulkanContext::SetupDebugMessenger()
    {
        if(!m_EnableValidationLayers) { return; }

        VkDebugUtilsMessengerCreateInfoEXT createInfo {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT 
                                   | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT 
                                   | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT 
                               | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT 
                               | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = DebugCallback;

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

    QueueFamilyIndices VulkanContext::FindQueueFamilies(VkPhysicalDevice device)
    {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount { 0 };
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i { 0 };
        for(const auto& queueFamily : queueFamilies)
        {
            if(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                indices.GraphicsFamily = i;
            }

            VkBool32 presentSupport { false };
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_Surface, &presentSupport);
            
            if(presentSupport)
            {
                indices.PresentFamily = i;
            }

            if(indices.IsComplete()) { break; }
        
            i++;
        }
        return indices;
    }

    bool VulkanContext::IsDeviceSuitable(VkPhysicalDevice device)
    {
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);

        VkPhysicalDeviceFeatures deviceFeatures;
        vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

        QueueFamilyIndices indices { FindQueueFamilies(device) };

        return indices.IsComplete() &&
               deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
               deviceFeatures.geometryShader;
    }

    void VulkanContext::PickPhysicalDevice()
    {
        uint32_t deviceCount { 0 };
        vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);

        if(deviceCount == 0)
        {
            AE_ENGINE_CRITICAL("Failed to find GPUs with Vulkan support!");
            return;
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());

        for(const auto device : devices)
        {
            if(IsDeviceSuitable(device))
            {
                m_PhysicalDevice = device;
                break;
            }
        }

        if(m_PhysicalDevice == VK_NULL_HANDLE)
        {
            AE_ENGINE_CRITICAL("Failed to find a suitable GPU");
        }
        else
        {
            VkPhysicalDeviceProperties properties;
            vkGetPhysicalDeviceProperties(m_PhysicalDevice, &properties);
            AE_ENGINE_INFO("Physical Device Selected: {0}, {1}", properties.deviceName, properties.deviceID);
        }
    }

    void VulkanContext::CreateLogicalDevice()
    {
        QueueFamilyIndices indices { FindQueueFamilies(m_PhysicalDevice) };

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies { indices.GraphicsFamily.value() };

        float queuePriority { 0.1f };

        for(uint32_t queueFamily : uniqueQueueFamilies)
        {
            VkDeviceQueueCreateInfo queueCreateInfo {};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;

            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkPhysicalDeviceFeatures deviceFeatures {};

        VkDeviceCreateInfo createInfo {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.pEnabledFeatures = &deviceFeatures;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(m_DeviceExtensions.size());
        createInfo.ppEnabledExtensionNames = m_DeviceExtensions.data();

        if(m_EnableValidationLayers)
        {
            createInfo.enabledLayerCount = static_cast<uint32_t>(m_ValidationLayers.size());
            createInfo.ppEnabledLayerNames = m_ValidationLayers.data();
        }
        else
        {
            createInfo.enabledLayerCount = 0;
        }

        if(vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create Logical Device!");
            return;
        }
        else
        {
            AE_ENGINE_INFO("Vulkan Logical Device created successfully.");
        }

        vkGetDeviceQueue(m_Device, indices.GraphicsFamily.value(), 0, &m_GraphicsQueue);
        vkGetDeviceQueue(m_Device, indices.PresentFamily.value(), 0, &m_PresentQueue);
    }

    void VulkanContext::Init(GLFWwindow* windowHandle) 
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

            debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT 
                                    | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT 
                                    | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT 
                                | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT 
                                | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debugCreateInfo.pfnUserCallback = DebugCallback;
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
            AE_ENGINE_INFO("Vulkan Instance created successfully!");
        }

        SetupDebugMessenger();

        if(glfwCreateWindowSurface(m_Instance, windowHandle, nullptr, &m_Surface) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create Window Surface!");
            return;
        }
        else
        {
            AE_ENGINE_INFO("Vulkan Window Surface created successfully.");
        }

        PickPhysicalDevice();
        CreateLogicalDevice();
    }
} 