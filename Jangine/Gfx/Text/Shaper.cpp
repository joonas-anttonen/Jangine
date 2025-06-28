#include "Shaper.hpp"

#include "../Core.hpp"
#include "Font.hpp"

#include <harfbuzz/hb.h>
#include <harfbuzz/hb-ft.h>

namespace Jangine::Gfx::Text
{
    Shaper::Glyph Shaper::GlyphIterator::Iterator::operator*() const
    {
        return Shaper::Glyph{
            static_cast<float_t>(pos[index].x_advance) / 64.0f,
            static_cast<float_t>(pos[index].y_advance) / 64.0f,
            static_cast<float_t>(pos[index].x_offset) / 64.0f,
            static_cast<float_t>(pos[index].y_offset) / 64.0f,
            static_cast<Text::Glyph>(glyphs[info[index].codepoint])};
    }

    Shaper::Shaper(const Font &font)
        : font(font),
          logger(Jangine::Core::GetLogger("Gfx::Text::TextShaper"))
    {
        hbBuffer = hb_buffer_create();
        hbFont = hb_ft_font_create_referenced(font.freetypeFace());
    }

    Shaper::~Shaper()
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
    }

    Shaper::GlyphIterator Shaper::ShapeText(std::string_view text)
    {
        hb_buffer_reset(hbBuffer);
        hb_buffer_add_utf8(hbBuffer, text.data(), static_cast<int32_t>(text.size()), 0, static_cast<int32_t>(text.size()));
        hb_buffer_guess_segment_properties(hbBuffer);

        hb_shape(hbFont, hbBuffer, nullptr, 0);

        uint32_t glyphCount = 0;
        auto glyphInfos = hb_buffer_get_glyph_infos(hbBuffer, &glyphCount);
        auto glyphPositions = hb_buffer_get_glyph_positions(hbBuffer, &glyphCount);

        return GlyphIterator{font.glyphs(), glyphInfos, glyphPositions, glyphCount};
    }

    float_t Shaper::MeasureText(std::string_view text)
    {
        float width = 0;
        for (const auto &glyph : ShapeText(text))
        {
            width += glyph.xAdvance + glyph.glyph.bearingX;
        }
        return width;
    }

    void Shaper::CalculateTextLayout(std::string_view text, float_t scale, Eigen::Vector2f availableSize, bool_t wordWrap, Layout &layout)
    {
        layout.Reset();
        layout.font = &font;

        availableSize *= 1.0f / scale;

        float_t ascender = static_cast<float_t>(font.ascender());
        float_t lineHeight = static_cast<float_t>(font.height());
        float_t spaceWidth = MeasureText(" ");
        uint32_t spaceCodepoint = ' ';
        float_t cursorY = ascender;
        float_t totalWidth = 0;
        float_t totalHeight = 0;
        int wordCountOnLine = 0;
        // int scratchRunesCount = 0;
        float_t availableWidth = availableSize.x();

        scratchRunes.clear();

        auto append_space = [&]()
        {
            // f (scratchRunesCount < static_cast<int>(scratchRunes.size()))
            //    scratchRunes[scratchRunesCount++] = spaceCodepoint;
            scratchRunes.push_back(static_cast<uint8_t>(spaceCodepoint));
        };

        auto append_word = [&](std::string_view word)
        {
            for (char c : word)
            {
                // TODO: Proper Unicode handling (runes)
                scratchRunes.push_back(static_cast<uint8_t>(c));
                // scratchRunes[scratchRunesCount++] = static_cast<uint8_t>(c);
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

                float_t farX = x + glyph.width;
                float_t farY = y + glyph.height;
                totalWidth = std::max(totalWidth, farX);
                totalHeight = std::max(totalHeight, farY);

                bool isWithinBounds = x <= availableSize.x() &&
                                      y <= availableSize.y() &&
                                      (x + glyph.width * 0.5f) <= availableSize.x() &&
                                      (y + glyph.height * 0.5f) <= availableSize.y();
                if (isWithinBounds)
                {
                    layout.Append(Eigen::Vector2f(x, y), glyph, scale);
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

        layout.size = Eigen::Vector2f(totalWidth, totalHeight) * scale;
    }
}