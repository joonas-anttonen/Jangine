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
        float R = 0, G = 0, B = 0, A = 1;

        constexpr Color() = default;
        constexpr Color(float r, float g, float b, float a = 1.0f)
            : R(std::clamp(r, 0.0f, 1.0f)),
              G(std::clamp(g, 0.0f, 1.0f)),
              B(std::clamp(b, 0.0f, 1.0f)),
              A(std::clamp(a, 0.0f, 1.0f)) {}

        bool IsTransparent() const { return A == 0.0f; }

        Color WithAlpha(float alpha) const { return Color(R, G, B, alpha); }

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
            return (static_cast<uint32_t>(R * 255.0f) << 16) |
                   (static_cast<uint32_t>(G * 255.0f) << 8) |
                   (static_cast<uint32_t>(B * 255.0f) << 0);
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
            oss << "R: " << R << ", G: " << G << ", B: " << B << ", A: " << A;
            return oss.str();
        }

        static Color Lerp(const Color &c1, const Color &c2, float t)
        {
            return Color(
                c1.R + (c2.R - c1.R) * t,
                c1.G + (c2.G - c1.G) * t,
                c1.B + (c2.B - c1.B) * t,
                c1.A + (c2.A - c1.A) * t);
        }

        bool operator==(const Color &other) const
        {
            return R == other.R && G == other.G && B == other.B && A == other.A;
        }
        bool operator!=(const Color &other) const { return !(*this == other); }

        static const Color White, Black, Transparent;
    };

    inline const Color Color::White = Color(1, 1, 1, 1);
    inline const Color Color::Black = Color(0, 0, 0, 1);
    inline const Color Color::Transparent = Color(0, 0, 0, 0);
}