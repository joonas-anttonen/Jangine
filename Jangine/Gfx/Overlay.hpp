#pragma once

#include "../Shared.hpp"
#include "Shared.hpp"
#include "Core.hpp"

#include "Presenter.hpp"
#include "Text/FontCollection.hpp"

namespace Jangine::Logging
{
    class Logger;
}

namespace Jangine::Gfx
{
    namespace Text
    {
        class Font;
    }

    class Overlay
    {
        struct Vertex2f
        {
            Eigen::Vector2f position;
            Eigen::Vector2f uv;
            uint32_t color;
        };

    public:
        class CommandBuffer
        {
            friend class Overlay;

        public:
            CommandBuffer() = default;
            ~CommandBuffer() = default;
            CommandBuffer &operator=(const CommandBuffer &) = delete;
            CommandBuffer(const CommandBuffer &) = delete;
            CommandBuffer &operator=(CommandBuffer &&) = default;
            CommandBuffer(CommandBuffer &&from) = default;

            /// @brief Single draw command
            struct Command
            {
                explicit Command()
                    : vertexOffset(0), indexOffset(0), indexCount(0), texture(nullptr), font(nullptr), fontWidth(.5f) {}
                explicit Command(uint32_t vertexOffset, uint32_t indexOffset)
                    : vertexOffset(vertexOffset), indexOffset(indexOffset), indexCount(0), texture(SharedHandle<PixelBuffer>()), font(nullptr), fontWidth(.5f) {}
                Command &operator=(const Command &) = delete;
                Command(const Command &) = delete;
                Command &operator=(Command &&) = default;
                Command(Command &&from) = default;

                uint32_t vertexOffset;
                uint32_t indexOffset;
                uint32_t indexCount;
                SharedHandle<PixelBuffer> texture;
                const Text::Font *font;
                float_t fontWidth;
                std::optional<Rectangle> scissor;
            };

            /// @brief Batch of draw commands
            struct CommandBatch
            {
                explicit CommandBatch()
                    : firstCommandIndex(0), commandCount(0), surface(nullptr) {}
                explicit CommandBatch(uint32_t firstCommandIndex)
                    : firstCommandIndex(firstCommandIndex), commandCount(0), surface(nullptr) {}

                uint32_t firstCommandIndex;
                uint32_t commandCount;
                PixelBuffer *surface;
            };

            void PushScissor(Rectangle scissor)
            {
                scissorStack.push(scissor);
            }

            void PopScissor()
            {
                ThrowInvalidOperationIf(scissorStack.empty());
                scissorStack.pop();
            }

            /// @brief Resets the command buffer to its initial state.
            void Reset()
            {
                commands.clear();
                batches.clear();
                vertices.clear();
                indices.clear();
                temp_points.clear();
                temp_normals.clear();
                scratchVertices.clear();
                batchInProgress = false;
            }

            void BeginBatch()
            {
                ThrowInvalidOperationIf(batchInProgress);

                batchInProgress = true;
                batches.push_back(CommandBatch(static_cast<uint32_t>(commands.size())));

                auto &cmd = BeginCommand();
                (void)cmd;
            }

            void EndBatch(PixelBuffer *surface = nullptr)
            {
                ThrowInvalidOperationIfNot(batchInProgress);

                batchInProgress = false;
                batches.back().surface = surface;
            }

            [[nodiscard]] Command &BeginCommand()
            {
                CommandBatch &batch = batches.back();
                batch.commandCount++;
                commands.emplace_back(static_cast<uint32_t>(vertices.size()), static_cast<uint32_t>(indices.size()));
                return commands.back();
            }

            [[nodiscard]] Command &GetCurrentCommand()
            {
                ThrowInvalidOperationIfNot(batchInProgress);
                ThrowInvalidOperationIf(commands.empty());

                return commands.back();
            }

