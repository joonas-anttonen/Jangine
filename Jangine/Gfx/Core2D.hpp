#pragma once

#include <Eigen/Core>

#include "../Shared.hpp"
#include "Shared.hpp"
#include "Core.hpp"

#include "Presenter.hpp"

namespace Jangine::Logging
{
    class Logger;
}

namespace Jangine::Gfx
{
    class Core2D
    {
    public:
        struct Vertex
        {
            Eigen::Vector2f position;
            Eigen::Vector2f uv;
            Eigen::Vector4f color;
        };

        struct PushConstants
        {
            Eigen::Vector2f scale;
            Eigen::Vector2f translation;
            uint32_t smoothing;
            Eigen::Vector3f padding; // Padding to ensure 16-byte alignment
        };

        Core2D(Gfx::Core *gfx);
        ~Core2D();

        void Create();
        void InitializeRendering(const DisplayParameters &wantedDisplayParameters);
        void Render(const Presenter &presenter);

    private:
        void DestroyRendering();

        Gfx::Core *gfx = nullptr;

        Handle<PixelBuffer> backBuffer;
        Handle<Pipeline> renderPipeline;
        Handle<Pipeline> compositePipeline;

        const Logging::Logger &logger;
    };
}