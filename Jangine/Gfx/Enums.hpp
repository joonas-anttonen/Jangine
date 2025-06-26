#pragma once
#include <cstdint>
#include <format>

namespace Jangine::Gfx
{
    enum class ImageLayout : uint32_t
    {
        UNDEFINED = 0,
        GENERAL = 1,
        COLOR_ATTACHMENT_OPTIMAL = 2,
        DEPTH_STENCIL_ATTACHMENT_OPTIMAL = 3,
        DEPTH_STENCIL_READ_ONLY_OPTIMAL = 4,
        SHADER_READ_ONLY_OPTIMAL = 5,
        TRANSFER_SRC_OPTIMAL = 6,
        TRANSFER_DST_OPTIMAL = 7,
        PREINITIALIZED = 8,
        DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL = 1000117000,
        DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL = 1000117001,
        DEPTH_ATTACHMENT_OPTIMAL = 1000241000,
        DEPTH_READ_ONLY_OPTIMAL = 1000241001,
        STENCIL_ATTACHMENT_OPTIMAL = 1000241002,
        STENCIL_READ_ONLY_OPTIMAL = 1000241003,
        READ_ONLY_OPTIMAL = 1000314000,
        ATTACHMENT_OPTIMAL = 1000314001,
        PRESENT_SRC_KHR = 1000001002,
        VIDEO_DECODE_DST_KHR = 1000024000,
        VIDEO_DECODE_SRC_KHR = 1000024001,
        VIDEO_DECODE_DPB_KHR = 1000024002,
        SHARED_PRESENT_KHR = 1000111000,
        FRAGMENT_DENSITY_MAP_OPTIMAL_EXT = 1000218000,
        FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR = 1000164003,
        RENDERING_LOCAL_READ_KHR = 1000232000,
        VIDEO_ENCODE_DST_KHR = 1000299000,
        VIDEO_ENCODE_SRC_KHR = 1000299001,
        VIDEO_ENCODE_DPB_KHR = 1000299002,
        ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT = 1000339000,
    };

    enum class ImageFilter : uint32_t
    {
        Nearest,
        Linear,
    };

    enum class ImageFit
    {
        None,
        Center,
        Fill,
        FillAspect,
    };

    enum class SamplerAddressMode : uint32_t
    {
        REPEAT = 0,
        MIRRORED_REPEAT = 1,
        CLAMP_TO_EDGE = 2,
        CLAMP_TO_BORDER = 3,
        MIRROR_CLAMP_TO_EDGE = 4,
    };

    enum class BorderColor : uint32_t
    {
        FLOAT_TRANSPARENT_BLACK = 0,
        INT_TRANSPARENT_BLACK = 1,
        FLOAT_OPAQUE_BLACK = 2,
        INT_OPAQUE_BLACK = 3,
        FLOAT_OPAQUE_WHITE = 4,
        INT_OPAQUE_WHITE = 5,
        FLOAT_CUSTOM_EXT = 1000287003,
        INT_CUSTOM_EXT = 1000287004,
    };

    enum class SamplerFilter : uint32_t
    {
        NEAREST = 0,
        LINEAR = 1,
    };

    enum class SamplerMipmapMode : uint32_t
    {
        NEAREST = 0,
        LINEAR = 1,
    };

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

    enum class ShaderStage : uint32_t
    {
        VERTEX = 0x00000001,
        TESSELLATION_CONTROL_BIT = 0x00000002,
        TESSELLATION_EVALUATION_BIT = 0x00000004,
        GEOMETRY = 0x00000008,
        FRAGMENT = 0x00000010,
        COMPUTE = 0x00000020,
        ALL_GRAPHICS = 0x0000001F,
        ALL = 0x7FFFFFFF,
        RAYGEN_BIT_KHR = 0x00000100,
        ANY_HIT_BIT_KHR = 0x00000200,
        CLOSEST_HIT_BIT_KHR = 0x00000400,
        MISS_BIT_KHR = 0x00000800,
        INTERSECTION_BIT_KHR = 0x00001000,
        CALLABLE_BIT_KHR = 0x00002000,
        TASK_BIT_EXT = 0x00000040,
        MESH_BIT_EXT = 0x00000080,
    };

