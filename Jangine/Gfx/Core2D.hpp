#pragma once

#include "../Shared.hpp"
#include "Shared.hpp"
#include "Core.hpp"

#include "Presenter.hpp"
#include "CommandBuffer2D.hpp"
#include "Text/FontCollection.hpp"

namespace Jangine::Logging
{
    class Logger;
}

namespace Jangine::Gfx
{
    class Core2D
    {
    public:
        struct PushConstants
        {
            Eigen::Vector2f scale;
            uint32_t smoothing;
            uint32_t padding; // Padding to ensure 16-byte alignment
        };

        struct BlurPushConstants
        {
            float scale;
            float strength;
        };

        Core2D(Gfx::Core *gfx);
        ~Core2D();
        Core2D &operator=(const Core2D &) = delete;
        Core2D(const Core2D &) = delete;
        Core2D &operator=(Core2D &&) = delete;
        Core2D(Core2D &&from) = delete;

        void Create();
        void InitializeRendering(const DisplayParameters &wantedDisplayParameters);
        void Render(const Presenter &presenter, double_t absoluteTime, float_t deltaTime);

        // Try to acquire a command buffer from the pool
        // Returns true if a command buffer was acquired, false otherwise (render thread is busy with previous submissions)
        [[nodiscard]] bool_t TryAcquireCommandBuffer(CommandBuffer2D **outBuffer)
        {
            std::scoped_lock lock(commandBufferPoolLock);
            if (freeCommandBufferIndices.empty())
            {
                *outBuffer = nullptr;
                return false;
            }
            size_t idx = freeCommandBufferIndices.top();
            freeCommandBufferIndices.pop();
            *outBuffer = &commandBufferPool[idx];
            return true;
        }

        // Submit a command buffer for execution
        void SubmitCommandBuffer(CommandBuffer2D *buffer)
        {
            std::scoped_lock lock(commandBufferQueueLock);
            commandBufferQueue.push(buffer);
        }

        const Text::Font *GetDefaultFont() const
        {
            return defaultFont;
        }

        Text::Shaper *GetDefaultShaper() const
        {
            return defaultShaper.get();
        }

        bool_t IsReady() const
        {
            return isReady;
        }

        SharedHandle<PixelBuffer> GetAcrylicBuffer() const
        {
            return blurBuffer;
        }

    private:
        void InitializeCommandBufferPool()
        {
            commandBufferPool.resize(COMMAND_BUFFER_POOL_SIZE);
            for (size_t i = 0; i < COMMAND_BUFFER_POOL_SIZE; ++i)
            {
                commandBufferPool[i].index = static_cast<uint32_t>(i);
                freeCommandBufferIndices.push(i);
            }
        }

        void ReturnCommandBuffer(CommandBuffer2D *buffer)
        {
            buffer->Reset();

            std::scoped_lock lock(commandBufferPoolLock);
            freeCommandBufferIndices.push(buffer->index);
        }

        void RecordBatch(CommandBuffer commandBuffer, std::span<const CommandBuffer2D::Command> commands, PixelBuffer *targetBuffer);
        void PrepareFrame(const Presenter &presenter);
        void FinishFrame(const Presenter &presenter);

        static constexpr size_t COMMAND_BUFFER_POOL_SIZE = 4;

        static constexpr uint32_t MAX_VERTICES = 65536;
        static constexpr uint32_t MAX_INDICES = 65536;

        Gfx::Core *gfx = nullptr;

        DisplayParameters displayParameters;

        Handle<PixelSampler> nearestPixelSampler;
        Handle<PixelSampler> linearPixelSampler;

        Handle<MemoryBuffer> vertexBuffer;
        Handle<MemoryBuffer> indexBuffer;

        Handle<PixelBuffer> placeholderBuffer;
        Handle<PixelBuffer> backBuffer;
        Handle<Pipeline> renderPipeline;
        Handle<Pipeline> compositePipeline;

        Handle<Pipeline> blurHorizontalPipeline;
        Handle<Pipeline> blurVerticalPipeline;
        Handle<PixelSampler> blurSampler;
        Handle<PixelBuffer> blurIntermediateBuffer;
        SharedHandle<PixelBuffer> blurBuffer;
        Handle<PixelBuffer> blurNoiseBuffer;
        Handle<PixelSampler> blurNoiseSampler;

        Text::FontCollection fontCollection;
        const Text::Font *defaultFont = nullptr;
        std::unique_ptr<Text::Shaper> defaultShaper = nullptr;

        SpinLock commandBufferPoolLock;
        std::vector<CommandBuffer2D> commandBufferPool;
        std::stack<size_t> freeCommandBufferIndices;
        SpinLock commandBufferQueueLock;
        std::queue<CommandBuffer2D *> commandBufferQueue;
        CommandBuffer2D *currentCommandBuffer = nullptr;

        bool_t isReady{false};

        const Logging::Logger &logger;
    };
}