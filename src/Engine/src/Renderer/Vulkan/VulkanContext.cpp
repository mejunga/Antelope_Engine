#define VMA_IMPLEMENTATION

#include <Engine/Renderer/Vulkan/VulkanContext.hpp>
#include <Engine/Debug/Log.hpp>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>
#include <cstring>


namespace Antelope
{
    VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, 
                                          const VkDebugUtilsMessengerCreateInfoEXT *pMessengerInfo, 
                                          const VkAllocationCallbacks *pAllocator,
                                          VkDebugUtilsMessengerEXT *pMessenger)
    {
        auto func { (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT") };

        if (func != nullptr)
        {
            return(func(instance, pMessengerInfo, pAllocator, pMessenger));
        }
        else
        {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }

    void DestroyDebugUtilsMessengerEXT(VkInstance instance,
                                       VkDebugUtilsMessengerEXT messenger,
                                       const VkAllocationCallbacks *pAllocator)
    {
        auto func { (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT") };

        if (func != nullptr)
        {
            func(instance, messenger, pAllocator);
        }
    }

    VulkanContext::VulkanContext(GLFWwindow* windowHandle) : m_WindowHandle(windowHandle)
    {
        CreateInstance();
        AE_ENGINE_INFO("Vulkan Instance created.");
        SetupDebugMessenger();
        AE_ENGINE_TRACE("Debug Messenger connected to Engine Logger.");
        CreateSurface();
        AE_ENGINE_TRACE("Window Surface created.");
        PickPhysicalDevice();
        CreateLogicalDevice();
        AE_ENGINE_INFO("Logical Device created.");
        CreateMemoryAllocator();
        AE_ENGINE_TRACE("VMA Allocator created.");
    }

    VulkanContext::~VulkanContext() 
    {
        if (m_Device != VK_NULL_HANDLE) 
        {
            vkDeviceWaitIdle(m_Device);
        }

        if (m_Allocator != VK_NULL_HANDLE) 
        {
            vmaDestroyAllocator(m_Allocator);
            AE_ENGINE_TRACE("VMA Allocator destroyed.");
        }

        if (m_Device != VK_NULL_HANDLE)
        {
            vkDestroyDevice(m_Device, nullptr);
            AE_ENGINE_TRACE("Vulkan Logical Device destroyed.");
        }

        if (m_Surface != VK_NULL_HANDLE)
        {
            vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
            AE_ENGINE_TRACE("Vulkan Surface destroyed.");
        }

        if (m_EnableValidationLayers && m_DebugMessenger != VK_NULL_HANDLE)
        {
            DestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);
            AE_ENGINE_TRACE("Vulkan Debug Messenger destroyed.");
        }

        if (m_Instance != VK_NULL_HANDLE)
        {
            vkDestroyInstance(m_Instance, nullptr);
            AE_ENGINE_TRACE("Vulkan Instance destroyed.");
        }
    }

    SwapchainSupportDetails VulkanContext::QuerySwapChainSupport(VkPhysicalDevice device)
    {
        SwapchainSupportDetails details;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_Surface, &details.Capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount, nullptr);

        if (formatCount != 0)
        {
            details.Formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount, details.Formats.data());
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface, &presentModeCount, nullptr);

        if (presentModeCount != 0)
        {
            details.PresentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface, &presentModeCount, details.PresentModes.data());
        }

        return details;
    }

    QueueFamilyIndices VulkanContext::FindQueueFamilies(VkPhysicalDevice device)
    {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount { 0 };
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i { 0 };

        for (const auto& queueFamily : queueFamilies)
        {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                indices.GraphicsFamily = i;
            }

            VkBool32 presentSupport { false };
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_Surface, &presentSupport);

            if (presentSupport)
            {
                indices.PresentFamily = i;
            }

            if ((queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT) && !(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT))
            {
                indices.TransferFamily = i;
            }

            i++;
        }

        if (!indices.TransferFamily.has_value() && indices.GraphicsFamily.has_value())
        {
            indices.TransferFamily = indices.GraphicsFamily.value();
        }

        return indices;
    }

    VkFormat VulkanContext::FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) 
    {
        for (VkFormat format : candidates) 
        {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(m_PhysicalDevice, format, &props);

            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) { return format; }
            else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) { return format; }
        }
        
        AE_ENGINE_CRITICAL("Failed to find supported format!");
        throw std::runtime_error("Failed to find supported format");
    }

    bool VulkanContext::CheckValidationLayerSupport()
    {
        uint32_t layerCount { 0 };
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
    
        for (const char* layerName : m_ValidationLayers)
        {
            bool layerFound { false };

            for (const auto& layerProperties : availableLayers)
            {
                if (strcmp(layerName, layerProperties.layerName) == 0) 
                {
                    layerFound = true;
                    break;
                }
            }

            if (!layerFound)
            {
                return false;
            }
        }

        return true;
    }

    bool VulkanContext::CheckDeviceExtensionSupport(VkPhysicalDevice device)
    {
        uint32_t extensionCount { 0 };
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        for (const char* requiredExt : m_DeviceExtensions)
        {
            bool found = false;

            for (const auto& availableExt : availableExtensions)
            {
                if (strcmp(requiredExt, availableExt.extensionName) == 0)
                {
                    found = true;
                    break;
                }
            }
            
            if (!found) { return false; }
        }

        return true;
    }

    bool VulkanContext::IsDeviceSuitable(VkPhysicalDevice device)
    {
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);

        VkPhysicalDeviceFeatures deviceFeatures;
        vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

        QueueFamilyIndices indices { FindQueueFamilies(device) };

        bool extensionsSupported { CheckDeviceExtensionSupport(device) };
        bool swapChainAdequate { false };

        if (extensionsSupported)
        {
            SwapchainSupportDetails swapChainSupport { QuerySwapChainSupport(device) };
            swapChainAdequate = !swapChainSupport.Formats.empty() && !swapChainSupport.PresentModes.empty();
        }
        
        return indices.IsComplete() &&
               extensionsSupported &&
               swapChainAdequate &&
               deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
               deviceFeatures.geometryShader &&
               deviceFeatures.multiDrawIndirect;
    }

    VKAPI_ATTR VkBool32 VKAPI_CALL VulkanContext::DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
        void *pUserData
    )
    {
        if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        {
            AE_ENGINE_ERROR("VULKAN VALIDATION: {0}", pCallbackData->pMessage);
        }
        else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        {
            AE_ENGINE_WARN("VULKAN VALIDATION: {0}", pCallbackData->pMessage);
        }
        else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
        {
            AE_ENGINE_TRACE("VULKAN VALIDATION: {0}", pCallbackData->pMessage);
        }

        return VK_FALSE;
    }

    VkSampleCountFlagBits VulkanContext::GetMaxUsableSampleCount() 
    {
        VkPhysicalDeviceProperties physicalDeviceProperties;
        vkGetPhysicalDeviceProperties(m_PhysicalDevice, &physicalDeviceProperties);

        VkSampleCountFlags counts { physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts };

        if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
        if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
        if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
        if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
        if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
        if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

        return VK_SAMPLE_COUNT_1_BIT;
    }

    void VulkanContext::CreateInstance()
    {
        if (m_EnableValidationLayers && !CheckValidationLayerSupport())
        {
            AE_ENGINE_WARN("Validation layers requested, but not available! Proceeding without validation layers.");
            m_EnableValidationLayers = false;
        }

        VkApplicationInfo appInfo {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Antelope Editor";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "Antelope Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_3;

        uint32_t glfwExtensionCount { 0 };
        const char **glfwExtensions { glfwGetRequiredInstanceExtensions(&glfwExtensionCount) };
        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (m_EnableValidationLayers)
        {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
        
        VkInstanceCreateInfo instanceInfo {};
        instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceInfo.pApplicationInfo = &appInfo;
        instanceInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        instanceInfo.ppEnabledExtensionNames = extensions.data();

        VkDebugUtilsMessengerCreateInfoEXT messengerInfo {};

        if (m_EnableValidationLayers)
        {
            instanceInfo.enabledLayerCount = static_cast<uint32_t>(m_ValidationLayers.size());
            instanceInfo.ppEnabledLayerNames = m_ValidationLayers.data();
            AE_ENGINE_TRACE("Vulkan Validation Layers enabled.");

            messengerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            messengerInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT 
                                          | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT 
                                          | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            messengerInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT 
                                      | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT 
                                      | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            messengerInfo.pfnUserCallback = DebugCallback;
            instanceInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &messengerInfo;
        }
        else
        {
            instanceInfo.enabledLayerCount = 0;
            instanceInfo.pNext = nullptr;
            AE_ENGINE_INFO("Vulkan Validation Layers disabled (Release Mode).");
        }

        VkResult result { vkCreateInstance(&instanceInfo, nullptr, &m_Instance) };

        if (result != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create Vulkan Instance! Error Code: {0}", (int)result);
            throw std::runtime_error("Failed to create Vulkan Instance");
        }
    }

    void VulkanContext::SetupDebugMessenger()
    {
        if (!m_EnableValidationLayers) { return; }

        VkDebugUtilsMessengerCreateInfoEXT messengerInfo {};
        messengerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        messengerInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT 
                                      | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT 
                                      | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        messengerInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT 
                                  | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT 
                                  | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        messengerInfo.pfnUserCallback = DebugCallback;

        if (CreateDebugUtilsMessengerEXT(m_Instance, &messengerInfo, nullptr, &m_DebugMessenger) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to setup debug messenger!");
            throw std::runtime_error("Failed to setup debug messenger");
        }
    }

    void VulkanContext::CreateSurface()
    {
        if (glfwCreateWindowSurface(m_Instance, m_WindowHandle, nullptr, &m_Surface) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create Window Surface!");
            throw std::runtime_error("Failed to create Window Surface");
        }
    }

    void VulkanContext::PickPhysicalDevice()
    {
        uint32_t deviceCount { 0 };
        vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);

        if (deviceCount == 0)
        {
            AE_ENGINE_CRITICAL("Failed to find GPUs with Vulkan support!");
            throw std::runtime_error("Failed to find GPUs with Vulkan support");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());

        for (const auto device : devices)
        {
            if (IsDeviceSuitable(device))
            {
                m_PhysicalDevice = device;
                break;
            }
        }

        if (m_PhysicalDevice == VK_NULL_HANDLE)
        {
            AE_ENGINE_CRITICAL("Failed to find a suitable GPU!");
            throw std::runtime_error("Failed to find a suitable GPU");
        }
        else
        {
            VkPhysicalDeviceProperties properties;
            vkGetPhysicalDeviceProperties(m_PhysicalDevice, &properties);
            AE_ENGINE_INFO("Physical Device Selected: {0} (ID: {1})", properties.deviceName, properties.deviceID);

            m_MsaaSamples = GetMaxUsableSampleCount();
            AE_ENGINE_INFO("Max usable MSAA samples: {0}", (int)m_MsaaSamples);
        }
    }

    void VulkanContext::CreateLogicalDevice()
    {
        QueueFamilyIndices indices { FindQueueFamilies(m_PhysicalDevice) };

        std::vector<VkDeviceQueueCreateInfo> deviceQueueInfos;
        std::set<uint32_t> uniqueQueueFamilies { indices.GraphicsFamily.value(), indices.PresentFamily.value(), indices.TransferFamily.value() };

        float queuePriority { 0.1f };

        for (uint32_t queueFamily : uniqueQueueFamilies)
        {
            VkDeviceQueueCreateInfo deviceQueueInfo {};
            deviceQueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            deviceQueueInfo.queueFamilyIndex = queueFamily;
            deviceQueueInfo.queueCount = 1;
            deviceQueueInfo.pQueuePriorities = &queuePriority;

            deviceQueueInfos.push_back(deviceQueueInfo);
        }

        VkPhysicalDeviceVulkan12Features vulkan12Features {};
        vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        vulkan12Features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
        vulkan12Features.descriptorBindingPartiallyBound = VK_TRUE;
        vulkan12Features.descriptorBindingVariableDescriptorCount = VK_TRUE;
        vulkan12Features.runtimeDescriptorArray = VK_TRUE;
        vulkan12Features.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;

        VkPhysicalDeviceFeatures2 deviceFeatures2 {};
        deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        deviceFeatures2.pNext = &vulkan12Features;
        deviceFeatures2.features.multiDrawIndirect = VK_TRUE;
        deviceFeatures2.features.geometryShader = VK_TRUE;
        deviceFeatures2.features.samplerAnisotropy = VK_TRUE;

        VkDeviceCreateInfo deviceInfo {};
        deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        
        deviceInfo.pNext = &deviceFeatures2; 
        deviceInfo.pEnabledFeatures = nullptr;

        deviceInfo.queueCreateInfoCount = static_cast<uint32_t>(deviceQueueInfos.size());
        deviceInfo.pQueueCreateInfos = deviceQueueInfos.data();
        deviceInfo.enabledExtensionCount = static_cast<uint32_t>(m_DeviceExtensions.size());
        deviceInfo.ppEnabledExtensionNames = m_DeviceExtensions.data();

        if (m_EnableValidationLayers)
        {
            deviceInfo.enabledLayerCount = static_cast<uint32_t>(m_ValidationLayers.size());
            deviceInfo.ppEnabledLayerNames = m_ValidationLayers.data();
        }
        else
        {
            deviceInfo.enabledLayerCount = 0;
        }

        if (vkCreateDevice(m_PhysicalDevice, &deviceInfo, nullptr, &m_Device) != VK_SUCCESS)
        {
            AE_ENGINE_CRITICAL("Failed to create Logical Device!");
            throw std::runtime_error("Failed to create Logical Device");
        }

        vkGetDeviceQueue(m_Device, indices.GraphicsFamily.value(), 0, &m_GraphicsQueue);
        vkGetDeviceQueue(m_Device, indices.PresentFamily.value(), 0, &m_PresentQueue);
        vkGetDeviceQueue(m_Device, indices.TransferFamily.value(), 0, &m_TransferQueue);
    }

    void VulkanContext::CreateMemoryAllocator()
    {
        VmaAllocatorCreateInfo allocatorInfo {};
        allocatorInfo.physicalDevice = m_PhysicalDevice;
        allocatorInfo.device = m_Device;
        allocatorInfo.instance = m_Instance;
        allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;

        if (vmaCreateAllocator(&allocatorInfo, &m_Allocator) != VK_SUCCESS) 
        {
            AE_ENGINE_CRITICAL("Failed to create VMA Allocator!");
            throw std::runtime_error("Failed to create VMA Allocator");
        } 
    }
}