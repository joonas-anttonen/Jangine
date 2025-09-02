#pragma once

#include "Shared.hpp"    // Gfx::Text
#include "../Shared.hpp" // Gfx
#include "../../Logging/Logger.hpp"

typedef struct hb_buffer_t hb_buffer_t;
typedef struct hb_font_t hb_font_t;
typedef struct hb_glyph_info_t hb_glyph_info_t;
typedef struct hb_glyph_position_t hb_glyph_position_t;

namespace Jangine::Gfx::Text
{
    // Forward declarations
    class Font;

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

    class Layout
    {
    public:
        struct Glyph
        {
            Eigen::Vector2f position;
            Eigen::Vector2f size;
            Eigen::Vector2f uv0;
            Eigen::Vector2f uv1;
        };

        std::vector<Glyph> glyphs;
        Eigen::Vector2f size = Eigen::Vector2f::Zero();
        const Font *font = nullptr;

        // Appends a glyph to the text layout.
        void Append(const Eigen::Vector2f &position, const Gfx::Text::Glyph &glyph, float_t scale)
        {
            Glyph layoutGlyph;
            layoutGlyph.position = position * scale;
            layoutGlyph.size = Eigen::Vector2f(glyph.width, glyph.height) * scale;
            layoutGlyph.uv0 = Eigen::Vector2f(glyph.u0, glyph.v0);
            layoutGlyph.uv1 = Eigen::Vector2f(glyph.u1, glyph.v1);
            glyphs.push_back(layoutGlyph);
        }

        // Completely resets the state of the text layout.
        void Reset()
        {
            font = nullptr;
            size = Eigen::Vector2f::Zero();
            glyphs.clear();
        }

        // Iterator support for range-based for loops
        auto begin() { return glyphs.begin(); }
        auto end() { return glyphs.end(); }
        auto begin() const { return glyphs.begin(); }
        auto end() const { return glyphs.end(); }
    };

    class Shaper
    {
    public:
        struct Glyph
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
                const std::vector<Text::Glyph> &glyphs;
                const hb_glyph_info_t *info;
                const hb_glyph_position_t *pos;
                size_t index;
                size_t count;

                Iterator(const std::vector<Text::Glyph> &glyphs, const hb_glyph_info_t *i, const hb_glyph_position_t *p, size_t idx, size_t cnt)
                    : glyphs(glyphs), info(i), pos(p), index(idx), count(cnt) {}

                Glyph operator*() const;
                Iterator &operator++()
                {
                    ++index;
                    return *this;
                }

                bool operator!=(const Iterator &other) const
                {
                    return index != other.index;
                }
            };

            const std::vector<Text::Glyph> &glyphs;
            const hb_glyph_info_t *info;
            const hb_glyph_position_t *pos;
            size_t count;

            auto begin() const { return Iterator(glyphs, info, pos, 0, count); }
            auto end() const { return Iterator(glyphs, info, pos, count, count); }
        };

        Shaper(const Font &font);
        ~Shaper();

        Shaper(const Shaper &) = delete;
        Shaper &operator=(const Shaper &) = delete;
        Shaper(Shaper &&) = delete;
        Shaper &operator=(Shaper &&) = delete;

        GlyphIterator ShapeText(std::string_view text);
        float_t MeasureText(std::string_view text);
        void CalculateTextLayout(std::string_view text, float_t scale, Eigen::Vector2f availableSize, bool_t wordWrap, Layout &layout);

    private:
        // Buffers for splitting and shaping
        static constexpr uint32_t MaxWordsPerLine = 128;
        std::vector<uint8_t> scratchRunes;

        hb_buffer_t *hbBuffer = nullptr;
        hb_font_t *hbFont = nullptr;

        const Font &font;

        const Logging::Logger &logger;
    };
}