#pragma once

#include "../Shared.hpp"
#include "Shared.hpp"
#include "Core.hpp"

#include "Text/Shaper.hpp"

namespace Jangine::Gfx
{
    namespace Text
    {
        class Font;
    }

    class CommandBuffer2D
    {
        friend class Core2D;

    public:
        CommandBuffer2D() = default;
        ~CommandBuffer2D() = default;
        CommandBuffer2D &operator=(const CommandBuffer2D &) = delete;
        CommandBuffer2D(const CommandBuffer2D &) = delete;
        CommandBuffer2D &operator=(CommandBuffer2D &&) = default;
        CommandBuffer2D(CommandBuffer2D &&from) = default;

        /// @brief Single draw command
        struct Command
        {
            explicit Command()
                : vertexOffset(0), indexOffset(0), indexCount(0), texture(nullptr), font(nullptr) {}
            explicit Command(uint32_t vertexOffset, uint32_t indexOffset)
                : vertexOffset(vertexOffset), indexOffset(indexOffset), indexCount(0), texture(SharedHandle<PixelBuffer>()), font(nullptr) {}
            Command &operator=(const Command &) = delete;
            Command(const Command &) = delete;
            Command &operator=(Command &&) = default;
            Command(Command &&from) = default;

            uint32_t vertexOffset;
            uint32_t indexOffset;
            uint32_t indexCount;
            SharedHandle<PixelBuffer> texture;
            const Text::Font *font;
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

            // Check if new command is needed (font changed)
            auto &currentCommand = GetCurrentCommand();
            if (currentCommand.font != layout.font)
            {
                BeginCommand().font = layout.font;
            }

            for (const auto &glyph : layout.glyphs)
            {
                Eigen::Vector2f a = position + glyph.position;
                Eigen::Vector2f c = a + glyph.size;

                PushQuadUV(a, c, glyph.uv0, glyph.uv1, color);
            }
        }

        void DrawImage(SharedHandle<PixelBuffer> image, Eigen::Vector2f targetPosition, Eigen::Vector2f targetExtent, ImageFit imageFit)
        {
            ThrowInvalidOperationIfNot(batchInProgress);

            Command &newCommand = BeginCommand();
            newCommand.texture = image;

            Eigen::Vector2f imageExtent(static_cast<float_t>(image->width), static_cast<float_t>(image->height));
            Eigen::Vector2f uv0(0.0f, 0.0f);
            Eigen::Vector2f uv1(1.0f, 1.0f);

            Eigen::Vector2f finalImagePosition = targetPosition;
            Eigen::Vector2f finalImageExtent = targetExtent;

            switch (imageFit)
            {
            case ImageFit::None:
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

            PushQuadUV(finalImagePosition, finalImagePosition + finalImageExtent, uv0, uv1, Color::White);
        }

        void DrawRectangle(Rectangle rectangle, Color color, float_t thickness = 1.0f)
        {
            DrawRectangle(rectangle.position(), rectangle.position() + rectangle.extent(), color, thickness);
        }

        void DrawRectangle(Eigen::Vector2f a, Eigen::Vector2f c, Color color, float_t thickness = 1.0f)
        {
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
            PushQuadUV(rectangle.position(), rectangle.position() + rectangle.extent(),
                       Eigen::Vector2f(0, 0), Eigen::Vector2f(1, 1), color);
        }

        void FillRectangle(Eigen::Vector2f a, Eigen::Vector2f c, Color color)
        {
            PushQuadUV(a, c, Eigen::Vector2f(0, 0), Eigen::Vector2f(1, 1), color);
        }

        void PushQuadUV(Eigen::Vector2f a, Eigen::Vector2f c, Eigen::Vector2f a_uv, Eigen::Vector2f c_uv, Color color)
        {
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

            vertices.push_back(Vertex2f(a, a_uv, GetColorVector4f(color)));
            vertices.push_back(Vertex2f(b, b_uv, GetColorVector4f(color)));
            vertices.push_back(Vertex2f(c, c_uv, GetColorVector4f(color)));
            vertices.push_back(Vertex2f(d, d_uv, GetColorVector4f(color)));

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
        static inline Eigen::Vector4f GetColorVector4f(Color color)
        {
            return Eigen::Vector4f(color.r, color.g, color.b, color.a);
        }

        bool_t batchInProgress = false;

        std::vector<Command> commands;
        std::vector<CommandBatch> batches;

        std::vector<Vertex2f> vertices;
        std::vector<uint16_t> indices;

        std::vector<Eigen::Vector2f> temp_points;
        std::vector<Eigen::Vector2f> temp_normals;
        std::vector<Eigen::Vector2f> scratchVertices;
    };
}