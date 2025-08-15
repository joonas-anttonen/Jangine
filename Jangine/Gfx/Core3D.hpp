#pragma once

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
    class Core3D
    {
    public:
        Core3D(Gfx::Core *gfx);
        ~Core3D();

        void Create();
        void InitializeRendering(const DisplayParameters &wantedDisplayParameters);
        void Render(const Presenter &presenter);

    private:
        Core *core;

        DisplayParameters displayParameters;

        Handle<PixelBuffer> renderBuffer;
        Handle<PixelBuffer> depthBuffer;
        Handle<PixelBuffer> motionBuffer;
        Handle<PixelBuffer> displayBuffer;

        const Logging::Logger &logger;
    };
}