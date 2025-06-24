#include "Core.hpp"

#include "../Logging/Logger.hpp"

#include <format>
#include <set>
#include <vulkan/vulkan.h>

namespace Jangine::Gfx
{
    static void ThrowVulkanIfFailed(VkResult result, const std::string &message)
    {
        if (result != VK_SUCCESS)
        {
            throw Jangine::JangineException(message + " - Error code: " + std::to_string(result));
        }
    }

    static Version MakeVersion(int32_t vulkanVersion)
    {
        return Jangine::Version(
            VK_API_VERSION_MAJOR(vulkanVersion),
            VK_API_VERSION_MINOR(vulkanVersion),
            VK_API_VERSION_PATCH(vulkanVersion));
    }

    Core::Core(const ApiParameters &parameters)
        : logger(Jangine::Core::GetLogger("Gfx::Core"))
    {
        logger.Func(__func__);

        CreateInstance(parameters);
    }

    Core::~Core()
    {
        logger.Func(__func__);

        if (device)
        {
            vkDeviceWaitIdle(device);
        }

        physicalDevice = VK_NULL_HANDLE;

        if (queryPool)
        {
            vkDestroyQueryPool(device, queryPool, nullptr);
            queryPool = VK_NULL_HANDLE;
        }

        if (commandPool)
        {
            vkDestroyCommandPool(device, commandPool, nullptr);
            commandPool = VK_NULL_HANDLE;
        }

        if (device)
        {
            vkDestroyDevice(device, nullptr);
            device = VK_NULL_HANDLE;
        }

        if (debugMessenger)
        {
            auto destroyDebugUtilsMessenger = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
            if (destroyDebugUtilsMessenger)
            {
                destroyDebugUtilsMessenger(instance, debugMessenger, nullptr);
                debugMessenger = VK_NULL_HANDLE;
            }
        }

        if (instance)
        {
            vkDestroyInstance(instance, nullptr);
            instance = VK_NULL_HANDLE;
        }
    }

    std::vector<PhysicalDevice> Core::GetPhysicalDevices() const
    {
        uint32_t deviceCount = 0;
        ThrowVulkanIfFailed(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr), "vkEnumeratePhysicalDevices");

        if (deviceCount == 0)
        {
            throw Jangine::JangineException("No Vulkan physical devices found.");
        }

        std::vector<VkPhysicalDevice> vkPhysicalDevices(deviceCount);
        ThrowVulkanIfFailed(vkEnumeratePhysicalDevices(instance, &deviceCount, vkPhysicalDevices.data()), "vkEnumeratePhysicalDevices");

        std::vector<PhysicalDevice> physicalDevices;
        physicalDevices.reserve(deviceCount);

        for (const auto &vkPhysicalDevice : vkPhysicalDevices)
        {
            VkPhysicalDeviceProperties deviceProperties;
            vkGetPhysicalDeviceProperties(vkPhysicalDevice, &deviceProperties);

            PhysicalDevice device{
                .name = deviceProperties.deviceName,
                .vulkan = MakeVersion(deviceProperties.apiVersion),
                .vulkanHandle = vkPhysicalDevice,
                .driver = MakeVersion(deviceProperties.driverVersion),
                .type = static_cast<PhysicalDeviceType>(deviceProperties.deviceType),
                .id = {
                    .bytes = {
                        deviceProperties.pipelineCacheUUID[0], deviceProperties.pipelineCacheUUID[1], deviceProperties.pipelineCacheUUID[2], deviceProperties.pipelineCacheUUID[3],
                        deviceProperties.pipelineCacheUUID[4], deviceProperties.pipelineCacheUUID[5], deviceProperties.pipelineCacheUUID[6], deviceProperties.pipelineCacheUUID[7],
                        deviceProperties.pipelineCacheUUID[8], deviceProperties.pipelineCacheUUID[9], deviceProperties.pipelineCacheUUID[10], deviceProperties.pipelineCacheUUID[11],
                        deviceProperties.pipelineCacheUUID[12], deviceProperties.pipelineCacheUUID[13], deviceProperties.pipelineCacheUUID[14], deviceProperties.pipelineCacheUUID[15]}}};

            physicalDevices.push_back(device);
            logger.Warning("Found physical device: " + device.ToString());
        }

