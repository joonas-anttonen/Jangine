#pragma once

#include "../Shared.hpp"
#include "Shared.hpp"
#include "Core.hpp"

#include "Presenter.hpp"

#include "Camera.hpp"

namespace Jangine::Logging
{
    class Logger;
}

namespace Jangine::Gfx
{
    class Core3D
    {
        struct ShapeVertex
        {
            Eigen::Vector3f Position;
            Eigen::Vector2f UV;
        };

        struct PerSceneData
        {
            Eigen::Matrix4f ViewProjection;

            Eigen::Matrix4f View;
            Eigen::Matrix4f ViewInverse;

            Eigen::Vector3f ViewPosition;
            float_t _Padding0;

            Eigen::Vector2f Screen;
        };

        struct PerMeshData
        {
            Eigen::Matrix4f Transform;

            Eigen::Vector4f Color;
            Eigen::Vector4f ColorOuterStart;
            Eigen::Vector4f ColorInnerEnd;
            Eigen::Vector4f ColorOuterEnd;
            float_t Radius;
            float_t Thickness;
            float_t AngleStart;
            float_t AngleEnd;
            int32_t ScaleSpace;
            int32_t Alignment;
        };

        struct PerLineMeshData
        {
            Eigen::Matrix4f Transform;

            Eigen::Vector3f Start;
            float_t _Padding0;
            Eigen::Vector3f End;
            float_t _Padding1;
            Eigen::Vector4f Color;
            Eigen::Vector4f ColorEnd;
            float_t Thickness;
            int32_t ScaleSpace;
            int32_t Alignment;
        };

    public:
        Core3D(Gfx::Core *gfx);
        ~Core3D();

        void Create();
        void InitializeRendering(const DisplayParameters &wantedDisplayParameters);
        void Render(const Presenter &presenter, double_t absoluteTime, float_t deltaTime);

    private:
        Core *gfx;

        DisplayParameters displayParameters;

        static constexpr uint32_t MAX_VERTICES = 65536;
        static constexpr uint32_t MAX_INDICES = 65536;

        Handle<MemoryBuffer> vertexBuffer;
        Handle<MemoryBuffer> indexBuffer;
        Handle<MemoryBuffer> perSceneBuffer;
        Handle<MemoryBuffer> perMeshBuffer;

        Handle<PixelBuffer> renderBuffer;
        Handle<PixelBuffer> depthBuffer;
        Handle<PixelBuffer> motionBuffer;
        Handle<PixelBuffer> displayBuffer;

        Handle<Pipeline> shapePipeline;

        BlenderCamera camera;

        const Logging::Logger &logger;
    };
}