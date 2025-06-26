#pragma once

#include "../Shared.hpp"
#include "Shared.hpp"
#include "../Color.hpp"
#include "ShaderProgram.hpp"
#include "Presenter.hpp"
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
    class Core2D;
    class Core3D;

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
        struct SingleCommand
        {
            CommandBuffer commandBuffer;
            VkFence fence;
        };

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

        PhysicalDevice SelectOptimalDevice(std::span<const PhysicalDevice> physicalDevices) const
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

        Core2D *GetCore2D() const
        {
            return core2D;
        }

        Core3D *GetCore3D() const
        {
            return core3D;
        }

        // Write data to a memory buffer
        // This is a mapped memory write using memcpy
        // Only valid for host-visible and/or host-coherent memory
        void WriteMemoryBuffer(MemoryBuffer *memoryBuffer, std::span<const uint8_t> data);

        Handle<MemoryBuffer> CreateMemoryBuffer(uint32_t size, MemoryUsage usage, MemoryAccess access);
        void DestroyMemoryBuffer(MemoryBuffer *memoryBuffer);
        void operator()(MemoryBuffer *mb) { DestroyMemoryBuffer(mb); }

        Handle<PixelSampler> CreatePixelSampler(const PixelSamplerParameters &parameters);
        void DestroyPixelSampler(PixelSampler *pixelSampler);
        void operator()(PixelSampler *ps) { DestroyPixelSampler(ps); }

        Handle<Pipeline> CreatePipeline(const PipelineParameters &parameters);
        void DestroyPipeline(Pipeline *pipeline);
        void operator()(Pipeline *p) { DestroyPipeline(p); }

        // Write data to a pixel buffer
        // This is a staged upload using a temporary buffer
        void WritePixelBuffer(PixelBuffer *pixelBuffer, std::span<const uint8_t> data, ImageLayout srcLayout, ImageLayout dstLayout);

        Handle<PixelBuffer> CreatePixelBuffer(std::span<const uint8_t> data,
                                              uint32_t width,
                                              uint32_t height,
                                              Format format,
                                              PixelBufferUsage usage,
                                              Aspect aspect = Aspect::Color,
                                              Samples samples = Samples::X1);
        Handle<PixelBuffer> CreatePixelBuffer(uint32_t width,
                                              uint32_t height,
                                              Format format,
                                              PixelBufferUsage usage,
                                              Aspect aspect = Aspect::Color,
                                              Samples samples = Samples::X1);

        void DestroyPixelBuffer(PixelBuffer *pixelBuffer);
        void operator()(PixelBuffer *pb) { DestroyPixelBuffer(pb); }

        void SetThreadId(std::thread::id threadId)
        {
            renderingThreadId = threadId;
        }

        void FullBarrier(CommandBuffer commandBuffer);

        void PixelBufferBarrier(CommandBuffer commandBuffer, Presenter::Image &pixelBuffer, ImageLayout srcLayout, ImageLayout dstLayout);
        void PixelBufferBarrier(CommandBuffer commandBuffer, PixelBuffer *pixelBuffer, ImageLayout srcLayout, ImageLayout dstLayout);

        void ClearPixelBuffer(CommandBuffer commandBuffer, PixelBuffer *pixelBuffer, Color clearColor);

        void PushDescriptorSets(CommandBuffer commandBuffer, Pipeline *pipeline, uint32_t descriptorWriteCount, const VkWriteDescriptorSet *descriptorWrites);

    private:
        void CreateInstance(const ApiParameters &params);
        void CreateDevice(const Parameters &params);
        void CreateQueryPool(uint32_t capacity);
        void CreateMemoryAllocator();

        void ResolveDeviceSampleCount();
        void ResolveDeviceDepthFormat();

        VkDescriptorSetLayout CreateDescriptorLayout(const std::vector<PipelineParameters::DescriptorBinding> &bindings);
        VkPipelineLayout CreatePipelineLayout(VkDescriptorSetLayout layout, const std::vector<PipelineParameters::PushConstantRange> &pushConstantRanges);

        SingleCommand BeginSingleCommand();
        void SubmitSingleCommand(const SingleCommand &singleCommand);
        void EndSingleCommand(const SingleCommand &singleCommand);

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

        std::thread::id renderingThreadId;

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
