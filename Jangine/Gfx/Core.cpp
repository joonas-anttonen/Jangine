#include "Core.hpp"
#include "Presenter.hpp"
#include "Core2D.hpp"

#include "../Logging/Logger.hpp"

#if !defined(JANGINE_INTELLISENSE_IGNORE_GENERATED_FILES)
#include "BuiltInShaders.hpp"
#endif

#include <format>
#include <set>
#include <fstream>

// Disable warnings for external Vulkan header
#if defined(_MSC_VER)
#pragma warning(push, 0)
#elif defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#endif

#include <vulkan/vulkan.h>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#if defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace Jangine::Gfx
{
    static void ThrowVulkanIfFailed(VkResult result, const std::string &message = "")
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

#if !defined(JANGINE_INTELLISENSE_IGNORE_GENERATED_FILES)
        std::istringstream shaderPackageStream(
            std::string(reinterpret_cast<const char *>(BuiltInShaders_data), BuiltInShaders_data_size),
            std::ios::binary);
        auto pkg = IO::ShaderPackage::Deserialize(shaderPackageStream);

        for (const auto &[name, program] : pkg)
        {
            shaderProgramCache[name] = program;
        }
#endif

        CreateInstance(parameters);
    }

    Core::~Core()
    {
        logger.Func(__func__);

        if (vulkanDevice)
        {
            vkDeviceWaitIdle(vulkanDevice);
        }

        if (core2D)
        {
            delete core2D;
            core2D = nullptr;
        }

        if (presenter)
        {
            delete presenter;
            presenter = nullptr;
        }

        vulkanPhysicalDevice = VK_NULL_HANDLE;

        if (vulkanMemoryAllocator)
        {
            vmaDestroyAllocator(vulkanMemoryAllocator);
            vulkanMemoryAllocator = VK_NULL_HANDLE;
            vulkanMemoryAllocatorAllocatedBytes = 0;
        }

        if (vulkanQueryPool)
        {
            vkDestroyQueryPool(vulkanDevice, vulkanQueryPool, nullptr);
            vulkanQueryPool = VK_NULL_HANDLE;
        }

        if (vulkanCommandPool)
        {
            vkDestroyCommandPool(vulkanDevice, vulkanCommandPool, nullptr);
            vulkanCommandPool = VK_NULL_HANDLE;
        }

        if (vulkanDevice)
        {
            vkDestroyDevice(vulkanDevice, nullptr);
            vulkanDevice = VK_NULL_HANDLE;
        }

        if (vulkanDebugMessenger)
        {
            auto destroyDebugUtilsMessenger = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(vulkanInstance, "vkDestroyDebugUtilsMessengerEXT"));
            if (destroyDebugUtilsMessenger)
            {
                destroyDebugUtilsMessenger(vulkanInstance, vulkanDebugMessenger, nullptr);
                vulkanDebugMessenger = VK_NULL_HANDLE;
            }
        }

        if (vulkanInstance)
        {
            vkDestroyInstance(vulkanInstance, nullptr);
            vulkanInstance = VK_NULL_HANDLE;
        }
    }

    void Core::Create(const Parameters &params)
    {
        logger.Func(__func__);

        CreateDevice(params);

        core2D = new Core2D(this);
        core2D->Create();
    }

    void Core::CreatePresenter(const Surface &surface)
    {
        logger.Func(__func__);

        ThrowInvalidOperationIf(presenter != nullptr, "Presenter already created.");

        presenter = new Presenter(
            vulkanInstance,
            vulkanDevice,
            vulkanPhysicalDevice,
            vulkanQueueLock,
            vulkanQueue,
            vulkanQueueFamilyIndex,
            reinterpret_cast<VkSurfaceKHR>(surface.vulkanHandle));

        DisplayParameters newDisplayParameters = currentDisplayParameters;
        newDisplayParameters.displayWidth = surface.width;
        newDisplayParameters.displayHeight = surface.height;
        newDisplayParameters.displayFormat = Format::BGRA8;
        SetDisplayParameters(newDisplayParameters);
    }

    void Core::InitializeRendering(const DisplayParameters &displayParameters)
    {
        logger.Func(__func__);

        this->currentDisplayParameters = displayParameters;

        if (presenter)
        {
            presenter->InitializeRendering(displayParameters);
        }
        if (core2D)
        {
            core2D->InitializeRendering(displayParameters);
        }
    }

    void Core::Render(double_t absoluteTime, float_t deltaTime)
    {
        (void)absoluteTime; // Avoid unused parameter warning
        (void)deltaTime;    // Avoid unused parameter warning

        if (!presenter)
        {
            logger.Error("Presenter is not initialized. Cannot render.");
            return;
        }

        {
            std::lock_guard<SpinLock> lock(displayParametersLock);

            if (wantedDisplayParameters.has_value())
            {
                InitializeRendering(wantedDisplayParameters.value());
                wantedDisplayParameters.reset();
            }
        }

        bool_t canRender = presenter->BeginFrame();
        if (!canRender)
        {
            logger.Warning("Presenter cannot render at this time. Skipping frame.");
            return;
        }

        if (core2D)
        {
            core2D->Render(*presenter);
        }

        if (!pendingScreenCapture)
        {
            presenter->EndFrame();
        }
        else
        {
            pendingScreenCapture = false;
            presenter->EndFrame();
        }
    }

    VkDescriptorSetLayout Core::CreateDescriptorLayout(const std::vector<PipelineParameters::DescriptorBinding> &bindings)
    {
        ThrowInvalidOperationIf(bindings.empty());

        std::vector<VkDescriptorSetLayoutBinding> vulkanBindings(bindings.size());
        std::transform(
            bindings.begin(),
            bindings.end(),
            vulkanBindings.begin(),
            [](const auto &binding)
            {
                ThrowInvalidOperationIf(binding.descriptorCount == 0);
                return VkDescriptorSetLayoutBinding{
                    .binding = binding.binding,
                    .descriptorType = static_cast<VkDescriptorType>(binding.descriptorType),
                    .descriptorCount = binding.descriptorCount,
                    .stageFlags = static_cast<VkShaderStageFlags>(binding.stages),
                    .pImmutableSamplers = static_cast<const VkSampler *>(binding.immutableSamplers)};
            });

        VkDescriptorSetLayoutCreateInfo layoutInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
            .bindingCount = static_cast<uint32_t>(vulkanBindings.size()),
            .pBindings = vulkanBindings.data()};

        VkDescriptorSetLayout pDescriptorSetLayout;
        ThrowVulkanIfFailed(vkCreateDescriptorSetLayout(vulkanDevice, &layoutInfo, nullptr, &pDescriptorSetLayout));
        return pDescriptorSetLayout;
    }

    VkPipelineLayout Core::CreatePipelineLayout(VkDescriptorSetLayout layout, const std::vector<PipelineParameters::PushConstantRange> &pushConstantRanges)
    {
        std::vector<VkPushConstantRange> vulkanPushConstantRanges(pushConstantRanges.size());
        std::transform(
            pushConstantRanges.begin(),
            pushConstantRanges.end(),
            vulkanPushConstantRanges.begin(),
            [](const auto &range)
            {
                ThrowInvalidOperationIfNot(range.size > 0, "Push constant range size must be greater than 0.");
                ThrowInvalidOperationIfNot(range.offset % 4 == 0, "Push constant range offset must be a multiple of 4.");
                return VkPushConstantRange{
                    .stageFlags = static_cast<VkShaderStageFlags>(range.stageFlags),
                    .offset = range.offset,
                    .size = range.size};
            });
        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &layout,
            .pushConstantRangeCount = static_cast<uint32_t>(vulkanPushConstantRanges.size()),
            .pPushConstantRanges = vulkanPushConstantRanges.data()};

        VkPipelineLayout pPipelineLayout;
        ThrowVulkanIfFailed(vkCreatePipelineLayout(vulkanDevice, &pipelineLayoutCreateInfo, nullptr, &pPipelineLayout));

        return pPipelineLayout;
    }

    Handle<Pipeline> Core::CreatePipeline(const PipelineParameters &parameters)
    {
        logger.Func(__func__);

        // 1. Descriptor Set Layout and Pipeline Layout
        VkDescriptorSetLayout descriptorSetLayout = CreateDescriptorLayout(parameters.descriptorLayout);
        VkPipelineLayout pipelineLayout = CreatePipelineLayout(descriptorSetLayout, parameters.pushConstantRanges);

        // 2. Input Assembly
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo{};
        inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssemblyStateCreateInfo.topology = static_cast<VkPrimitiveTopology>(parameters.topology);

        // 3. Rasterization
        VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo{};
        rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizationStateCreateInfo.cullMode = static_cast<VkCullModeFlags>(parameters.cullMode);
        rasterizationStateCreateInfo.frontFace = static_cast<VkFrontFace>(parameters.frontFace);
        rasterizationStateCreateInfo.lineWidth = 1.0f;

        // 4. Depth/Stencil
        VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo{};
        depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencilStateCreateInfo.depthTestEnable = parameters.depthTestEnabled ? VK_TRUE : VK_FALSE;
        depthStencilStateCreateInfo.depthWriteEnable = parameters.depthWriteEnabled ? VK_TRUE : VK_FALSE;
        depthStencilStateCreateInfo.depthCompareOp = static_cast<VkCompareOp>(parameters.depthCompareOp);
        depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
        depthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;

        // 5. Viewport/Scissor (dynamic)
        VkPipelineViewportStateCreateInfo viewportStateCreateInfo{};
        viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportStateCreateInfo.viewportCount = 1;
        viewportStateCreateInfo.scissorCount = 1;

        std::array<VkDynamicState, 2> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo{};
        dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

        // 6. Multisampling
        VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo{};
        multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        // 7. Vertex Input
        std::vector<VkVertexInputBindingDescription> vertexInputBindings(parameters.vertexInputBindings.size());
        std::transform(
            parameters.vertexInputBindings.begin(),
            parameters.vertexInputBindings.end(),
            vertexInputBindings.begin(),
            [](const auto &binding)
            {
                ThrowInvalidOperationIf(binding.stride == 0);
                return VkVertexInputBindingDescription{
                    .binding = binding.binding,
                    .stride = binding.stride,
                    .inputRate = static_cast<VkVertexInputRate>(binding.inputRate)};
            });

        std::vector<VkVertexInputAttributeDescription> vertexInputAttributes(parameters.vertexInputAttributes.size());
        std::transform(
            parameters.vertexInputAttributes.begin(),
            parameters.vertexInputAttributes.end(),
            vertexInputAttributes.begin(),
            [](const auto &attr)
            {
                ThrowInvalidOperationIf(static_cast<VkFormat>(attr.format) == VK_FORMAT_UNDEFINED);
                return VkVertexInputAttributeDescription{
                    .location = attr.location,
                    .binding = attr.binding,
                    .format = static_cast<VkFormat>(attr.format),
                    .offset = attr.offset};
            });

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
        vertexInputInfo.pVertexBindingDescriptions = vertexInputBindings.data();
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
        vertexInputInfo.pVertexAttributeDescriptions = vertexInputAttributes.data();

        // 8. Color Attachments & Blending
        std::vector<VkFormat> colorAttachmentFormats;
        std::vector<VkPipelineColorBlendAttachmentState> colorAttachmentBlends;
        for (const auto &att : parameters.attachments)
        {
            colorAttachmentFormats.push_back(static_cast<VkFormat>(att.format));
            colorAttachmentBlends.push_back({.blendEnable = att.blend.blendEnable ? VK_TRUE : VK_FALSE,
                                             .srcColorBlendFactor = static_cast<VkBlendFactor>(att.blend.srcColorBlendFactor),
                                             .dstColorBlendFactor = static_cast<VkBlendFactor>(att.blend.dstColorBlendFactor),
                                             .colorBlendOp = static_cast<VkBlendOp>(att.blend.colorBlendOp),
                                             .srcAlphaBlendFactor = static_cast<VkBlendFactor>(att.blend.srcAlphaBlendFactor),
                                             .dstAlphaBlendFactor = static_cast<VkBlendFactor>(att.blend.dstAlphaBlendFactor),
                                             .alphaBlendOp = static_cast<VkBlendOp>(att.blend.alphaBlendOp),
                                             .colorWriteMask = static_cast<VkColorComponentFlags>(att.blend.colorWriteMask)});
        }

        VkPipelineRenderingCreateInfo pipelineRendering{};
        pipelineRendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        pipelineRendering.colorAttachmentCount = static_cast<uint32_t>(colorAttachmentFormats.size());
        pipelineRendering.pColorAttachmentFormats = colorAttachmentFormats.data();
        pipelineRendering.depthAttachmentFormat = (parameters.depthTestEnabled || parameters.depthWriteEnabled)
                                                      ? static_cast<VkFormat>(deviceDepthFormat)
                                                      : VK_FORMAT_UNDEFINED;

        VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo{};
        colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlendStateCreateInfo.attachmentCount = static_cast<uint32_t>(colorAttachmentBlends.size());
        colorBlendStateCreateInfo.pAttachments = colorAttachmentBlends.data();

        // 9. Shader Stages
        std::vector<VkPipelineShaderStageCreateInfo> shaderStageCreateInfos;
        std::vector<VkShaderModule> shaderModules;
        for (const auto &stage : parameters.shaderProgram->stages)
        {
            VkShaderModuleCreateInfo shaderModuleCreateInfo{};
            shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            shaderModuleCreateInfo.codeSize = stage.bytecode.size();
            shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t *>(stage.bytecode.data());

            VkShaderModule shaderModule;
            ThrowVulkanIfFailed(vkCreateShaderModule(vulkanDevice, &shaderModuleCreateInfo, nullptr, &shaderModule));
            shaderModules.push_back(shaderModule);

            VkPipelineShaderStageCreateInfo stageCreateInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .stage = static_cast<VkShaderStageFlagBits>(stage.stage),
                .module = shaderModule,
                .pName = stage.entryPoint.c_str(),
                .pSpecializationInfo = nullptr};

            shaderStageCreateInfos.push_back(stageCreateInfo);
        }

        // 10. Pipeline Create Info
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.pNext = &pipelineRendering;
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.stageCount = static_cast<uint32_t>(shaderStageCreateInfos.size());
        pipelineInfo.pStages = shaderStageCreateInfos.data();
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
        pipelineInfo.pViewportState = &viewportStateCreateInfo;
        pipelineInfo.pRasterizationState = &rasterizationStateCreateInfo;
        pipelineInfo.pMultisampleState = &multisampleStateCreateInfo;
        pipelineInfo.pDepthStencilState = &depthStencilStateCreateInfo;
        pipelineInfo.pColorBlendState = &colorBlendStateCreateInfo;
        pipelineInfo.pDynamicState = &dynamicStateCreateInfo;

        VkPipeline pipeline;
        VkResult result = vkCreateGraphicsPipelines(vulkanDevice, nullptr, 1, &pipelineInfo, nullptr, &pipeline);

        // Clean up shader modules
        for (auto shaderModule : shaderModules)
            vkDestroyShaderModule(vulkanDevice, shaderModule, nullptr);

        ThrowVulkanIfFailed(result, "Failed to create graphics pipeline");

        return Handle<Pipeline>(new Pipeline(pipeline, pipelineLayout, descriptorSetLayout), std::ref(*this));
    }

    void Core::DestroyPipeline(Pipeline *pipeline)
    {
        if (!pipeline)
        {
            return;
        }

        logger.Func(__func__);

        vkDestroyPipeline(vulkanDevice, pipeline->vulkanHandle, nullptr);
        vkDestroyPipelineLayout(vulkanDevice, pipeline->vulkanLayout, nullptr);
        vkDestroyDescriptorSetLayout(vulkanDevice, pipeline->vulkanDescriptorSetLayout, nullptr);
    }

    std::vector<PhysicalDevice> Core::GetPhysicalDevices() const
    {
        uint32_t deviceCount = 0;
        ThrowVulkanIfFailed(vkEnumeratePhysicalDevices(vulkanInstance, &deviceCount, nullptr));

        if (deviceCount == 0)
        {
            throw Jangine::JangineException("No Vulkan physical devices found.");
        }

        std::vector<VkPhysicalDevice> vkPhysicalDevices(deviceCount);
        ThrowVulkanIfFailed(vkEnumeratePhysicalDevices(vulkanInstance, &deviceCount, vkPhysicalDevices.data()));

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
        }

        return physicalDevices;
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
            ThrowVulkanIfFailed(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &deviceExtensionCount, nullptr));

            std::vector<VkExtensionProperties> extensionProperties(deviceExtensionCount);
            ThrowVulkanIfFailed(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &deviceExtensionCount, extensionProperties.data()));

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
        ThrowVulkanIfFailed(vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device));
        this->vulkanDevice = device;
        this->vulkanPhysicalDevice = physicalDevice;

        VkQueue generalQueue;
        vkGetDeviceQueue(device, generalQueueFamilyIndex, 0, &generalQueue);
        this->vulkanQueue = generalQueue;
        this->vulkanQueueFamilyIndex = generalQueueFamilyIndex;

        VkCommandPoolCreateInfo commandPoolCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = generalQueueFamilyIndex};

        VkCommandPool commandPool;
        ThrowVulkanIfFailed(vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &commandPool));
        this->vulkanCommandPool = commandPool;

        if (capabilities.timestamps)
        {
            CreateQueryPool(8);
        }

        ResolveDeviceSampleCount();
        ResolveDeviceDepthFormat();

        CreateMemoryAllocator();
    }

    void Core::ResolveDeviceSampleCount()
    {
        ThrowInvalidOperationIf(!vulkanPhysicalDevice);

        VkPhysicalDeviceProperties physicalDeviceProperties;
        vkGetPhysicalDeviceProperties(vulkanPhysicalDevice, &physicalDeviceProperties);

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
        ThrowInvalidOperationIf(!vulkanPhysicalDevice);

        std::array<VkFormat, 2> depthFormats = {
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D16_UNORM};

        for (const auto &format : depthFormats)
        {
            VkFormatProperties formatProperties;
            vkGetPhysicalDeviceFormatProperties(vulkanPhysicalDevice, format, &formatProperties);

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
        ThrowVulkanIfFailed(vkCreateQueryPool(vulkanDevice, &createInfo, nullptr, &queryPool));
        this->vulkanQueryPool = queryPool;
    }

    void Core::CreateMemoryAllocator()
    {
        VmaDeviceMemoryCallbacks memoryCallbacks = {
            .pfnAllocate = [](VmaAllocator VMA_NOT_NULL allocator,
                              uint32_t memoryType,
                              VkDeviceMemory VMA_NOT_NULL_NON_DISPATCHABLE memory,
                              VkDeviceSize size,
                              void *VMA_NULLABLE pUserData)
            {
                (void)allocator; // Unused parameter
                (void)memoryType; // Unused parameter
                (void)memory; // Unused parameter

                auto self = reinterpret_cast<Core *>(pUserData);
                if (self)
                {
                    self->vulkanMemoryAllocatorAllocatedBytes += size;
                } },
            .pfnFree = [](VmaAllocator VMA_NOT_NULL allocator,
                          uint32_t memoryType,
                          VkDeviceMemory VMA_NOT_NULL_NON_DISPATCHABLE memory,
                          VkDeviceSize size,
                          void *VMA_NULLABLE pUserData)
            {
                (void)allocator; // Unused parameter
                (void)memoryType; // Unused parameter
                (void)memory; // Unused parameter

                auto self = reinterpret_cast<Core *>(pUserData);
                if (self)
                {
                    self->vulkanMemoryAllocatorAllocatedBytes -= size;
                } },
            .pUserData = this // Pass this as user data
        };

        VmaAllocatorCreateInfo allocatorInfo = {};
        allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_4;
        allocatorInfo.physicalDevice = vulkanPhysicalDevice;
        allocatorInfo.device = vulkanDevice;
        allocatorInfo.instance = vulkanInstance;
        allocatorInfo.pDeviceMemoryCallbacks = &memoryCallbacks;

        VmaAllocator allocator;
        ThrowVulkanIfFailed(vmaCreateAllocator(&allocatorInfo, &allocator));
        this->vulkanMemoryAllocator = allocator;
    }

    Handle<PixelBuffer> Core::CreatePixelBuffer(uint32_t width, uint32_t height, Format format, PixelBufferUsage usage, Aspect aspect, Samples samples)
    {
        logger.Func(__func__);

        VkImageCreateInfo imageCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = static_cast<VkFormat>(format),
            .extent = {width, height, 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = static_cast<VkSampleCountFlagBits>(samples),
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = static_cast<VkImageUsageFlags>(usage),
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};

        VmaAllocationCreateInfo allocationCreateInfo = {};
        allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

        VkImage image;
        VmaAllocation allocation;
        VmaAllocationInfo allocationInfo;
        ThrowVulkanIfFailed(vmaCreateImage(vulkanMemoryAllocator, &imageCreateInfo, &allocationCreateInfo, &image, &allocation, &allocationInfo));

        VkMemoryPropertyFlags memoryProperties;
        vmaGetMemoryTypeProperties(vulkanMemoryAllocator, allocationInfo.memoryType, &memoryProperties);

        VkImageViewCreateInfo imageViewCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = static_cast<VkFormat>(format),
            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY},
            .subresourceRange = {.aspectMask = static_cast<VkImageAspectFlags>(aspect), .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1}};

        VkImageView view;
        ThrowVulkanIfFailed(vkCreateImageView(vulkanDevice, &imageViewCreateInfo, nullptr, &view));

        auto pixelBuffer = Handle<PixelBuffer>(
            new PixelBuffer(width, height, format, usage, aspect, samples, image, view, allocation),
            std::ref(*this));
        return pixelBuffer;
    }

    void Core::DestroyPixelBuffer(PixelBuffer *pixelBuffer)
    {
        if (!pixelBuffer)
        {
            return;
        }

        logger.Func(__func__);

        if (pixelBuffer->vulkanImageView)
        {
            vkDestroyImageView(vulkanDevice, pixelBuffer->vulkanImageView, nullptr);
        }
        if (pixelBuffer->vulkanAllocation)
        {
            vmaDestroyImage(vulkanMemoryAllocator, pixelBuffer->vulkanImage, pixelBuffer->vulkanAllocation);
        }

        delete pixelBuffer;
    }

    void Core::CreateInstance(const ApiParameters &params)
    {
        uint32_t availableApiVersionRaw = 0;
        ThrowVulkanIfFailed(vkEnumerateInstanceVersion(&availableApiVersionRaw));

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
            ThrowVulkanIfFailed(vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr));

            std::vector<VkLayerProperties> layerProperties(instanceLayerCount);
            ThrowVulkanIfFailed(vkEnumerateInstanceLayerProperties(&instanceLayerCount, layerProperties.data()));

            for (uint32_t i = 0; i < instanceLayerCount; i++)
            {
                availableInstanceLayers.insert(layerProperties[i].layerName);
            }
        }

        // Get available instance extensions
        {
            uint32_t instanceExtensionCount = 0;
            ThrowVulkanIfFailed(vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, nullptr));

            std::vector<VkExtensionProperties> extensionProperties(instanceExtensionCount);
            ThrowVulkanIfFailed(vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, extensionProperties.data()));

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
        ThrowVulkanIfFailed(vkCreateInstance(&instanceCreateInfo, nullptr, &instance));
        this->vulkanInstance = instance;

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

                ThrowVulkanIfFailed(
                    vkCreateDebugUtilsMessengerEXT(instance, &debugCreateInfo, nullptr, &vulkanDebugMessenger),
                    "Failed to create debug utils messenger.");
            }
            else
            {
                logger.Warning("Debugging is enabled but some required functions are not available.");
                capabilities.debugging = false;
            }
        }
    }
}