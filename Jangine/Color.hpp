#pragma once

#include <cstdint>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

namespace Jangine
{
    struct Color
    {
        float r = 0, g = 0, b = 0, a = 1;

        constexpr Color() = default;
        constexpr Color(float r, float g, float b, float a = 1.0f)
            : r(std::clamp(r, 0.0f, 1.0f)),
              g(std::clamp(g, 0.0f, 1.0f)),
              b(std::clamp(b, 0.0f, 1.0f)),
              a(std::clamp(a, 0.0f, 1.0f)) {}

        bool IsTransparent() const { return a == 0.0f; }

        Color WithAlpha(float alpha) const { return Color(r, g, b, alpha); }

        static Color FromUInt(uint32_t rgb)
        {
            return Color(
                ((rgb & 0xff0000) >> 16) / 255.0f,
                ((rgb & 0x00ff00) >> 8) / 255.0f,
                ((rgb & 0x0000ff) >> 0) / 255.0f,
                1.0f);
        }

        uint32_t ToUInt() const
        {
            return (static_cast<uint32_t>(r * 255.0f) << 16) |
                   (static_cast<uint32_t>(g * 255.0f) << 8) |
                   (static_cast<uint32_t>(b * 255.0f) << 0);
        }

        std::string ToHexString() const
        {
            std::ostringstream oss;
            oss << "#" << std::uppercase << std::setfill('0') << std::setw(6) << std::hex << ToUInt();
            return oss.str();
        }

        std::string ToString() const
        {
            std::ostringstream oss;
            oss << "R: " << r << ", G: " << g << ", B: " << b << ", A: " << a;
            return oss.str();
        }

        static Color Lerp(const Color &c1, const Color &c2, float t)
        {
            return Color(
                c1.r + (c2.r - c1.r) * t,
                c1.g + (c2.g - c1.g) * t,
                c1.b + (c2.b - c1.b) * t,
                c1.a + (c2.a - c1.a) * t);
        }

        bool operator==(const Color &other) const
        {
            return r == other.r && g == other.g && b == other.b && a == other.a;
        }
        bool operator!=(const Color &other) const { return !(*this == other); }

        static const Color White, Black, Transparent;

        static const Color NordPolarNight1;   // nord0 - dark blue-gray
        static const Color NordPolarNight2;   // nord1 - slightly lighter blue-gray
        static const Color NordPolarNight3;   // nord2 - medium blue-gray
        static const Color NordPolarNight4;   // nord3 - lightest blue-gray
        static const Color NordSnow1;         // nord4 - near white
        static const Color NordSnow2;         // nord5 - lighter white
        static const Color NordSnow3;         // nord6 - lightest white
        static const Color NordFrostTeal;     // nord7 - teal
        static const Color NordFrostCyan;     // nord8 - cyan
        static const Color NordFrostBlue;     // nord9 - blue
        static const Color NordFrostDeepBlue; // nord10 - deep blue
        static const Color NordAuroraRed;     // nord11 - red
        static const Color NordAuroraOrange;  // nord12 - orange
        static const Color NordAuroraYellow;  // nord13 - yellow
        static const Color NordAuroraGreen;   // nord14 - green
        static const Color NordAuroraPurple;  // nord15 - purple
    };

    inline const Color Color::White = Color(1, 1, 1, 1);
    inline const Color Color::Black = Color(0, 0, 0, 1);
    inline const Color Color::Transparent = Color(0, 0, 0, 0);

    inline const Color Color::NordPolarNight1 = Color::FromUInt(0x2E3440);   // dark blue-gray
    inline const Color Color::NordPolarNight2 = Color::FromUInt(0x3B4252);   // slightly lighter blue-gray
    inline const Color Color::NordPolarNight3 = Color::FromUInt(0x434C5E);   // medium blue-gray
    inline const Color Color::NordPolarNight4 = Color::FromUInt(0x4C566A);   // lightest blue-gray
    inline const Color Color::NordSnow1 = Color::FromUInt(0xD8DEE9);         // near white
    inline const Color Color::NordSnow2 = Color::FromUInt(0xE5E9F0);         // lighter white
    inline const Color Color::NordSnow3 = Color::FromUInt(0xECEFF4);         // lightest white
    inline const Color Color::NordFrostTeal = Color::FromUInt(0x8FBCBB);     // teal
    inline const Color Color::NordFrostCyan = Color::FromUInt(0x88C0D0);     // cyan
    inline const Color Color::NordFrostBlue = Color::FromUInt(0x81A1C1);     // blue
    inline const Color Color::NordFrostDeepBlue = Color::FromUInt(0x5E81AC); // deep blue
    inline const Color Color::NordAuroraRed = Color::FromUInt(0xBF616A);     // red
    inline const Color Color::NordAuroraOrange = Color::FromUInt(0xD08770);  // orange
    inline const Color Color::NordAuroraYellow = Color::FromUInt(0xEBCB8B);  // yellow
    inline const Color Color::NordAuroraGreen = Color::FromUInt(0xA3BE8C);   // green
    inline const Color Color::NordAuroraPurple = Color::FromUInt(0xB48EAD);  // purple
}