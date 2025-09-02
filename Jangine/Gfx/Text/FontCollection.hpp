#pragma once

#include "Shared.hpp"    // Gfx::Text
#include "../Shared.hpp" // Gfx
#include "../../Logging/Logger.hpp"

#include "Font.hpp"

namespace Jangine::Gfx::Text
{
    struct FontKey
    {
        std::string name;
        uint32_t size;

        bool operator==(const FontKey &other) const
        {
            return name == other.name && size == other.size;
        }
    };

    struct FreeTypeGlyphData
    {
        FT_Glyph glyph = nullptr;
        uint8_t *bitmap_ptr = nullptr;
        uint32_t index = 0;
        uint32_t stride = 0;
        uint32_t width = 0;
        uint32_t height = 0;
        int32_t bearing_x = 0;
        int32_t bearing_y = 0;

        // Copy: NO Move: YES
        FreeTypeGlyphData(const FreeTypeGlyphData &) = delete;
        FreeTypeGlyphData &operator=(const FreeTypeGlyphData &) = delete;
        FreeTypeGlyphData(FreeTypeGlyphData &&from)
        {
            glyph = std::exchange(from.glyph, nullptr);
            bitmap_ptr = std::exchange(from.bitmap_ptr, nullptr);
            index = std::exchange(from.index, 0);
            stride = std::exchange(from.stride, 0);
            width = std::exchange(from.width, 0);
            height = std::exchange(from.height, 0);
            bearing_x = std::exchange(from.bearing_x, 0);
            bearing_y = std::exchange(from.bearing_y, 0);
        }
        FreeTypeGlyphData &operator=(FreeTypeGlyphData &&) = default;

        FreeTypeGlyphData(FT_Face in_face, uint32_t in_glyph_index);
        ~FreeTypeGlyphData();
    };
}

namespace std
{
    template <>
    struct hash<Jangine::Gfx::Text::FontKey>
    {
        std::size_t operator()(const Jangine::Gfx::Text::FontKey &k) const
        {
            std::size_t h1 = std::hash<std::string>{}(k.name);
            std::size_t h2 = std::hash<uint32_t>{}(k.size);
            return h1 ^ (h2 << 1); // Combine hashes
        }
    };
}

namespace Jangine::Gfx::Text
{
    // Forward declarations
    class Core;

    class FontCollection
    {
    public:
        FontCollection(Gfx::Core &gfx);
        ~FontCollection();

        FontCollection(const FontCollection &) = delete;
        FontCollection &operator=(const FontCollection &) = delete;
        FontCollection(FontCollection &&) = delete;
        FontCollection &operator=(FontCollection &&) = delete;

        const Font &GetFont(const FontKey &key);

    private:
        bool_t TryLoadFontData(const std::string &name, const uint8_t *&data, size_t &dataSize);

        FT_Library freetypeLibrary;

        std::unordered_map<std::string, std::vector<uint8_t>> fontData;
        std::unordered_map<FontKey, Font> fonts;

        Gfx::Core &gfx;
        const Logging::Logger &logger;
    };
}