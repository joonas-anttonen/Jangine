#pragma once

#include "../Shared.hpp"
#include "Shared.hpp"

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
        Core2D(Gfx::Core *gfx);
        ~Core2D();

        void Create();
        void InitializeRendering(const DisplayParameters &wantedDisplayParameters);
        void Render(const Presenter &presenter);

    private:
        void DestroyRendering();

        Gfx::Core *gfx = nullptr;
        Handle<PixelBuffer> backBuffer;

        const Logging::Logger &logger;
    };
}