    inline ShaderStage operator|(ShaderStage a, ShaderStage b)
    {
        return static_cast<ShaderStage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    enum class DescriptorType : uint32_t
    {
        SAMPLER = 0,
        COMBINED_IMAGE_SAMPLER = 1,
        SAMPLED_IMAGE = 2,
        STORAGE_IMAGE = 3,
        UNIFORM_TEXEL_BUFFER = 4,
        STORAGE_TEXEL_BUFFER = 5,
        UNIFORM_BUFFER = 6,
        STORAGE_BUFFER = 7,
        UNIFORM_BUFFER_DYNAMIC = 8,
        STORAGE_BUFFER_DYNAMIC = 9,
        INPUT_ATTACHMENT = 10,
        INLINE_UNIFORM_BLOCK = 1000138000,
        ACCELERATION_STRUCTURE_KHR = 1000150000,
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

    inline MemoryUsage operator&(MemoryUsage a, MemoryUsage b)
    {
        return static_cast<MemoryUsage>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
    }

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

    inline MemoryAccess operator&(MemoryAccess a, MemoryAccess b)
    {
        return static_cast<MemoryAccess>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
    }

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

    inline PixelBufferUsage operator&(PixelBufferUsage a, PixelBufferUsage b)
    {
        return static_cast<PixelBufferUsage>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
    }

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

    enum class CompareOp : uint32_t
    {
        NEVER = 0,
        LESS = 1,
        EQUAL = 2,
        LESS_OR_EQUAL = 3,
        GREATER = 4,
        NOT_EQUAL = 5,
        GREATER_OR_EQUAL = 6,
        ALWAYS = 7,
    };

    enum class FrontFace : uint32_t
    {
        COUNTER_CLOCKWISE = 0,
        CLOCKWISE = 1,
    };

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

    enum class CullMode : uint32_t
    {
        NONE = 0,
        FRONT = 0x00000001,
        BACK = 0x00000002,
        FRONT_AND_BACK = 0x00000003,
    };

    enum class PrimitiveTopology : uint32_t
    {
        POINT_LIST = 0,
        LINE_LIST = 1,
        LINE_STRIP = 2,
        TRIANGLE_LIST = 3,
        TRIANGLE_STRIP = 4,
        TRIANGLE_FAN = 5,
        LINE_LIST_WITH_ADJACENCY = 6,
        LINE_STRIP_WITH_ADJACENCY = 7,
        TRIANGLE_LIST_WITH_ADJACENCY = 8,
        TRIANGLE_STRIP_WITH_ADJACENCY = 9,
        PATCH_LIST = 10,
    };

    enum class DynamicState : uint32_t
    {
        VIEWPORT = 0,
        SCISSOR = 1,
        LINE_WIDTH = 2,
        DEPTH_BIAS = 3,
        BLEND_CONSTANTS = 4,
        DEPTH_BOUNDS = 5,
        STENCIL_COMPARE_MASK = 6,
        STENCIL_WRITE_MASK = 7,
        STENCIL_REFERENCE = 8,
        CULL_MODE = 1000267000,
        FRONT_FACE = 1000267001,
        PRIMITIVE_TOPOLOGY = 1000267002,
        VIEWPORT_WITH_COUNT = 1000267003,
        SCISSOR_WITH_COUNT = 1000267004,
        VERTEX_INPUT_BINDING_STRIDE = 1000267005,
        DEPTH_TEST_ENABLE = 1000267006,
        DEPTH_WRITE_ENABLE = 1000267007,
        DEPTH_COMPARE_OP = 1000267008,
        DEPTH_BOUNDS_TEST_ENABLE = 1000267009,
        STENCIL_TEST_ENABLE = 1000267010,
        STENCIL_OP = 1000267011,
        RASTERIZER_DISCARD_ENABLE = 1000377001,
        DEPTH_BIAS_ENABLE = 1000377002,
    };

    enum class VertexInputRate : uint32_t
    {
        VERTEX = 0,
        INSTANCE = 1,
    };

    enum class BlendFactor : uint32_t
    {
        ZERO = 0,
        ONE = 1,
        SRC_COLOR = 2,
        ONE_MINUS_SRC_COLOR = 3,
        DST_COLOR = 4,
        ONE_MINUS_DST_COLOR = 5,
        SRC_ALPHA = 6,
        ONE_MINUS_SRC_ALPHA = 7,
        DST_ALPHA = 8,
        ONE_MINUS_DST_ALPHA = 9,
        CONSTANT_COLOR = 10,
        ONE_MINUS_CONSTANT_COLOR = 11,
        CONSTANT_ALPHA = 12,
        ONE_MINUS_CONSTANT_ALPHA = 13,
        SRC_ALPHA_SATURATE = 14,
        SRC1_COLOR = 15,
        ONE_MINUS_SRC1_COLOR = 16,
        SRC1_ALPHA = 17,
        ONE_MINUS_SRC1_ALPHA = 18,
    };

    enum class BlendOp : uint32_t
    {
        ADD = 0,
        SUBTRACT = 1,
        REVERSE_SUBTRACT = 2,
        MIN = 3,
        MAX = 4,
        ZERO_EXT = 1000148000,
        SRC_EXT = 1000148001,
        DST_EXT = 1000148002,
        SRC_OVER_EXT = 1000148003,
        DST_OVER_EXT = 1000148004,
        SRC_IN_EXT = 1000148005,
        DST_IN_EXT = 1000148006,
        SRC_OUT_EXT = 1000148007,
        DST_OUT_EXT = 1000148008,
        SRC_ATOP_EXT = 1000148009,
        DST_ATOP_EXT = 1000148010,
        XOR_EXT = 1000148011,
        MULTIPLY_EXT = 1000148012,
        SCREEN_EXT = 1000148013,
        OVERLAY_EXT = 1000148014,
        DARKEN_EXT = 1000148015,
        LIGHTEN_EXT = 1000148016,
        COLORDODGE_EXT = 1000148017,
        COLORBURN_EXT = 1000148018,
        HARDLIGHT_EXT = 1000148019,
        SOFTLIGHT_EXT = 1000148020,
        DIFFERENCE_EXT = 1000148021,
        EXCLUSION_EXT = 1000148022,
        INVERT_EXT = 1000148023,
        INVERT_RGB_EXT = 1000148024,
        LINEARDODGE_EXT = 1000148025,
        LINEARBURN_EXT = 1000148026,
        VIVIDLIGHT_EXT = 1000148027,
        LINEARLIGHT_EXT = 1000148028,
        PINLIGHT_EXT = 1000148029,
        HARDMIX_EXT = 1000148030,
        HSL_HUE_EXT = 1000148031,
        HSL_SATURATION_EXT = 1000148032,
        HSL_COLOR_EXT = 1000148033,
        HSL_LUMINOSITY_EXT = 1000148034,
        PLUS_EXT = 1000148035,
        PLUS_CLAMPED_EXT = 1000148036,
        PLUS_CLAMPED_ALPHA_EXT = 1000148037,
        PLUS_DARKER_EXT = 1000148038,
        MINUS_EXT = 1000148039,
        MINUS_CLAMPED_EXT = 1000148040,
        CONTRAST_EXT = 1000148041,
        INVERT_OVG_EXT = 1000148042,
        RED_EXT = 1000148043,
        GREEN_EXT = 1000148044,
        BLUE_EXT = 1000148045,
    };

    enum class ColorComponent : uint32_t
    {
        R = 0x00000001,
        G = 0x00000002,
        B = 0x00000004,
        A = 0x00000008,
    };

    inline ColorComponent operator|(ColorComponent a, ColorComponent b)
    {
        return static_cast<ColorComponent>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

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

    template <>
    struct formatter<Jangine::Gfx::MemoryUsage>
    {
        constexpr auto parse(format_parse_context &ctx) { return ctx.begin(); }

        template <typename FormatContext>
        auto format(Jangine::Gfx::MemoryUsage memoryUsage, FormatContext &ctx) const
        {
            switch (memoryUsage)
            {
            case Jangine::Gfx::MemoryUsage::None:
                return format_to(ctx.out(), "None");
            case Jangine::Gfx::MemoryUsage::TransferSrc:
                return format_to(ctx.out(), "TransferSrc");
            case Jangine::Gfx::MemoryUsage::TransferDst:
                return format_to(ctx.out(), "TransferDst");
            case Jangine::Gfx::MemoryUsage::Storage:
                return format_to(ctx.out(), "Storage");
            case Jangine::Gfx::MemoryUsage::Uniform:
                return format_to(ctx.out(), "Uniform");
            case Jangine::Gfx::MemoryUsage::Index:
                return format_to(ctx.out(), "Index");
            case Jangine::Gfx::MemoryUsage::Vertex:
                return format_to(ctx.out(), "Vertex");
            default:
                return format_to(ctx.out(), "Unknown MemoryUsage");
            }
        }
    };
}