        return physicalDevices;
    }

    void Core::Create(const Parameters &params)
    {
        CreateDevice(params);
    }

    void Core::CreateDevice(const Parameters &params)
    {
        VkPhysicalDevice physicalDevice = params.physicalDevice.vulkanHandle;

        VkPhysicalDeviceProperties physicalDeviceProperties;
        vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

        float_t deviceTimestampPeriodInNanoseconds = physicalDeviceProperties.limits.timestampPeriod;
        if (deviceTimestampPeriodInNanoseconds > 0)
        {
            capabilities.timestamps = true;
        }

        std::set<std::string> availableDeviceExtensions;

        // Get available device extensions
        {
            uint32_t deviceExtensionCount = 0;
            ThrowVulkanIfFailed(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &deviceExtensionCount, nullptr), "vkEnumerateDeviceExtensionProperties");

            std::vector<VkExtensionProperties> extensionProperties(deviceExtensionCount);
            ThrowVulkanIfFailed(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &deviceExtensionCount, extensionProperties.data()), "vkEnumerateDeviceExtensionProperties");

            for (const auto &ext : extensionProperties)
            {
                availableDeviceExtensions.insert(ext.extensionName);
            }
        }

        std::vector<const char *> enabledExtensions;
        enabledExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        enabledExtensions.push_back(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
        enabledExtensions.push_back(VK_EXT_ROBUSTNESS_2_EXTENSION_NAME);
        enabledExtensions.push_back(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);

        // Check for required extensions
        for (const auto &ext : enabledExtensions)
        {
            ThrowNotSupportedIf(availableDeviceExtensions.find(ext) == availableDeviceExtensions.end(),
                                std::format("Required device extension '{}' is not supported by the physical device.", ext));
        }

        VkPhysicalDeviceSynchronization2Features synchronization2Features = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
            .pNext = nullptr,
            .synchronization2 = {}};

        VkPhysicalDeviceRobustness2FeaturesEXT robustness2Features = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT,
            .pNext = &synchronization2Features,
            .robustBufferAccess2 = {},
            .robustImageAccess2 = {},
            .nullDescriptor = {}};

        VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures subgroupExtendedTypesFeatures = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_EXTENDED_TYPES_FEATURES,
            .pNext = &robustness2Features,
            .shaderSubgroupExtendedTypes = {}};

        VkPhysicalDevice16BitStorageFeatures storage16BitFeatures = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES,
            .pNext = &subgroupExtendedTypesFeatures,
            .storageBuffer16BitAccess = {},
            .uniformAndStorageBuffer16BitAccess = {},
            .storagePushConstant16 = {},
            .storageInputOutput16 = {}};

        VkPhysicalDeviceShaderFloat16Int8Features shaderFloat16Int8Features = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES,
            .pNext = &storage16BitFeatures,
            .shaderFloat16 = {},
            .shaderInt8 = {}};

        VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
            .pNext = &shaderFloat16Int8Features,
            .dynamicRendering = {}};

        VkPhysicalDeviceFeatures2 physicalDeviceFeatures2 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .pNext = &dynamicRenderingFeatures,
            .features = {}};

        vkGetPhysicalDeviceFeatures2(physicalDevice, &physicalDeviceFeatures2);

        ThrowNotSupportedIf(physicalDeviceFeatures2.features.samplerAnisotropy == 0, "samplerAnisotropy");
        ThrowNotSupportedIf(physicalDeviceFeatures2.features.geometryShader == 0, "geometryShader");
        ThrowNotSupportedIf(physicalDeviceFeatures2.features.drawIndirectFirstInstance == 0, "drawIndirectFirstInstance");
        ThrowNotSupportedIf(physicalDeviceFeatures2.features.fragmentStoresAndAtomics == 0, "fragmentStoresAndAtomics");
        ThrowNotSupportedIf(physicalDeviceFeatures2.features.vertexPipelineStoresAndAtomics == 0, "vertexPipelineStoresAndAtomics");
        ThrowNotSupportedIf(physicalDeviceFeatures2.features.shaderStorageImageWriteWithoutFormat == 0, "shaderStorageImageWriteWithoutFormat");
        ThrowNotSupportedIf(physicalDeviceFeatures2.features.shaderImageGatherExtended == 0, "shaderImageGatherExtended");
        ThrowNotSupportedIf(physicalDeviceFeatures2.features.independentBlend == 0, "independentBlend");
        ThrowNotSupportedIf(robustness2Features.nullDescriptor == 0, "nullDescriptor");
        ThrowNotSupportedIf(synchronization2Features.synchronization2 == 0, "synchronization2");

        physicalDeviceFeatures2.features = {};
        physicalDeviceFeatures2.features.samplerAnisotropy = VK_TRUE;
        physicalDeviceFeatures2.features.geometryShader = VK_TRUE;
        physicalDeviceFeatures2.features.drawIndirectFirstInstance = VK_TRUE;
        physicalDeviceFeatures2.features.fragmentStoresAndAtomics = VK_TRUE;
        physicalDeviceFeatures2.features.vertexPipelineStoresAndAtomics = VK_TRUE;
        physicalDeviceFeatures2.features.shaderStorageImageWriteWithoutFormat = VK_TRUE;
        physicalDeviceFeatures2.features.shaderImageGatherExtended = VK_TRUE;
        physicalDeviceFeatures2.features.independentBlend = VK_TRUE;
        physicalDeviceFeatures2.features.shaderInt16 = shaderFloat16Int8Features.shaderFloat16 > 0 && storage16BitFeatures.storageBuffer16BitAccess > 0;

        robustness2Features.robustBufferAccess2 = VK_FALSE;
        robustness2Features.robustImageAccess2 = VK_FALSE;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProperties.data());

        uint32_t generalQueueFamilyIndex = UINT32_MAX;
        for (uint32_t i = 0; i < queueFamilyCount; ++i)
        {
            if (queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                generalQueueFamilyIndex = i;
                break;
            }
        }

        ThrowNotSupportedIf(generalQueueFamilyIndex == UINT32_MAX, "vkGetPhysicalDeviceQueueFamilyProperties");

        float_t queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueFamilyIndex = generalQueueFamilyIndex,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority};

        VkDeviceCreateInfo deviceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = &physicalDeviceFeatures2,
            .flags = 0,
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &queueCreateInfo,
            .enabledLayerCount = 0,
            .ppEnabledLayerNames = nullptr,
            .enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size()),
            .ppEnabledExtensionNames = enabledExtensions.data(),
            .pEnabledFeatures = nullptr};

        VkDevice device;
        ThrowVulkanIfFailed(vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device), "vkCreateDevice");
        this->device = device;
        this->physicalDevice = physicalDevice;

        VkQueue generalQueue;
        vkGetDeviceQueue(device, generalQueueFamilyIndex, 0, &generalQueue);
        this->generalQueue = generalQueue;
        this->generalQueueFamilyIndex = generalQueueFamilyIndex;

        VkCommandPoolCreateInfo commandPoolCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = generalQueueFamilyIndex};

        VkCommandPool commandPool;
        ThrowVulkanIfFailed(vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &commandPool), "vkCreateCommandPool");
        this->commandPool = commandPool;

        if (capabilities.timestamps)
        {
            CreateQueryPool(8);
        }

        ResolveDeviceSampleCount();
        ResolveDeviceDepthFormat();
    }

    void Core::ResolveDeviceSampleCount()
    {
        ThrowInvalidOperationIf(!physicalDevice, "!physicalDevice");

        VkPhysicalDeviceProperties physicalDeviceProperties;
        vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

        uint32_t sampleCounts = physicalDeviceProperties.limits.framebufferColorSampleCounts;

        if (sampleCounts & VK_SAMPLE_COUNT_64_BIT)
        {
            deviceSampleCount = Samples::X64;
        }
        else if (sampleCounts & VK_SAMPLE_COUNT_32_BIT)
        {
            deviceSampleCount = Samples::X32;
        }
        else if (sampleCounts & VK_SAMPLE_COUNT_16_BIT)
        {
            deviceSampleCount = Samples::X16;
        }
        else if (sampleCounts & VK_SAMPLE_COUNT_8_BIT)
        {
            deviceSampleCount = Samples::X8;
        }
        else if (sampleCounts & VK_SAMPLE_COUNT_4_BIT)
        {
            deviceSampleCount = Samples::X4;
        }
        else if (sampleCounts & VK_SAMPLE_COUNT_2_BIT)
        {
            deviceSampleCount = Samples::X2;
        }
        else
        {
            deviceSampleCount = Samples::X1;
        }
    }

    void Core::ResolveDeviceDepthFormat()
    {
        ThrowInvalidOperationIf(!physicalDevice, "!physicalDevice");

        std::array<VkFormat, 2> depthFormats = {
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D16_UNORM};

        for (const auto &format : depthFormats)
        {
            VkFormatProperties formatProperties;
            vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &formatProperties);

            if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
            {
                deviceDepthFormat = static_cast<Format>(format);
                return;
            }
        }

        throw Jangine::NotSupportedException("ResolveDeviceDepthFormat");
    }

    void Core::CreateQueryPool(uint32_t capacity)
    {
        VkQueryPoolCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queryType = VK_QUERY_TYPE_TIMESTAMP,
            .queryCount = capacity,
            .pipelineStatistics = 0};

        VkQueryPool queryPool;
        ThrowVulkanIfFailed(vkCreateQueryPool(device, &createInfo, nullptr, &queryPool), "vkCreateQueryPool");
        this->queryPool = queryPool;
    }

    void Core::CreateInstance(const ApiParameters &params)
    {
        uint32_t availableApiVersionRaw = 0;
        ThrowVulkanIfFailed(vkEnumerateInstanceVersion(&availableApiVersionRaw),
                            "vkEnumerateInstanceVersion");

        Version availableApiVersion = MakeVersion(availableApiVersionRaw);
        ThrowNotSupportedIf(availableApiVersion < params.requiredApiVersion,
                            std::format(
                                "Required Vulkan API version {} is not supported. Available version: {}",
                                params.requiredApiVersion.ToString(),
                                availableApiVersion.ToString()));

        std::set<std::string> availableInstanceLayers;
        std::set<std::string> availableInstanceExtensions;

        // Get available instance layers
        {
            uint32_t instanceLayerCount = 0;
            ThrowVulkanIfFailed(vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr),
                                "vkEnumerateInstanceLayerProperties");

            std::vector<VkLayerProperties> layerProperties(instanceLayerCount);
            ThrowVulkanIfFailed(vkEnumerateInstanceLayerProperties(&instanceLayerCount, layerProperties.data()),
                                "vkEnumerateInstanceLayerProperties");

            for (uint32_t i = 0; i < instanceLayerCount; i++)
            {
                availableInstanceLayers.insert(layerProperties[i].layerName);
            }
        }

        // Get available instance extensions
        {
            uint32_t instanceExtensionCount = 0;
            ThrowVulkanIfFailed(vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, nullptr),
                                "vkEnumerateInstanceExtensionProperties");

            std::vector<VkExtensionProperties> extensionProperties(instanceExtensionCount);
            ThrowVulkanIfFailed(vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, extensionProperties.data()),
                                "vkEnumerateInstanceExtensionProperties");

            for (uint32_t i = 0; i < instanceExtensionCount; i++)
            {
                availableInstanceExtensions.insert(extensionProperties[i].extensionName);
            }
        }

        std::vector<const char *> enabledLayers;
        std::vector<const char *> enabledExtensions;

