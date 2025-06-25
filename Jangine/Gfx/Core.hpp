#pragma once

#include "../Shared.hpp"
#include "Shared.hpp"
#include "../Color.hpp"
#include "ShaderProgram.hpp"
#include "../Core.hpp"

#include <array>
#include <vector>
#include <span>
#include <string>
#include <unordered_map>
#include <istream>
#include <cstdint>
#include <stdexcept>

namespace Jangine::Gfx
{
    class Presenter;
    class Core2D;
    class Core3D;

    struct Pipeline
    {
        VkPipeline vulkanHandle;
        VkPipelineLayout vulkanLayout;
        VkDescriptorSetLayout vulkanDescriptorSetLayout;
    };

    struct PipelineParameters
    {
        struct VertexInputBinding
        {
            uint32_t binding;
            uint32_t stride;
            VertexInputRate inputRate;
        };

        struct VertexInputAttribute
        {
            uint32_t location;
            uint32_t binding;
            Format format;
            uint32_t offset;
        };

        struct AttachmentBlend
        {
            uint32_t blendEnable;
            BlendFactor srcColorBlendFactor;
            BlendFactor dstColorBlendFactor;
            BlendOp colorBlendOp;
            BlendFactor srcAlphaBlendFactor;
            BlendFactor dstAlphaBlendFactor;
            BlendOp alphaBlendOp;
            ColorComponent colorWriteMask;
        };

        struct Attachment
        {
            Format format;
            AttachmentBlend blend;
        };

        struct PushConstantRange
        {
            ShaderStage stageFlags;
            uint32_t offset;
            uint32_t size;
        };

        struct DescriptorBinding
        {
            uint32_t binding;
            DescriptorType descriptorType;
            uint32_t descriptorCount;
            ShaderStage stages;
            void_t *immutableSamplers;
        };

        const ShaderProgram *shaderProgram;

        PrimitiveTopology topology;
        FrontFace frontFace;
        CullMode cullMode;
        bool_t depthTestEnabled;
        bool_t depthWriteEnabled;
        CompareOp depthCompareOp;

        std::vector<VertexInputBinding> vertexInputBindings;
        std::vector<VertexInputAttribute> vertexInputAttributes;

        std::vector<Attachment> attachments;

        std::vector<PushConstantRange> pushConstantRanges;
        std::vector<DescriptorBinding> descriptorLayout;
    };

    struct PhysicalDevice
    {
        std::string name;
        Version vulkan;
        VkPhysicalDevice vulkanHandle;
        Version driver;
        PhysicalDeviceType type;
        Guid id;

        std::string ToString() const
        {
            return std::format("{} [Vulkan: {}] [Driver: {}]", name, vulkan.ToString(), driver.ToString());
        }
    };

    struct ApiParameters
    {
        bool_t enableDebugging = false;

        Version appVersion;
        Version appEngineVersion;
        Version requiredApiVersion;

        std::string appName;
        std::string appEngineName;
    };

    struct Parameters
    {
        PhysicalDevice physicalDevice;
    };

    class Core
    {
        struct ApiCapabilities
        {
            bool_t debugging;
            bool_t timestamps;
        };

    public:
        Core(const ApiParameters &params);
        ~Core();

        Core(const Core &) = delete;
        Core &operator=(const Core &) = delete;
        Core(Core &&) = delete;
        Core &operator=(Core &&) = delete;

        std::vector<PhysicalDevice> GetPhysicalDevices() const;

        PhysicalDevice SelectOptimalPhysicalDevice(std::span<const PhysicalDevice> physicalDevices) const
        {
            ThrowInvalidOperationIf(physicalDevices.empty());

            PhysicalDevice optimalDevice = physicalDevices[0];

            for (const auto &d : physicalDevices)
            {
                if (d.type == PhysicalDeviceType::Discrete)
                {
                    optimalDevice = d;
                    break;
                }
                if (d.type == PhysicalDeviceType::Integrated)
                {
                    optimalDevice = d;
                }
            }

            logger.Debug(optimalDevice.ToString(), __func__);
            return optimalDevice;
        }

        void Create(const Parameters &params);
        void CreatePresenter(const Surface &surface);
        void_t *GetSurfaceCreationHandle() const
        {
            return reinterpret_cast<void_t *>(vulkanInstance);
        }

        void InitializeRendering(const DisplayParameters &displayParameters);

        const DisplayParameters &GetDisplayParameters()
        {
            std::lock_guard<SpinLock> lock(displayParametersLock);
            return currentDisplayParameters;
        }
        void SetDisplayParameters(const DisplayParameters &displayParameters)
        {
            std::lock_guard<SpinLock> lock(displayParametersLock);
            this->wantedDisplayParameters = displayParameters;

            logger.Debug(displayParameters.ToString(), __func__);
        }

        void Render(double_t absoluteTime, float_t deltaTime);

        const ShaderProgram *GetShaderProgram(const std::string &name) const
        {
            auto it = shaderProgramCache.find(name);
            if (it != shaderProgramCache.end())
            {
                return &it->second;
            }

            return nullptr;
        }

        VkDescriptorSetLayout CreateDescriptorLayout(const std::vector<PipelineParameters::DescriptorBinding> &bindings);
        VkPipelineLayout CreatePipelineLayout(VkDescriptorSetLayout layout, const std::vector<PipelineParameters::PushConstantRange> &pushConstantRanges);
        Handle<Pipeline> CreatePipeline(const PipelineParameters &parameters);
        void DestroyPipeline(Pipeline *pipeline);
        void operator()(Pipeline *p) { DestroyPipeline(p); }

        Handle<PixelBuffer> CreatePixelBuffer(
            uint32_t width,
            uint32_t height,
            Format format,
            PixelBufferUsage usage,
            Aspect aspect = Aspect::Color,
            Samples samples = Samples::X1);

        void DestroyPixelBuffer(PixelBuffer *pixelBuffer);
        void operator()(PixelBuffer *pb) { DestroyPixelBuffer(pb); }

    private:
        void CreateInstance(const ApiParameters &params);
        void CreateDevice(const Parameters &params);
        void CreateQueryPool(uint32_t capacity);
        void CreateMemoryAllocator();

        void ResolveDeviceSampleCount();
        void ResolveDeviceDepthFormat();

        VkInstance vulkanInstance = nullptr;
        VkDevice vulkanDevice = nullptr;
        VkPhysicalDevice vulkanPhysicalDevice = nullptr;
        SpinLock vulkanQueueLock;
        VkQueue vulkanQueue = nullptr;
        uint32_t vulkanQueueFamilyIndex = 0;
        VkCommandPool vulkanCommandPool = nullptr;
        VkQueryPool vulkanQueryPool = nullptr;
        VkDebugUtilsMessengerEXT vulkanDebugMessenger = nullptr;

        VmaAllocator vulkanMemoryAllocator = nullptr;
        size_t vulkanMemoryAllocatorAllocatedBytes = 0;

        bool_t pendingScreenCapture = false;

        Presenter *presenter = nullptr;
        Core2D *core2D = nullptr;
        Core3D *core3D = nullptr;

        std::unordered_map<std::string, ShaderProgram> shaderProgramCache;

        const Logging::Logger &logger;

        ApiCapabilities capabilities;

        SpinLock displayParametersLock;
        std::optional<DisplayParameters> wantedDisplayParameters;
        DisplayParameters currentDisplayParameters = {
            .renderWidth = 1280,
            .renderHeight = 720,
        };

        Samples deviceSampleCount = Samples::X1;
        Format deviceDepthFormat = Format::Undefined;
    };
}
