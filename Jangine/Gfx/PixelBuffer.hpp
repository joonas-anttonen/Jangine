#pragma once

#include "Shared.hpp"

namespace Jangine::Gfx
{
    // Forward declarations
    class Core;

    class PixelBufferSource
    {
    public:
        PixelBufferSource(Core *gfx, bool_t isSingleImage);

    private:
        void ThreadFunction()
        {
            while (!threadExitRequested.load(std::memory_order_relaxed))
            {
                // Process images or data here
                // This is a placeholder for the actual image processing logic
            }
        }

        SharedHandle<PixelBuffer> imageA;
        SharedHandle<PixelBuffer> imageB;
        Handle<MemoryBuffer> stagingBuffer;

        bool_t isSingleImage;

        Core *gfx;

        std::atomic<bool> threadExitRequested{false};
        std::thread thread;
    };

    class WebPSource : public PixelBufferSource
    {
    public:
        WebPSource(Core *gfx)
            : PixelBufferSource(gfx, false)
        {
        }

    private:
    };
}