            void DrawText(const Text::Layout &layout, Eigen::Vector2f position, Color color)
            {
                ThrowInvalidOperationIfNot(batchInProgress);

                if (color.IsTransparent() || layout.font() == nullptr)
                {
                    return;
                }

                // New command if font or font size changed
                auto &currentCommand = GetCurrentCommand();
                if (currentCommand.font != layout.font() || currentCommand.fontWidth != layout.font_width())
                {
                    auto &newCommand = BeginCommand();
                    newCommand.font = layout.font();
                    newCommand.fontWidth = layout.font_width();
                }

                // New command if scissor changed
                if (scissorStack.size() > 0 && GetCurrentCommand().scissor != scissorStack.top())
                {
                    auto &newCommand = BeginCommand();
                    newCommand.font = layout.font();
                    newCommand.fontWidth = layout.font_width();
                    newCommand.scissor = scissorStack.top();
                }

                for (const auto &glyph : layout)
                {
                    Eigen::Vector2f a = position + glyph.position;
                    Eigen::Vector2f c = a + glyph.size;

                    PushQuadUV(a, c, glyph.uv0, glyph.uv1, color);
                }
            }

            void DrawImage(SharedHandle<PixelBuffer> image, Eigen::Vector2f targetPosition, Eigen::Vector2f targetExtent, ImageFit imageFit, Color color = Color::White)
            {
                ThrowInvalidOperationIfNot(batchInProgress);

                if (color.IsTransparent() || !image)
                {
                    return;
                }

                Command &newCommand = BeginCommand();
                newCommand.texture = image;
                if (scissorStack.size() > 0)
                {
                    newCommand.scissor = scissorStack.top();
                }

                Eigen::Vector2f imageExtent(static_cast<float_t>(image->width), static_cast<float_t>(image->height));
                Eigen::Vector2f uv0(0.0f, 0.0f);
                Eigen::Vector2f uv1(1.0f, 1.0f);

                Eigen::Vector2f finalImagePosition = targetPosition;
                Eigen::Vector2f finalImageExtent = targetExtent;

                switch (imageFit)
                {
                    // None will draw the corresponding area of the image
                case ImageFit::None:
                    finalImagePosition = targetPosition;
                    finalImageExtent = targetExtent;

                    // Shift UVs so that only the target area is drawn
                    uv0.x() = targetPosition.x() / imageExtent.x();
                    uv0.y() = targetPosition.y() / imageExtent.y();
                    uv1.x() = (targetPosition.x() + targetExtent.x()) / imageExtent.x();
                    uv1.y() = (targetPosition.y() + targetExtent.y()) / imageExtent.y();
                    break;
                case ImageFit::Stretch:
                    finalImagePosition = targetPosition;
                    finalImageExtent = imageExtent;
                    break;
                case ImageFit::Fill:
                    finalImagePosition = targetPosition;
                    finalImageExtent = targetExtent;
                    break;
                case ImageFit::FillAspect:
                {
                    bool horizontal = imageExtent.x() > imageExtent.y();
                    float scale = horizontal
                                      ? targetExtent.x() / imageExtent.x()
                                      : targetExtent.y() / imageExtent.y();
                    finalImageExtent = imageExtent * scale;
                    Eigen::Vector2f offset = (targetExtent - finalImageExtent) * 0.5f;
                    finalImagePosition = targetPosition + offset;
                    break;
                }
                case ImageFit::Center:
                {
                    Eigen::Vector2f offset = (targetExtent - imageExtent) * 0.5f;
                    finalImagePosition = targetPosition + offset;
                    finalImageExtent = imageExtent;
                    break;
                }
                default:
                    break;
                }

                PushQuadUV(finalImagePosition, finalImagePosition + finalImageExtent, uv0, uv1, color);
            }

            void DrawRectangle(Rectangle rectangle, Color color, float_t thickness = 1.0f)
            {
                DrawRectangle(rectangle.position(), rectangle.position() + rectangle.extent(), color, thickness);
            }

