#include "FontCollection.hpp"

#include "../Core.hpp"

#include <freetype/freetype.h>
#include <freetype/ftglyph.h>

#if !defined(JANGINE_INTELLISENSE_IGNORE_GENERATED_FILES)
#include "BuiltInFonts.hpp"
#endif

namespace Jangine::Gfx::Text
{
    static void ThrowIfFailed(FT_Error errorCode, const std::string &message = "")
    {
        if (errorCode != 0)
        {
            throw Jangine::JangineException(std::format("{}", message));
        }
    }

    FreeTypeGlyphData::FreeTypeGlyphData(FT_Face in_face, uint32_t in_glyph_index)
    {
        ThrowIfFailed(FT_Load_Glyph(
            in_face,
            static_cast<FT_UInt>(in_glyph_index),
            FT_LOAD_RENDER));

        FT_GlyphSlot slot = in_face->glyph;
        ThrowIfFailed(FT_Render_Glyph(
            slot,
            FT_RENDER_MODE_SDF));

        ThrowIfFailed(FT_Get_Glyph(
            slot,
            &glyph));

        bitmap_ptr = slot->bitmap.buffer;
        stride = slot->bitmap.pitch;
        width = slot->bitmap.width;
        height = slot->bitmap.rows;
        index = in_glyph_index;
        bearing_x = slot->metrics.horiBearingX >> 6; // Convert from 26.6 fixed point to pixels
        bearing_y = slot->metrics.horiBearingY >> 6; // Convert from 26.6 fixed point to pixels
    }

    FreeTypeGlyphData::~FreeTypeGlyphData()
    {
        if (glyph)
        {
            FT_Done_Glyph(glyph);
            glyph = nullptr;
        }
    }

    FontCollection::FontCollection(Gfx::Core *gfx)
        : gfx(gfx),
          logger(Jangine::Core::GetLogger("Gfx::Text::FontCollection"))
    {
        FT_Library library;
        ThrowIfFailed(FT_Init_FreeType(&library));
        freetypeLibrary = library;
    }

    FontCollection::~FontCollection()
    {
        // In C++, member destructors are called after the parent destructor
        // and member constructors are called before the parent constructor
        fonts.clear();

        if (freetypeLibrary)
        {
            FT_Done_FreeType(freetypeLibrary);
            freetypeLibrary = nullptr;
        }
    }

    static std::vector<FreeTypeGlyphData> LoadGlyphs(FT_Face face)
    {
        std::vector<FreeTypeGlyphData> glyphs;

        for (uint32_t i = 0; i < static_cast<uint32_t>(face->num_glyphs); ++i)
        {
            glyphs.emplace_back(face, i);
        }

        return glyphs;
    }

    static uint32_t CalculateAtlasSize(const std::vector<FreeTypeGlyphData> &glyphs, uint32_t glyphPadding)
    {
        uint32_t atlasSize = 128;

        while (true)
        {
            bool_t isLargeEnough = true;

            uint32_t x = 0;
            uint32_t y = 0;
            uint32_t maxHeight = 0;

            for (const auto &ftGlyph : glyphs)
            {
                uint32_t glyphWidth = ftGlyph.width + glyphPadding * 2;
                uint32_t glyphHeight = ftGlyph.height + glyphPadding * 2;

                maxHeight = std::max(maxHeight, glyphHeight);
                if (x + glyphWidth > atlasSize)
                {
                    x = 0;
                    y += maxHeight;
                    maxHeight = glyphHeight;
                }
                if (y + glyphHeight > atlasSize)
                {
                    isLargeEnough = false;
                    break;
                }
                x += glyphWidth;
            }

            if (!isLargeEnough)
            {
                atlasSize *= 2;
            }
            else
            {
                break;
            }
        }

        return atlasSize;
    }

