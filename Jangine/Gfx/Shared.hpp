#pragma once

#include "../Shared.hpp"
#include "../Color.hpp"

#include <cstdint>
#include <optional>
#include <format>
#include <string>

// Forward declarations for Vulkan handles
typedef struct VkInstance_T *VkInstance;
typedef struct VkDevice_T *VkDevice;
typedef struct VkPhysicalDevice_T *VkPhysicalDevice;
typedef struct VkQueue_T *VkQueue;
typedef struct VkCommandPool_T *VkCommandPool;
typedef struct VkQueryPool_T *VkQueryPool;
typedef struct VkDebugUtilsMessengerEXT_T *VkDebugUtilsMessengerEXT;

typedef struct VkFence_T *VkFence;
typedef struct VkSemaphore_T *VkSemaphore;

typedef struct VkSurfaceKHR_T *VkSurfaceKHR;
typedef struct VkSwapchainKHR_T *VkSwapchainKHR;

typedef struct VkCommandBuffer_T *VkCommandBuffer;
typedef struct VkImage_T *VkImage;
typedef struct VkImageView_T *VkImageView;

typedef struct VmaAllocator_T *VmaAllocator;
typedef struct VmaAllocation_T *VmaAllocation;

namespace Jangine::Gfx
{
    enum class Format : int
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

    struct Extent
    {
        uint32_t Width = 0;
        uint32_t Height = 0;

        bool operator==(const Extent &other) const
        {
            return Width == other.Width && Height == other.Height;
        }

        bool operator!=(const Extent &other) const
        {
            return !(*this == other);
        }
    };

    enum class AntialiasingMode
    {
        None,
        Fsr
    };

    enum class UpscalingMode
    {
        None,
        Quality,
        Balanced,
        Performance,
        UltraPerformance
    };

    struct DisplayParameters
    {
        uint32_t renderWidth = 0;
        uint32_t renderHeight = 0;
        uint32_t displayWidth = 0;
        uint32_t displayHeight = 0;
        uint32_t displayRefreshRate = 0;
        Format displayFormat = Format::Undefined;
        bool_t verticalSync = false;
        uint32_t viewportWidth = 0;
        uint32_t viewportHeight = 0;
        AntialiasingMode antialiasingMode = AntialiasingMode::None;
        Color clearColor = Color::FromUInt(0x232731);

        static float GetRenderScaleFactor(AntialiasingMode aaMode, UpscalingMode upscaleMode)
        {
            if (aaMode == AntialiasingMode::Fsr)
            {
                switch (upscaleMode)
                {
                case UpscalingMode::Quality:
                    return 1.0f / 1.5f;
                case UpscalingMode::Balanced:
                    return 1.0f / 1.7f;
                case UpscalingMode::Performance:
                    return 1.0f / 2.0f;
                case UpscalingMode::UltraPerformance:
                    return 1.0f / 3.0f;
                default:
                    return 1.0f;
                }
            }
            return 1.0f;
        }

        std::string ToString() const
        {
            return std::format(
                "DisplayParameters(renderWidth: {}, renderHeight: {}, displayWidth: {}, displayHeight: {}, verticalSync: {}, viewportWidth: {}, viewportHeight: {}, clearColor: {})",
                renderWidth,
                renderHeight,
                displayWidth,
                displayHeight,
                verticalSync ? "true" : "false",
                viewportWidth,
                viewportHeight,
                clearColor.ToHexString());
        }

        bool_t RenderSizeChanged(const DisplayParameters &other) const
        {
            return renderWidth != other.renderWidth || renderHeight != other.renderHeight;
        }

        bool_t DisplaySizeChanged(const DisplayParameters &other) const
        {
            return displayWidth != other.displayWidth || displayHeight != other.displayHeight;
        }

        bool_t DisplayFormatChanged(const DisplayParameters &other) const
        {
            return displayFormat != other.displayFormat;
        }

        bool_t DisplayVerticalSyncChanged(const DisplayParameters &other) const
        {
            return verticalSync != other.verticalSync;
        }

        bool_t DisplaySizeChanged(uint32_t width, uint32_t height) const
        {
            return displayWidth != width || displayHeight != height;
        }

        bool_t DisplaySizeChanged(const Extent &extent) const
        {
            return displayWidth != extent.Width || displayHeight != extent.Height;
        }

        bool_t DisplayFormatChanged(Format format) const
        {
            return displayFormat != format;
        }
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

    enum class Samples : int
    {
        X1 = 1,   // = VK_SAMPLE_COUNT_1_BIT
        X2 = 2,   // = VK_SAMPLE_COUNT_2_BIT
        X4 = 4,   // = VK_SAMPLE_COUNT_4_BIT
        X8 = 8,   // = VK_SAMPLE_COUNT_8_BIT
        X16 = 16, // = VK_SAMPLE_COUNT_16_BIT
        X32 = 32, // = VK_SAMPLE_COUNT_32_BIT
        X64 = 64  // = VK_SAMPLE_COUNT_64_BIT
    };

    enum class PhysicalDeviceType
    {
        Discrete,
        Integrated,
        Virtual,
        Cpu,
        Other
    };

    class Core;
    class PixelBuffer
    {
        friend class Core;

    private:
        PixelBuffer(
            uint32_t width,
            uint32_t height,
            Format format,
            PixelBufferUsage usage,
            Aspect aspect,
            Samples samples,
            VkImage vulkanImage,
            VkImageView vulkanImageView,
            VmaAllocation vulkanAllocation)
            : width(width),
              height(height),
              format(format),
              usage(usage),
              aspect(aspect),
              samples(samples),
              vulkanImage(vulkanImage),
              vulkanImageView(vulkanImageView),
              vulkanAllocation(vulkanAllocation) {}

    public:
        PixelBuffer(const PixelBuffer &) = delete;
        PixelBuffer &operator=(const PixelBuffer &) = delete;
        PixelBuffer(PixelBuffer &&) = delete;
        PixelBuffer &operator=(PixelBuffer &&) = delete;

        uint32_t GetWidth() const { return width; }
        uint32_t GetHeight() const { return height; }
        Format GetFormat() const { return format; }
        PixelBufferUsage GetUsage() const { return usage; }
        Aspect GetAspect() const { return aspect; }
        Samples GetSamples() const { return samples; }

    private:
        const uint32_t width;
        const uint32_t height;
        const Format format;
        const PixelBufferUsage usage;
        const Aspect aspect;
        const Samples samples;

        const VkImage vulkanImage;
        const VkImageView vulkanImageView;

        const VmaAllocation vulkanAllocation;
    };
}