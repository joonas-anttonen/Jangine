#pragma once

#include "../Shared.hpp"
#include "Shared.hpp"
#include "Core.hpp"

#include <iostream>

namespace Jangine::Gfx
{
    class CommandBuffer2D
    {
        friend class Core2D;

    public:
        // Single draw command
        struct Command
        {
            uint32_t vertexOffset;
            uint32_t indexOffset;
            uint32_t indexCount;
            SharedHandle<PixelBuffer> texture;
            int32_t font;

            explicit Command() = default;
            explicit Command(uint32_t vertexOffset, uint32_t indexOffset, uint32_t indexCount, SharedHandle<PixelBuffer> texture, int32_t font)
                : vertexOffset(vertexOffset), indexOffset(indexOffset), indexCount(indexCount), texture(texture), font(font) {}
        };

        // Batch of draw commands
        struct CommandBatch
        {
            uint32_t firstCommandIndex;
            uint32_t commandCount;
            PixelBuffer *surface;

            explicit CommandBatch() = default;
            explicit CommandBatch(uint32_t firstCommandIndex, uint32_t commandCount = 0, PixelBuffer *surface = nullptr)
                : firstCommandIndex(firstCommandIndex), commandCount(commandCount), surface(surface) {}
        };

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
            commands.push_back(Command(static_cast<uint32_t>(vertices.size()), static_cast<uint32_t>(indices.size()), 0, SharedHandle<PixelBuffer>(), -1));
            return commands.back();
        }

        [[nodiscard]] Command &GetCurrentCommand()
        {
            ThrowInvalidOperationIfNot(batchInProgress);
            ThrowInvalidOperationIf(commands.empty());

            return commands.back();
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

        void DrawRectangle(Rectangle rectangle, Color color, float thickness = 1.0f)
        {
            DrawRectangle(rectangle.position(), rectangle.position() + rectangle.extent(), color, thickness);
        }

        void DrawRectangle(Eigen::Vector2f a, Eigen::Vector2f c, Color color, float thickness = 1.0f)
        {
            float half_thickness = thickness * 0.5f;

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

        void PushQuadUV(Eigen::Vector2f a, const Eigen::Vector2f c, Eigen::Vector2f a_uv, Eigen::Vector2f c_uv, Color color)
        {
            auto b = Eigen::Vector2f(c.x(), a.y());
            auto d = Eigen::Vector2f(a.x(), c.y());
            auto b_uv = Eigen::Vector2f(c_uv.x(), a_uv.y());
            auto d_uv = Eigen::Vector2f(a_uv.x(), c_uv.y());

            auto currentVertexCount = vertices.size();
            indices.push_back(static_cast<uint16_t>(currentVertexCount + 0));
            indices.push_back(static_cast<uint16_t>(currentVertexCount + 1));
            indices.push_back(static_cast<uint16_t>(currentVertexCount + 2));
            indices.push_back(static_cast<uint16_t>(currentVertexCount + 0));
            indices.push_back(static_cast<uint16_t>(currentVertexCount + 2));
            indices.push_back(static_cast<uint16_t>(currentVertexCount + 3));

            vertices.push_back(Vertex2f(a, a_uv, GetColorVector4f(color)));
            vertices.push_back(Vertex2f(b, b_uv, GetColorVector4f(color)));
            vertices.push_back(Vertex2f(c, c_uv, GetColorVector4f(color)));
            vertices.push_back(Vertex2f(d, d_uv, GetColorVector4f(color)));

            auto &currentCommand = GetCurrentCommand();
            currentCommand.indexCount += 6;
        }

    public:
        uint32_t index = 0;

        [[nodiscard]] inline std::span<const uint8_t> GetVertexData() const
        {
            return std::span(reinterpret_cast<const uint8_t *>(vertices.data()), vertices.size() * sizeof(Vertex2f));
        }

        [[nodiscard]] inline std::span<const uint8_t> GetIndexData() const
        {
            return std::span(reinterpret_cast<const uint8_t *>(indices.data()), indices.size() * sizeof(uint16_t));
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