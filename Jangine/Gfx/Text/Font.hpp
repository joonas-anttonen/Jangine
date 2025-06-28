#pragma once

#include "Shared.hpp"    // Gfx::Text
#include "Shaper.hpp" // Gfx::Text
#include "../Shared.hpp" // Gfx
#include "../../Logging/Logger.hpp"

namespace Jangine::Gfx::Text
{
    class Shaper;
    class Font
    {
    public:
        Font(FT_Face face, std::vector<Glyph> glyphs, SharedHandle<PixelBuffer> pixelBuffer);
        ~Font();

        Font(const Font &) = delete;
        Font &operator=(const Font &) = delete;
        Font(Font &&) = delete;
        Font &operator=(Font &&) = delete;

        int32_t ascender() const noexcept { return ascender_; }
        int32_t descender() const noexcept { return descender_; }
        int32_t height() const noexcept { return height_; }
        FT_Face freetypeFace() const noexcept { return face; }
        const std::vector<Glyph> &glyphs() const noexcept { return glyphs_; }

        uint32_t GetCharIndex(uint32_t codepoint) const noexcept;

        std::unique_ptr<Shaper> CreateTextShaper() const
        {
            return std::make_unique<Shaper>(*this);
        }

        const PixelBuffer* GetPixelBuffer() const noexcept
        {
            return pixelBuffer.get();
        }

    private:
        int32_t ascender_ = 0;
        int32_t descender_ = 0;
        int32_t height_ = 0;

        SharedHandle<PixelBuffer> pixelBuffer;
        std::vector<Glyph> glyphs_;
        FT_Face face = nullptr;
        const Logging::Logger &logger;
    };
}