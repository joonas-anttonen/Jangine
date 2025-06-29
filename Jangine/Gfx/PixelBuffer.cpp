#include "PixelBuffer.hpp"

#include "Core.hpp"

Jangine::Gfx::PixelBufferSource::PixelBufferSource(Core *gfx, bool_t isSingleImage)
    : isSingleImage(isSingleImage),
      gfx(gfx),
      imageA(MakeShared<PixelBuffer>(nullptr, *gfx)),
      imageB(MakeShared<PixelBuffer>(nullptr, *gfx)),
      stagingBuffer(MakeUnique<MemoryBuffer>(nullptr, *gfx))
{
    if (!isSingleImage)
    {
        thread = std::thread(&PixelBufferSource::ThreadFunction, this);
    }
}