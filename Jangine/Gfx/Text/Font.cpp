#include "Font.hpp"

#include "../Core.hpp"

#include <freetype/freetype.h>
#include <harfbuzz/hb.h>
#include <harfbuzz/hb-ft.h>

namespace Jangine::Gfx::Text
{
    Font::ShapedGlyph Font::GlyphIterator::Iterator::operator*() const
    {
        return Font::ShapedGlyph{
            static_cast<float_t>(pos[index].x_advance) / 64.0f,
            static_cast<float_t>(pos[index].y_advance) / 64.0f,
            static_cast<float_t>(pos[index].x_offset) / 64.0f,
            static_cast<float_t>(pos[index].y_offset) / 64.0f,
            static_cast<Text::Glyph>(glyphs[info[index].codepoint])};
    }

    Font::GlyphIterator Font::ShapeText(std::string_view text)
    {
        hb_buffer_reset(hbBuffer);
        hb_buffer_add_utf8(hbBuffer, text.data(), static_cast<int32_t>(text.size()), 0, static_cast<int32_t>(text.size()));
        hb_buffer_guess_segment_properties(hbBuffer);

        hb_shape(hbFont, hbBuffer, nullptr, 0);

        uint32_t glyphCount = 0;
        auto glyphInfos = hb_buffer_get_glyph_infos(hbBuffer, &glyphCount);
        auto glyphPositions = hb_buffer_get_glyph_positions(hbBuffer, &glyphCount);

        return GlyphIterator{glyphs_, glyphInfos, glyphPositions, glyphCount};
    }

    float_t Font::MeasureText(std::string_view text)
    {
        float width = 0;
        for (const auto &glyph : ShapeText(text))
        {
            width += glyph.xAdvance + glyph.glyph.bearingX;
        }
        return width;
    }

    void Font::CalculateTextLayoutWithWordWrap(std::string_view text, Eigen::Vector2f availableSize, Layout &layout)
    {
        static float_t spaceWidth = MeasureText(" ");
        static uint32_t spaceCodepoint = ' ';

        float_t cursorY = ascender_;

        layout.scratchRunes.clear();

        auto append_space = [&]()
        {
            layout.scratchRunes.push_back(static_cast<uint8_t>(spaceCodepoint));
        };

        auto append_text = [&](std::string_view span)
        {
            for (char c : span)
            {
                layout.scratchRunes.push_back(static_cast<uint8_t>(c));
            }
        };

        for (const auto &line : SplitLinesIterator(text))
        {
            float_t remainingWidth = availableSize.x();
            int wordCountOnLine = 0;

            if (line.size() == 0)
                continue;

            int i = 0;
            for (auto [wordStart, wordEnd] : SplitWordsIterator(line))
            {
                bool spaceAfterWord = i > 0;
                i++;
                std::string_view word = line.substr(wordStart, wordEnd - wordStart);
                float_t wordWidth = MeasureText(word);
                if (spaceAfterWord)
                    wordWidth += spaceWidth;

                bool wordOverflowsLine = remainingWidth < wordWidth;
                if (wordOverflowsLine)
                {
                    if (wordCountOnLine > 0)
                    {
                        AppendToLayout({layout.scratchRunes.data(), layout.scratchRunes.size()}, Eigen::Vector2f(0, cursorY), availableSize, layout);
                        layout.scratchRunes.clear();
                        remainingWidth = availableSize.x();
                        cursorY += height_;
                        append_text(word);
                        remainingWidth -= wordWidth;
                        wordCountOnLine = 1;
                    }
                    else
                    {
                        if (spaceAfterWord)
                            append_space();
                        append_text(word);
                        AppendToLayout({layout.scratchRunes.data(), layout.scratchRunes.size()}, Eigen::Vector2f(0, cursorY), availableSize, layout);
                        layout.scratchRunes.clear();
                        remainingWidth = availableSize.x();
                        cursorY += height_;
                        wordCountOnLine = 0;
                    }
                }
                else
                {
                    if (spaceAfterWord)
                        append_space();
                    append_text(word);
                    remainingWidth -= wordWidth;
                    wordCountOnLine++;
                }
            }
            AppendToLayout({layout.scratchRunes.data(), layout.scratchRunes.size()}, Eigen::Vector2f(0, cursorY), availableSize, layout);
            layout.scratchRunes.clear();
            cursorY += height_;
        }
    }

    void Font::AppendToLayout(std::string_view text, Eigen::Vector2f position, Eigen::Vector2f availableSize, Layout &layout)
    {
        for (const auto &shapedGlyph : ShapeText(text))
        {
            const Text::Glyph &glyph = shapedGlyph.glyph;
            float_t x = position.x() + shapedGlyph.xOffset + glyph.bearingX;
            float_t y = position.y() + shapedGlyph.yOffset - glyph.bearingY;
            position.x() += shapedGlyph.xAdvance;

            bool isWithinBounds = x <= availableSize.x() &&
                                  y <= availableSize.y() &&
                                  (x + glyph.width * 0.5f) <= availableSize.x() &&
                                  (y + glyph.height * 0.5f) <= availableSize.y();
            if (isWithinBounds)
            {
                layout.Append(Eigen::Vector2f(x, y), glyph);
            }
        }
    }

    void Font::CalculateTextLayout(std::string_view text, Eigen::Vector2f availableSize, Wrap wrap, Layout::RenderInfo renderInfo, Layout &layout)
    {
        layout.Reset(this, renderInfo);

        availableSize *= 1.0f / renderInfo.scale;

        switch (wrap)
        {
        case Wrap::None:
        {
            float_t cursorY = ascender_;

            for (const auto &line : SplitLinesIterator(text))
            {
                if (line.size() > 0)
                {
                    AppendToLayout(line, Eigen::Vector2f(0, cursorY), availableSize, layout);
                }
                cursorY += height_;
            }

            break;
        }
        case Wrap::Word:
            CalculateTextLayoutWithWordWrap(text, availableSize, layout);
        case Wrap::Character:
            // CalculateTextLayoutWithCharacterWrap(text, availableSize, renderInfo, layout);
        default:
            break;
        }
    }

    Font::Font(FT_Face face, std::vector<Text::Glyph> glyphs, SharedHandle<PixelBuffer> pixelBuffer)
        : face(face),
          glyphs_(std::move(glyphs)),
          pixelBuffer(std::move(pixelBuffer))
    {
        ascender_ = static_cast<float_t>(face->size->metrics.ascender >> 6);
        descender_ = static_cast<float_t>(face->size->metrics.descender >> 6);
        height_ = static_cast<float_t>(face->size->metrics.height >> 6);

        hbBuffer = hb_buffer_create();
        hbFont = hb_ft_font_create_referenced(face);
    }

    Font::~Font()
    {
        if (hbBuffer)
        {
            hb_buffer_destroy(hbBuffer);
            hbBuffer = nullptr;
        }
        if (hbFont)
        {
            hb_font_destroy(hbFont);
            hbFont = nullptr;
        }

        if (face)
        {
            FT_Done_Face(face);
            face = nullptr;
        }
    }
}