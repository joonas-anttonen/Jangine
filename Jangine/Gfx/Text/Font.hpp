#pragma once

#include "../Shared.hpp" // Gfx
#include "../../Logging/Logger.hpp"

typedef struct hb_buffer_t hb_buffer_t;
typedef struct hb_font_t hb_font_t;
typedef struct hb_glyph_info_t hb_glyph_info_t;
typedef struct hb_glyph_position_t hb_glyph_position_t;

typedef struct FT_LibraryRec_ *FT_Library;
typedef struct FT_FaceRec_ *FT_Face;
typedef struct FT_GlyphRec_ *FT_Glyph;

namespace Jangine::Gfx::Text
{
    class Font;

    struct FontKey
    {
        std::string name;
        uint32_t size;

        bool operator==(const FontKey &other) const
        {
            return name == other.name && size == other.size;
        }
    };

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

    struct SplitLinesIterator
    {
        std::string_view str;
        size_t pos = 0;

        struct Iterator
        {
            std::string_view str;
            size_t pos;
            size_t next;

            bool operator!=(const Iterator &other) const { return pos != other.pos; }
            std::string_view operator*() const
            {
                // Clamp to string size to avoid out-of-range
                size_t safe_pos = std::min(pos, str.size());
                size_t safe_next = std::min(next, str.size());
                return str.substr(safe_pos, safe_next - safe_pos);
            }
            Iterator &operator++()
            {
                if (next == std::string_view::npos || pos >= str.size())
                {
                    pos = str.size();
                    next = str.size();
                    return *this;
                }
                // Handle \r\n or \n\r
                size_t end = next;
                if (end + 1 < str.size() &&
                    ((str[end] == '\r' && str[end + 1] == '\n') ||
                     (str[end] == '\n' && str[end + 1] == '\r')))
                    ++end;
                pos = end + 1;
                if (pos >= str.size())
                {
                    next = str.size();
                }
                else
                {
                    next = str.find_first_of("\r\n", pos);
                    if (next == std::string_view::npos)
                        next = str.size();
                }
                return *this;
            }
        };

        Iterator begin() const
        {
            if (str.empty())
                return end();
            size_t next = str.find_first_of("\r\n", pos);
            if (next == std::string_view::npos)
                next = str.size();
            return Iterator{str, pos, next};
        }
        Iterator end() const
        {
            return Iterator{str, str.size(), str.size()};
        }
    };

    struct SplitWordsIterator
    {
        std::string_view line;
        size_t pos = 0;

        struct Iterator
        {
            std::string_view line;
            size_t pos;
            size_t next;

            bool operator!=(const Iterator &other) const { return pos != other.pos; }
            std::pair<size_t, size_t> operator*() const { return {pos, next}; }
            Iterator &operator++()
            {
                pos = next;
                // Skip only the first space if present
                if (pos < line.size() && line[pos] == ' ')
                    ++pos;
                if (pos >= line.size())
                {
                    next = line.size();
                    return *this;
                }
                next = line.find(' ', pos);
                if (next == std::string_view::npos)
                    next = line.size();
                return *this;
            }
        };

        Iterator begin() const
        {
            size_t start = pos;
            // Skip leading spaces
            if (start < line.size() && line[start] == ' ')
                ++start;
            if (start >= line.size())
                return end();
            size_t next = line.find(' ', start);
            if (next == std::string_view::npos)
                next = line.size();
            return Iterator{line, start, next};
        }
        Iterator end() const
        {
            return Iterator{line, line.size(), line.size()};
        }
    };

    enum class Wrap
    {
        None,
        Word,
        Character
    };

    class Layout
    {
        friend class Font;

    public:
        struct RenderInfo
        {
            float_t scale{1.0f};
            float_t width{0.01f};
        };

        struct Glyph
        {
            Eigen::Vector2f position;
            Eigen::Vector2f size;
            Eigen::Vector2f uv0;
            Eigen::Vector2f uv1;
        };

