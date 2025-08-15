#include "Core3D.hpp"

namespace Jangine::Gfx
{
    Core3D::Core3D(Gfx::Core *gfx)
        : core(gfx),
          renderBuffer(nullptr, std::ref(*gfx)),
          depthBuffer(nullptr, std::ref(*gfx)),
          motionBuffer(nullptr, std::ref(*gfx)),
          displayBuffer(nullptr, std::ref(*gfx)),
          logger(Jangine::Core::GetLogger("Gfx::Core3D"))
    {
        logger.Func(__func__);
    }

    Core3D::~Core3D()
    {
        logger.Func(__func__);
    }

    void Core3D::Create()
    {
        logger.Func(__func__);
    }

    void Core3D::InitializeRendering(const DisplayParameters &wantedDisplayParameters)
    {
        logger.Func(__func__);

        bool_t antialiasingModeChanged = displayParameters.AntialiasingModeChanged(wantedDisplayParameters);
        bool_t renderSizeChanged = displayParameters.RenderSizeChanged(wantedDisplayParameters);
        bool_t displaySizeChanged = displayParameters.DisplaySizeChanged(wantedDisplayParameters);
        bool_t surfaceFormatChanged = displayParameters.SurfaceFormatChanged(wantedDisplayParameters);

        displayParameters = wantedDisplayParameters;

        bool_t renderResourcesNull = !renderBuffer || !depthBuffer || !motionBuffer;
        bool_t initializeRender = renderResourcesNull || antialiasingModeChanged || renderSizeChanged || surfaceFormatChanged;

        bool_t displayResourcesNull = !displayBuffer;
        bool_t initializeDisplay = displayResourcesNull || displaySizeChanged || surfaceFormatChanged;

        if (initializeRender)
        {
            logger.Debug(
                std::format(
                    "Initializing render buffers: antialiasingModeChanged: {}, renderSizeChanged: {}, surfaceFormatChanged: {}",
                    antialiasingModeChanged,
                    renderSizeChanged,
                    surfaceFormatChanged),
                __func__);

            renderBuffer = core->CreatePixelBuffer(
                wantedDisplayParameters.renderWidth,
                wantedDisplayParameters.renderHeight,
                Format::RGBA32,
                PixelBufferUsage::ColorAttachment | PixelBufferUsage::Sampled |
                    PixelBufferUsage::TransferDst | PixelBufferUsage::TransferSrc,
                Aspect::Color);

            depthBuffer = core->CreatePixelBuffer(
                wantedDisplayParameters.renderWidth,
                wantedDisplayParameters.renderHeight,
                core->GetDeviceDepthFormat(),
                PixelBufferUsage::DepthAttachment | PixelBufferUsage::Sampled |
                    PixelBufferUsage::TransferDst | PixelBufferUsage::TransferSrc,
                Aspect::Depth);

            motionBuffer = core->CreatePixelBuffer(
                wantedDisplayParameters.renderWidth,
                wantedDisplayParameters.renderHeight,
                Format::R16G16,
                PixelBufferUsage::ColorAttachment | PixelBufferUsage::Sampled |
                    PixelBufferUsage::TransferDst | PixelBufferUsage::TransferSrc,
                Aspect::Color);
        }

        if (initializeDisplay)
        {
            logger.Debug(
                std::format(
                    "Initializing display buffer: displaySizeChanged: {}, surfaceFormatChanged: {}",
                    displaySizeChanged,
                    surfaceFormatChanged),
                __func__);

            displayBuffer = core->CreatePixelBuffer(
                wantedDisplayParameters.displayWidth,
                wantedDisplayParameters.displayHeight,
                Format::RGBA32,
                PixelBufferUsage::ColorAttachment | PixelBufferUsage::Sampled |
                    PixelBufferUsage::TransferDst | PixelBufferUsage::TransferSrc,
                Aspect::Color);
        }
    }

    void Core3D::Render(const Presenter &presenter)
    {
        (void)presenter; // Avoid unused parameter warning
    }
}