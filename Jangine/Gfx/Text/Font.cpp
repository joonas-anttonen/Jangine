#include "Font.hpp"

#include "../Core.hpp"

#include <freetype/freetype.h>

namespace Jangine::Gfx::Text
{
    Font::Font(FT_Face face, std::vector<Glyph> glyphs, SharedHandle<PixelBuffer> pixelBuffer)
        : face(face),
          glyphs_(std::move(glyphs)),
          pixelBuffer(std::move(pixelBuffer)),
          logger(Jangine::Core::GetLogger("Gfx::Text::Font"))
    {
        ascender_ = face->size->metrics.ascender >> 6;
        descender_ = face->size->metrics.descender >> 6;
        height_ = face->size->metrics.height >> 6;
    }

    Font::~Font()
    {
        if (face)
        {
            FT_Done_Face(face);
            face = nullptr;
        }
    }

    uint32_t Font::GetCharIndex(uint32_t codepoint) const noexcept
    {
        return FT_Get_Char_Index(face, static_cast<FT_ULong>(codepoint));
    }
}