        // Appends a glyph to the text layout.
        void Append(const Eigen::Vector2f &position, const Gfx::Text::Glyph &glyph)
        {
            Glyph layoutGlyph;
            layoutGlyph.position = position * renderInfo_.scale;
            layoutGlyph.size = Eigen::Vector2f(glyph.width, glyph.height) * renderInfo_.scale;
            layoutGlyph.uv0 = Eigen::Vector2f(glyph.u0, glyph.v0);
            layoutGlyph.uv1 = Eigen::Vector2f(glyph.u1, glyph.v1);
            glyphs.push_back(layoutGlyph);

            size.x() = std::max(size.x(), layoutGlyph.position.x() + layoutGlyph.size.x());
            size.y() = std::max(size.y(), layoutGlyph.position.y() + layoutGlyph.size.y());
        }

        // Completely resets the state of the text layout.
        void Reset(const Font *in_font, Layout::RenderInfo renderInfo)
        {
            font_ = in_font;
            renderInfo_ = renderInfo;
            size = Eigen::Vector2f::Zero();
            glyphs.clear();
        }

        // Iterator support for range-based for loops
        auto begin() const { return glyphs.cbegin(); }
        auto end() const { return glyphs.cend(); }

        Eigen::Vector2f extent() const { return size; }
        const Font *font() const { return font_; }
        float_t font_width() const { return renderInfo_.width; }

    private:
        std::vector<Glyph> glyphs;
        Eigen::Vector2f size = Eigen::Vector2f::Zero();
        const Font *font_ = nullptr;
        Layout::RenderInfo renderInfo_;

        std::vector<char> scratchRunes;
    };

    class Font
    {
        struct ShapedGlyph
        {
            float_t xAdvance;
            float_t yAdvance;
            float_t xOffset;
            float_t yOffset;
            Text::Glyph glyph;
        };

        struct GlyphIterator
        {
            struct Iterator
            {
                explicit Iterator(const std::vector<Text::Glyph> &glyphs, const hb_glyph_info_t *i, const hb_glyph_position_t *p, size_t idx, size_t cnt)
                    : glyphs(glyphs), info(i), pos(p), index(idx), count(cnt) {}

                ShapedGlyph operator*() const;
                Iterator &operator++()
                {
                    ++index;
                    return *this;
                }

                bool operator!=(const Iterator &other) const
                {
                    return index != other.index;
                }

            private:
                const std::vector<Text::Glyph> &glyphs;
                const hb_glyph_info_t *info;
                const hb_glyph_position_t *pos;
                size_t index;
                size_t count;
            };

            auto begin() const { return Iterator(glyphs, info, pos, 0, count); }
            auto end() const { return Iterator(glyphs, info, pos, count, count); }

            explicit GlyphIterator(const std::vector<Text::Glyph> &g, const hb_glyph_info_t *i, const hb_glyph_position_t *p, size_t c)
                : glyphs(g), info(i), pos(p), count(c) {}

        private:
            const std::vector<Text::Glyph> &glyphs;
            const hb_glyph_info_t *info;
            const hb_glyph_position_t *pos;
            size_t count;
        };

    public:
        using Ptr = Font *;

        Font(FT_Face face, std::vector<Text::Glyph> glyphs, SharedHandle<PixelBuffer> pixelBuffer);
        ~Font();

        Font(const Font &) = delete;
        Font &operator=(const Font &) = delete;
        Font(Font &&) = delete;
        Font &operator=(Font &&) = delete;

        const PixelBuffer *GetPixelBuffer() const noexcept
        {
            return pixelBuffer.get();
        }

        void CalculateTextLayout(std::string_view text, Eigen::Vector2f availableSize, Wrap wrap, Layout::RenderInfo renderInfo, Layout &layout);

    private:
        void CalculateTextLayoutWithWordWrap(std::string_view text, Eigen::Vector2f availableSize, Layout &layout);

        void AppendToLayout(std::string_view text, Eigen::Vector2f position, Eigen::Vector2f availableSize, Layout &layout);

        GlyphIterator ShapeText(std::string_view text);
        float_t MeasureText(std::string_view text);

        float_t ascender_{0};
        float_t descender_{0};
        float_t height_{0};

        SharedHandle<PixelBuffer> pixelBuffer;
        std::vector<Text::Glyph> glyphs_;

        hb_buffer_t *hbBuffer{nullptr};
        hb_font_t *hbFont{nullptr};
        FT_Face face{nullptr};
    };
}