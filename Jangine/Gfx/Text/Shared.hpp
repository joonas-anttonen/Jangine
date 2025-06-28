#pragma once

typedef struct FT_LibraryRec_ *FT_Library;
typedef struct FT_FaceRec_ *FT_Face;
typedef struct FT_GlyphRec_ *FT_Glyph;

#include <cstdint>

namespace Jangine::Gfx::Text
{
    struct Glyph
    {
        uint32_t index;
        float u0;
        float v0;
        float u1;
        float v1;
        uint32_t width;
        uint32_t height;
        int32_t bearingX;
        int32_t bearingY;
    };
}
