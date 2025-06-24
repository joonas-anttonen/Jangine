#pragma once
#include <cstdint>
#include <format>

namespace Jangine::Gfx
{
    enum class Format : uint32_t
    {
        Undefined = 0,
        R32 = 100,
        RG32 = 103,
        RGB32 = 106,
        RGBA32 = 109, // = VK_FORMAT_R32G32B32A32_SFLOAT
        RGBA8 = 37,   // = VK_FORMAT_R8G8B8A8_UNORM
        BGRA8 = 44,   // = VK_FORMAT_B8G8R8A8_UNORM
        D32 = 126     // = VK_FORMAT_D32_SFLOAT
    };

    enum class AntialiasingMode : uint32_t
    {
        None,
        Fsr
    };

    enum class UpscalingMode : uint32_t
    {
        None,
        Quality,
        Balanced,
        Performance,
        UltraPerformance
    };

    enum class MemoryUsage : uint32_t
    {
        None = 0,
        TransferSrc = 1 << 0,  // = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
        TransferDst = 1 << 1,  // = VK_BUFFER_USAGE_TRANSFER_DST_BIT
        UniformTexel = 1 << 2, // = VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT
        StorageTexel = 1 << 3, // = VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT
        Uniform = 1 << 4,      // = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
        Storage = 1 << 5,      // = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
        Index = 1 << 6,        // = VK_BUFFER_USAGE_INDEX_BUFFER_BIT
        Vertex = 1 << 7,       // = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
        Indirect = 1 << 8      // = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
    };

    inline MemoryUsage operator|(MemoryUsage a, MemoryUsage b)
    {
        return static_cast<MemoryUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    enum class MemoryAccess : uint32_t
    {
        None = 0,
        Write = 1 << 0,
        Read = 1 << 1,
        ReadWrite = Write | Read
    };

    inline MemoryAccess operator|(MemoryAccess a, MemoryAccess b)
    {
        return static_cast<MemoryAccess>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    enum class PixelBufferUsage : uint32_t
    {
        None = 0,
        TransferSrc = 1 << 0,     // = VK_IMAGE_USAGE_TRANSFER_SRC_BIT
        TransferDst = 1 << 1,     // = VK_IMAGE_USAGE_TRANSFER_DST_BIT
        Sampled = 1 << 2,         // = VK_IMAGE_USAGE_SAMPLED_BIT
        Storage = 1 << 3,         // = VK_IMAGE_USAGE_STORAGE_BIT
        ColorAttachment = 1 << 4, // = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
        DepthAttachment = 1 << 5, // = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
        InputAttachment = 1 << 6  // = VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT
    };

    inline PixelBufferUsage operator|(PixelBufferUsage a, PixelBufferUsage b)
    {
        return static_cast<PixelBufferUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    enum class Aspect : uint32_t
    {
        Color = 1 << 0,   // = VK_IMAGE_ASPECT_COLOR_BIT
        Depth = 1 << 1,   // = VK_IMAGE_ASPECT_DEPTH_BIT
        Stencil = 1 << 2, // = VK_IMAGE_ASPECT_STENCIL_BIT
        DepthStencil = Depth | Stencil
    };

    inline Aspect operator|(Aspect a, Aspect b)
    {
        return static_cast<Aspect>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    enum class Samples : uint32_t
    {
        X1 = 1,   // = VK_SAMPLE_COUNT_1_BIT
        X2 = 2,   // = VK_SAMPLE_COUNT_2_BIT
        X4 = 4,   // = VK_SAMPLE_COUNT_4_BIT
        X8 = 8,   // = VK_SAMPLE_COUNT_8_BIT
        X16 = 16, // = VK_SAMPLE_COUNT_16_BIT
        X32 = 32, // = VK_SAMPLE_COUNT_32_BIT
        X64 = 64  // = VK_SAMPLE_COUNT_64_BIT
    };

    enum class PhysicalDeviceType : uint32_t
    {
        Discrete,
        Integrated,
        Virtual,
        Cpu,
        Other
    };
}

// Custom formatters
namespace std
{
    template <>
    struct formatter<Jangine::Gfx::Format>
    {
        constexpr auto parse(format_parse_context &ctx) { return ctx.begin(); }

        template <typename FormatContext>
        auto format(Jangine::Gfx::Format format, FormatContext &ctx) const
        {
            switch (format)
            {
            case Jangine::Gfx::Format::Undefined:
                return format_to(ctx.out(), "Undefined");
            case Jangine::Gfx::Format::R32:
                return format_to(ctx.out(), "R32");
            case Jangine::Gfx::Format::RG32:
                return format_to(ctx.out(), "RG32");
            case Jangine::Gfx::Format::RGB32:
                return format_to(ctx.out(), "RGB32");
            case Jangine::Gfx::Format::RGBA32:
                return format_to(ctx.out(), "RGBA32");
            case Jangine::Gfx::Format::RGBA8:
                return format_to(ctx.out(), "RGBA8");
            case Jangine::Gfx::Format::BGRA8:
                return format_to(ctx.out(), "BGRA8");
            case Jangine::Gfx::Format::D32:
                return format_to(ctx.out(), "D32");
            default:
                return format_to(ctx.out(), "Unknown Format");
            }
        }
    };

    template <>
    struct formatter<Jangine::Gfx::AntialiasingMode>
    {
        constexpr auto parse(format_parse_context &ctx) { return ctx.begin(); }

        template <typename FormatContext>
        auto format(Jangine::Gfx::AntialiasingMode mode, FormatContext &ctx) const
        {
            switch (mode)
            {
            case Jangine::Gfx::AntialiasingMode::None:
                return format_to(ctx.out(), "None");
            case Jangine::Gfx::AntialiasingMode::Fsr:
                return format_to(ctx.out(), "Fsr");
            default:
                return format_to(ctx.out(), "Unknown AntialiasingMode");
            }
        }
    };
}