            void DrawRectangle(Eigen::Vector2f a, Eigen::Vector2f c, Color color, float_t thickness = 1.0f)
            {
                if (color.IsTransparent())
                {
                    return;
                }

                auto &currentCommand = GetCurrentCommand();
                if (currentCommand.texture || currentCommand.font)
                {
                    auto &command = BeginCommand();
                    if (scissorStack.size() > 0 && GetCurrentCommand().scissor != scissorStack.top())
                    {
                        command.scissor = scissorStack.top();
                    }
                }
                else if (scissorStack.size() > 0 && GetCurrentCommand().scissor != scissorStack.top())
                {
                    auto &newCommand = BeginCommand();
                    newCommand.scissor = scissorStack.top();
                }

                float_t half_thickness = thickness * 0.5f;

                // Top edge
                PushQuadUV(
                    Eigen::Vector2f(a.x() - half_thickness, a.y() - half_thickness),
                    Eigen::Vector2f(c.x() + half_thickness, a.y() + half_thickness),
                    Eigen::Vector2f(0, 0), Eigen::Vector2f(1, 1), color);

                // Bottom edge
                PushQuadUV(
                    Eigen::Vector2f(a.x() - half_thickness, c.y() - half_thickness),
                    Eigen::Vector2f(c.x() + half_thickness, c.y() + half_thickness),
                    Eigen::Vector2f(0, 0), Eigen::Vector2f(1, 1), color);

                // Left edge
                PushQuadUV(
                    Eigen::Vector2f(a.x() - half_thickness, a.y() + half_thickness),
                    Eigen::Vector2f(a.x() + half_thickness, c.y() - half_thickness),
                    Eigen::Vector2f(0, 0), Eigen::Vector2f(1, 1), color);

                // Right edge
                PushQuadUV(
                    Eigen::Vector2f(c.x() - half_thickness, a.y() + half_thickness),
                    Eigen::Vector2f(c.x() + half_thickness, c.y() - half_thickness),
                    Eigen::Vector2f(0, 0), Eigen::Vector2f(1, 1), color);
            }

            void FillRectangle(Rectangle rectangle, Color color)
            {
                if (color.IsTransparent())
                {
                    return;
                }

                auto &currentCommand = GetCurrentCommand();
                if (currentCommand.texture || currentCommand.font)
                {
                    auto &command = BeginCommand();
                    if (scissorStack.size() > 0 && GetCurrentCommand().scissor != scissorStack.top())
                    {
                        command.scissor = scissorStack.top();
                    }
                }
                else if (scissorStack.size() > 0 && GetCurrentCommand().scissor != scissorStack.top())
                {
                    auto &newCommand = BeginCommand();
                    newCommand.scissor = scissorStack.top();
                }

                PushQuadUV(rectangle.position(), rectangle.position() + rectangle.extent(),
                           Eigen::Vector2f(0, 0), Eigen::Vector2f(1, 1), color);
            }

            void FillRectangle(Eigen::Vector2f a, Eigen::Vector2f c, Color color)
            {
                if (color.IsTransparent())
                {
                    return;
                }

                auto &currentCommand = GetCurrentCommand();
                if (currentCommand.texture || currentCommand.font)
                {
                    auto &command = BeginCommand();
                    if (scissorStack.size() > 0 && GetCurrentCommand().scissor != scissorStack.top())
                    {
                        command.scissor = scissorStack.top();
                    }
                }
                else if (scissorStack.size() > 0 && GetCurrentCommand().scissor != scissorStack.top())
                {
                    auto &newCommand = BeginCommand();
                    newCommand.scissor = scissorStack.top();
                }

                PushQuadUV(a, c, Eigen::Vector2f(0, 0), Eigen::Vector2f(1, 1), color);
            }

            void PushQuadUV(Eigen::Vector2f a, Eigen::Vector2f c, Eigen::Vector2f a_uv, Eigen::Vector2f c_uv, Color color)
            {
                if (color.IsTransparent())
                {
                    return;
                }

                auto quadStartIndex = static_cast<uint16_t>(vertices.size());
                indices.push_back(quadStartIndex + 0);
                indices.push_back(quadStartIndex + 1);
                indices.push_back(quadStartIndex + 2);
                indices.push_back(quadStartIndex + 0);
                indices.push_back(quadStartIndex + 2);
                indices.push_back(quadStartIndex + 3);

                auto b = Eigen::Vector2f(c.x(), a.y());
                auto d = Eigen::Vector2f(a.x(), c.y());
                auto b_uv = Eigen::Vector2f(c_uv.x(), a_uv.y());
                auto d_uv = Eigen::Vector2f(a_uv.x(), c_uv.y());

                vertices.push_back(Vertex2f(a, a_uv, color.ToUInt()));
                vertices.push_back(Vertex2f(b, b_uv, color.ToUInt()));
                vertices.push_back(Vertex2f(c, c_uv, color.ToUInt()));
                vertices.push_back(Vertex2f(d, d_uv, color.ToUInt()));

                auto &currentCommand = GetCurrentCommand();
                currentCommand.indexCount += 6;
            }