    static void BuildAtlas(const std::vector<FreeTypeGlyphData> &freetypeGlyphs, std::vector<Glyph> &glyphs, std::vector<uint8_t> &atlasData, uint32_t& atlasSize)
    {
        static constexpr uint32_t channels = 4;
        static constexpr uint32_t padding = 2;

        atlasSize = CalculateAtlasSize(freetypeGlyphs, padding);
        atlasData.resize(atlasSize * atlasSize * channels);

        uint32_t atlasX = 0;
        uint32_t atlasY = 0;
        uint32_t maxHeight = 0;
        for (const auto &ftGlyph : freetypeGlyphs)
        {
            const uint32_t glyphWidth = ftGlyph.width;
            const uint32_t glyphHeight = ftGlyph.height;
            const uint32_t glyphWidthPadding = glyphWidth + padding * 2;
            const uint32_t glyphHeightPadding = glyphHeight + padding * 2;

            maxHeight = std::max(maxHeight, glyphHeightPadding);
            // If we are out of atlas bounds, go to the next line
            if (atlasX + glyphWidthPadding > atlasSize)
            {
                atlasX = 0;
                atlasY += maxHeight;
                maxHeight = glyphHeightPadding;
            }

            // Copy glyph bitmap to atlas bitmap
            const uint32_t glyphXPosInBitmap = atlasX + padding; // in pixels
            const uint32_t glyphYPosInBitmap = atlasY + padding;

            // Copy glyph bitmap to atlas bitmap
            for (uint32_t glyphY = 0; glyphY < glyphHeight; ++glyphY)
            {
                for (uint32_t glyphX = 0; glyphX < glyphWidth; ++glyphX)
                {
                    uint32_t atlasBitmapIndex = ((glyphYPosInBitmap + glyphY) * atlasSize + (glyphXPosInBitmap + glyphX)) * channels;

                    uint32_t readPosition = glyphY * ftGlyph.stride + glyphX;
                    ThrowInvalidOperationIf(readPosition >= ftGlyph.width * ftGlyph.height, "Read position out of bounds");
                    uint8_t value = ftGlyph.bitmap_ptr[readPosition];
                    atlasData[atlasBitmapIndex + 0] = 255;
                    atlasData[atlasBitmapIndex + 1] = 255;
                    atlasData[atlasBitmapIndex + 2] = 255;
                    atlasData[atlasBitmapIndex + 3] = value;
                }
            }

            // Calculate glyph position in texture coordinates
            float_t u0 = static_cast<float_t>(glyphXPosInBitmap) / atlasSize;
            float_t v0 = static_cast<float_t>(glyphYPosInBitmap) / atlasSize;
            float_t u1 = static_cast<float_t>(glyphXPosInBitmap + glyphWidth) / atlasSize;
            // HACK: Add 1 pixel to the height to prevent cutting off the bottom of some glyphs
            float_t v1 = static_cast<float_t>(glyphYPosInBitmap + glyphHeight + 1) / atlasSize;

            Glyph glyph{
                ftGlyph.index,
                u0, v0, u1, v1,
                ftGlyph.width,
                ftGlyph.height,
                ftGlyph.bearing_x,
                ftGlyph.bearing_y};

            if (glyphs.size() <= glyph.index)
                glyphs.resize(glyph.index + 1);
            glyphs[glyph.index] = glyph;

            atlasX += glyphWidthPadding;
        }
    }

    const Font &FontCollection::GetFont(const FontKey &id)
    {
        auto it = fonts.find(id);
        if (it != fonts.end())
        {
            return it->second;
        }

        const uint8_t *data = nullptr;
        size_t dataSize = 0;

        if (!data)
        {
            auto fontDataIt = fontData.find(id.name);
            if (fontDataIt != fontData.end())
            {
                data = fontDataIt->second.data();
                dataSize = fontDataIt->second.size();
            }
        }

        if (!data)
        {
            if (!TryLoadFontData(id.name, data, dataSize))
            {
                ThrowInvalidOperationIf(data == nullptr || dataSize == 0,
                                        std::format("Failed to load font data for '{}'", id.name));
            }
        }

        FT_Face face;
        ThrowIfFailed(FT_New_Memory_Face(freetypeLibrary, data, static_cast<FT_Long>(dataSize), 0, &face));
        ThrowIfFailed(FT_Set_Char_Size(
            face,
            static_cast<FT_F26Dot6>(id.size * 64),
            static_cast<FT_F26Dot6>(id.size * 64),
            72, 72));

        std::vector<FreeTypeGlyphData> freetypeGlyphs = LoadGlyphs(face);
        std::vector<Glyph> glyphs;
        std::vector<uint8_t> atlasData;
        uint32_t atlasSize = 0;
        BuildAtlas(freetypeGlyphs, glyphs, atlasData, atlasSize);

        Handle<PixelBuffer> pixelBuffer = gfx->CreatePixelBuffer(
            std::span<const std::byte>(reinterpret_cast<const std::byte *>(atlasData.data()), atlasData.size()),
            atlasSize, atlasSize,
            Format::RGBA8,
            PixelBufferUsage::Sampled);

        auto pair = fonts.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(id),
            std::forward_as_tuple(face, std::move(glyphs), PromoteToShared(std::move(pixelBuffer))));
        return pair.first->second;
    }

    bool_t FontCollection::TryLoadFontData(const std::string &name, const uint8_t *&data, size_t &dataSize)
    {
        (void)name; // Unused parameter, can be used for custom font loading logic
        (void)data;
        (void)dataSize;
#if !defined(JANGINE_INTELLISENSE_IGNORE_GENERATED_FILES)
        data = BuiltInFonts_data;
        dataSize = BuiltInFonts_data_size;
#endif
        return data != nullptr && dataSize > 0;
    }
}