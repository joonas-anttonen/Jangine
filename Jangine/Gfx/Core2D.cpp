#include "Core2d.hpp"
#include "Core.hpp"

namespace Jangine::Gfx
{
    Core2D::Core2D(Gfx::Core *gfx)
        : gfx(gfx), logger(Jangine::Core::GetLogger("Gfx::Core2D")), backBuffer(nullptr, std::ref(*gfx))
    {
        logger.Func(__func__);
    }

    Core2D::~Core2D()
    {
        logger.Func(__func__);
    }

    void Core2D::Create()
    {
    }

    void Core2D::DestroyRendering()
    {
        if (backBuffer)
        {
            backBuffer.reset();
        }
    }

    void Core2D::InitializeRendering(const DisplayParameters &wantedDisplayParameters)
    {
        (void)wantedDisplayParameters; // Avoid unused parameter warning
        logger.Func(__func__);

        // DestroyRendering();

        backBuffer = gfx->CreatePixelBuffer(
            wantedDisplayParameters.displayWidth,
            wantedDisplayParameters.displayHeight,
            wantedDisplayParameters.displayFormat,
            PixelBufferUsage::ColorAttachment | PixelBufferUsage::Sampled |
                PixelBufferUsage::TransferDst | PixelBufferUsage::TransferSrc);
    }

    void Core2D::Render(const Presenter &presenter)
    {
        (void)presenter; // Avoid unused parameter warning
    }

}