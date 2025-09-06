#pragma once

#include "../Shared.hpp"
#include "Shared.hpp"
#include "../Color.hpp"
#include "ShaderProgram.hpp"
#include "Presenter.hpp"
#include "PixelBuffer.hpp"
#include "../Core.hpp"
#include "../UserInput.hpp"

namespace Jangine::Gfx
{
    class Overlay;
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
            size_t uniformBufferOffsetAlignment;
            size_t maxUniformBufferRange;
            size_t maxStorageBufferRange;
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
                if (d.type == PhysicalDeviceType::DISCRETE_GPU)
                {
                    optimalDevice = d;
                    break;
                }
                if (d.type == PhysicalDeviceType::INTEGRATED_GPU)
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

        Format GetDeviceDepthFormat() const
        {
            return deviceDepthFormat;
        }

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

        Overlay &GetCore2D() const
        {
            return *overlay;
        }

        Core3D &GetCore3D() const
        {
            return *core3D;
        }

        auto ApplyUserInput(UserInput::Event &event)
        {
            userInput.ApplyEvent(event);
        }

        const UserInput &GetUserInput() const
        {
            return userInput;
        }

        const ApiCapabilities &GetCapabilities() const { return capabilities; }

        /// @brief Write data to a memory buffer directly using memcpy
        /// @note Only valid for host-visible and/or host-coherent memory
        void WriteMemoryBuffer(MemoryBuffer *memoryBuffer, std::span<const std::byte> data, size_t offset = 0);

        /// @brief Write data to a memory buffer using a staging buffer
        void StageToMemoryBuffer(MemoryBuffer *memoryBuffer, std::span<const std::byte> data);

        Handle<MemoryBuffer> CreateMemoryBuffer(size_t size, MemoryBufferUsage usage, MemoryAccess access);
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

        /// @brief Write data to a pixel buffer using a staging buffer
        void StageToPixelBuffer(PixelBuffer *pixelBuffer, std::span<const std::byte> data, ImageLayout srcLayout, ImageLayout dstLayout);

        Handle<PixelBuffer> CreatePixelBuffer(std::span<const std::byte> data,
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

        Rectangle UnsafeGetCurrentViewport() const
        {
            return currentViewport;
        }
        void UnsafeSetViewport(Rectangle viewport)
        {
            currentViewport = viewport;
        }

        void BindBuffers(CommandBuffer commandBuffer, const MemoryBuffer *vertexBuffer, const MemoryBuffer *indexBuffer);

        void FullBarrier(CommandBuffer commandBuffer);
        void TransferBarrier(CommandBuffer commandBuffer);
        void PixelBufferBarrier(CommandBuffer commandBuffer, Presenter::Image &pixelBuffer, ImageLayout srcLayout, ImageLayout dstLayout);
        void PixelBufferBarrier(CommandBuffer commandBuffer, PixelBuffer *pixelBuffer, ImageLayout srcLayout, ImageLayout dstLayout);

        void BlitPixelBuffer(CommandBuffer commandBuffer,
                             PixelBuffer *srcBuffer,
                             PixelBuffer *dstBuffer,
                             BlitFilter filter = BlitFilter::LINEAR,
                             ImageLayout srcCurrentLayout = ImageLayout::TRANSFER_SRC_OPTIMAL,
                             ImageLayout dstCurrentLayout = ImageLayout::TRANSFER_DST_OPTIMAL);

        void FillBuffer(CommandBuffer commandBuffer, MemoryBuffer *memoryBuffer, uint32_t value, size_t offset = 0, size_t size = ~0);
        void ClearPixelBuffer(CommandBuffer commandBuffer, PixelBuffer *pixelBuffer, Color clearColor, ImageLayout currentLayout = ImageLayout::TRANSFER_DST_OPTIMAL);
        void ClearPixelBuffer(CommandBuffer commandBuffer, PixelBuffer *pixelBuffer, uint32_t c0, uint32_t c1, uint32_t c2, uint32_t c3, ImageLayout currentLayout = ImageLayout::TRANSFER_DST_OPTIMAL);

        void PushDescriptorSets(CommandBuffer commandBuffer, Pipeline *pipeline, uint32_t descriptorWriteCount, const VkWriteDescriptorSet *descriptorWrites);

        void SetDebugName(MemoryBuffer *memoryBuffer, const std::string &name);
        void SetDebugName(PixelBuffer *pixelBuffer, const std::string &name);
        void SetDebugName(Pipeline *pipeline, const std::string &name);

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
        Overlay *overlay = nullptr;
        Core3D *core3D = nullptr;

        std::unordered_map<std::string, ShaderProgram> shaderProgramCache;

        const Logging::Logger &logger;

        ApiCapabilities capabilities;

        SpinLock displayParametersLock;
        std::optional<DisplayParameters> wantedDisplayParameters;
        DisplayParameters currentDisplayParameters = {
            .renderWidth = 2560,
            .renderHeight = 1440,
            .displayWidth = 2560,
            .displayHeight = 1440,
        };
        Rectangle currentViewport = {0, 0, 2560, 1440};

        UserInput userInput;

        Samples deviceSampleCount = Samples::X1;
        Format deviceDepthFormat = Format::Undefined;
    };
}