        public:
            uint32_t index = 0;

            [[nodiscard]] inline std::span<const std::byte> GetVertexData() const
            {
                return std::as_bytes(std::span(vertices));
            }

            [[nodiscard]] inline std::span<const std::byte> GetIndexData() const
            {
                return std::as_bytes(std::span(indices));
            }

            [[nodiscard]] inline std::span<const CommandBatch> GetBatches() const
            {
                return std::span(batches);
            }

            [[nodiscard]] inline std::span<const Command> GetBatchCommands(CommandBatch batch) const
            {
                return std::span(commands).subspan(batch.firstCommandIndex, batch.commandCount);
            }

        private:

            bool_t batchInProgress = false;

            std::vector<Command> commands;
            std::vector<CommandBatch> batches;

            std::vector<Vertex2f> vertices;
            std::vector<uint16_t> indices;

            std::vector<Eigen::Vector2f> temp_points;
            std::vector<Eigen::Vector2f> temp_normals;
            std::vector<Eigen::Vector2f> scratchVertices;

            std::stack<Rectangle> scissorStack;
        };

    public:
        struct PushConstants
        {
            Eigen::Vector2f scale;
            uint32_t isSdf;
            float_t sdfRange;
        };

        struct BlurPushConstants
        {
            float scale;
            float strength;
        };

        Overlay(Gfx::Core &gfx);
        ~Overlay();
        Overlay &operator=(const Overlay &) = delete;
        Overlay(const Overlay &) = delete;
        Overlay &operator=(Overlay &&) = delete;
        Overlay(Overlay &&from) = delete;

        void Create();
        void InitializeRendering(const DisplayParameters &wantedDisplayParameters);
        void Render(const Presenter &presenter, double_t absoluteTime, float_t deltaTime);

        // Try to acquire a command buffer from the pool
        // Returns true if a command buffer was acquired, false otherwise (render thread is busy with previous submissions)
        [[nodiscard]] bool_t TryAcquireCommandBuffer(CommandBuffer **outBuffer)
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
        void SubmitCommandBuffer(CommandBuffer *buffer)
        {
            std::scoped_lock lock(commandBufferQueueLock);
            commandBufferQueue.push(buffer);
        }

        Text::Font *GetDefaultFont()
        {
            return defaultFont;
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

        void ReturnCommandBuffer(CommandBuffer *buffer)
        {
            buffer->Reset();

            std::scoped_lock lock(commandBufferPoolLock);
            freeCommandBufferIndices.push(buffer->index);
        }

        void RecordBatch(Gfx::CommandBuffer commandBuffer, std::span<const CommandBuffer::Command> commands, PixelBuffer *targetBuffer);
        void PrepareFrame(const Presenter &presenter);
        void FinishFrame(const Presenter &presenter);

        static constexpr size_t COMMAND_BUFFER_POOL_SIZE = 4;

        static constexpr uint32_t MAX_VERTICES = 65536 * 2;
        static constexpr uint32_t MAX_INDICES = 65536 * 2;

        Gfx::Core &gfx;

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
        Text::Font *defaultFont = nullptr;

        SpinLock commandBufferPoolLock;
        std::vector<CommandBuffer> commandBufferPool;
        std::stack<size_t> freeCommandBufferIndices;
        SpinLock commandBufferQueueLock;
        std::queue<CommandBuffer *> commandBufferQueue;
        CommandBuffer *currentCommandBuffer = nullptr;

        bool_t isReady{false};

        const Logging::Logger &logger;
    };
}