#if defined(JANGINE_PLATFORM_WINDOWS)
        enabledExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
        enabledExtensions.push_back("VK_KHR_win32_surface");
#elif defined(JANGINE_PLATFORM_LINUX)
        enabledExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
        enabledExtensions.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
        enabledExtensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
        enabledExtensions.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
#else
#error "Unsupported platform for Vulkan instance creation"
#endif // Platform-specific layer and extension handling

        // Check that the enabled layers and extensions are available
        for (const auto &enabledLayer : enabledLayers)
        {
            ThrowNotSupportedIf(availableInstanceLayers.find(enabledLayer) == availableInstanceLayers.end(),
                                std::format("Required Vulkan instance layer '{}' is not available.", enabledLayer));
        }
        for (const auto &enabledExtension : enabledExtensions)
        {
            ThrowNotSupportedIf(availableInstanceExtensions.find(enabledExtension) == availableInstanceExtensions.end(),
                                std::format("Required Vulkan instance extension '{}' is not available.", enabledExtension));
        }

        if (params.enableDebugging)
        {
            auto validationAvailable = availableInstanceLayers.find("VK_LAYER_KHRONOS_validation") != availableInstanceLayers.end();
            auto debugUtilsAvailable = availableInstanceExtensions.find(VK_EXT_DEBUG_UTILS_EXTENSION_NAME) != availableInstanceExtensions.end();

            if (validationAvailable && debugUtilsAvailable)
            {
                capabilities.debugging = true;
                enabledLayers.push_back("VK_LAYER_KHRONOS_validation");
                enabledExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            }
            else
            {
                capabilities.debugging = false;
            }
        }

        VkApplicationInfo appInfo = {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = params.appName.c_str();
        appInfo.applicationVersion = VK_MAKE_API_VERSION(0, params.appVersion.major, params.appVersion.minor, params.appVersion.patch);
        appInfo.pEngineName = params.appEngineName.c_str();
        appInfo.engineVersion = VK_MAKE_API_VERSION(0, params.appEngineVersion.major, params.appEngineVersion.minor, params.appEngineVersion.patch);
        appInfo.apiVersion = availableApiVersionRaw;

        VkInstanceCreateInfo instanceCreateInfo = {};
        instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceCreateInfo.pApplicationInfo = &appInfo;
        instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(enabledLayers.size());
        instanceCreateInfo.ppEnabledLayerNames = enabledLayers.empty() ? nullptr : enabledLayers.data();
        instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size());
        instanceCreateInfo.ppEnabledExtensionNames = enabledExtensions.empty() ? nullptr : enabledExtensions.data();

        VkInstance instance;
        ThrowVulkanIfFailed(vkCreateInstance(&instanceCreateInfo, nullptr, &instance), "vkCreateInstance");
        this->instance = instance;

        if (params.enableDebugging && capabilities.debugging)
        {
            auto vkCreateDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));

            bool debugFunctionsOk = vkCreateDebugUtilsMessengerEXT;
            if (debugFunctionsOk)
            {
                static auto debugCallback = [](VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                               VkDebugUtilsMessageTypeFlagsEXT messageType,
                                               const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                               void *pUserData) -> VkBool32
                {
                    (void)messageType;
                    (void)pUserData;

                    auto logger = Jangine::Core::GetLogger("Gfx::Vulkan");
                    auto message = pCallbackData->pMessage ? pCallbackData->pMessage : "";

                    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
                        logger.Error(message);
                    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
                        logger.Warning(message);
                    else
                        logger.Debug(message);

                    return VK_FALSE;
                };

                VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
                debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
                debugCreateInfo.messageSeverity =
                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
                debugCreateInfo.messageType =
                    VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
                debugCreateInfo.pfnUserCallback = debugCallback;
                debugCreateInfo.pUserData = nullptr;

                VkDebugUtilsMessengerEXT debugMessenger{};
                ThrowVulkanIfFailed(
                    vkCreateDebugUtilsMessengerEXT(instance, &debugCreateInfo, nullptr, &debugMessenger),
                    "Failed to create debug utils messenger.");
                this->debugMessenger = debugMessenger;
            }
            else
            {
                logger.Warning("Debugging is enabled but some required functions are not available.");
                capabilities.debugging = false;
            }
        }
    }
}