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

    void Font::CalculateTextLayout(std::string_view text, Eigen::Vector2f availableSize, bool_t wordWrap, Layout::RenderInfo renderInfo, Layout &layout)
    {
        layout.Reset(this, renderInfo);

        availableSize *= 1.0f / renderInfo.scale;

        float_t ascender = static_cast<float_t>(ascender_);
        float_t lineHeight = static_cast<float_t>(height_);
        float_t spaceWidth = MeasureText(" ");
        uint32_t spaceCodepoint = ' ';
        float_t cursorY = ascender;
        int wordCountOnLine = 0;
        float_t availableWidth = availableSize.x();

        scratchRunes.clear();

        auto append_space = [&]()
        {
            scratchRunes.push_back(static_cast<uint8_t>(spaceCodepoint));
        };

        auto append_word = [&](std::string_view word)
        {
            for (char c : word)
            {
                // TODO: Proper Unicode handling (runes)
                scratchRunes.push_back(static_cast<uint8_t>(c));
            }
        };

        auto output_glyphs = [&]()
        {
            float_t cursorX = 0;
            for (const auto &shapedGlyph : ShapeText(std::string_view(reinterpret_cast<const char *>(scratchRunes.data()), scratchRunes.size())))
            {
                const Text::Glyph &glyph = shapedGlyph.glyph;
                float_t x = cursorX + shapedGlyph.xOffset + glyph.bearingX;
                float_t y = cursorY + shapedGlyph.yOffset - glyph.bearingY;
                cursorX += shapedGlyph.xAdvance;

                bool isWithinBounds = x <= availableSize.x() &&
                                      y <= availableSize.y() &&
                                      (x + glyph.width * 0.5f) <= availableSize.x() &&
                                      (y + glyph.height * 0.5f) <= availableSize.y();
                if (isWithinBounds)
                {
                    layout.Append(Eigen::Vector2f(x, y), glyph);
                }
            }
            scratchRunes.clear();
        };

        for (const auto &line : SplitLinesIterator(text))
        {
            float_t remainingWidth = availableWidth;

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
                if (wordOverflowsLine && wordWrap)
                {
                    if (wordCountOnLine > 0)
                    {
                        output_glyphs();
                        remainingWidth = availableWidth;
                        cursorY += lineHeight;
                        append_word(word);
                        remainingWidth -= wordWidth;
                        wordCountOnLine = 1;
                    }
                    else
                    {
                        if (spaceAfterWord)
                            append_space();
                        append_word(word);
                        output_glyphs();
                        remainingWidth = availableWidth;
                        cursorY += lineHeight;
                        wordCountOnLine = 0;
                    }
                }
                else
                {
                    if (spaceAfterWord)
                        append_space();
                    append_word(word);
                    remainingWidth -= wordWidth;
                    wordCountOnLine++;
                }
            }
            output_glyphs();
            cursorY += lineHeight;
        }
    }

    Font::Font(FT_Face face, std::vector<Text::Glyph> glyphs, SharedHandle<PixelBuffer> pixelBuffer)
        : face(face),
          glyphs_(std::move(glyphs)),
          pixelBuffer(std::move(pixelBuffer))
    {
        ascender_ = face->size->metrics.ascender >> 6;
        descender_ = face->size->metrics.descender >> 6;
        height_ = face->size->metrics.height >> 6;

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