#pragma once

#include "../Shared.hpp"
#include "../Color.hpp"
#include "Enums.hpp"

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

typedef struct VkPipeline_T *VkPipeline;
typedef struct VkPipelineLayout_T *VkPipelineLayout;
typedef struct VkDescriptorSetLayout_T *VkDescriptorSetLayout;

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
                "DisplayParameters(renderWidth: {}, renderHeight: {}, displayWidth: {}, displayHeight: {}, displayFormat: {}, verticalSync: {}, viewportWidth: {}, viewportHeight: {}, antialiasingMode: {}, clearColor: {})",
                renderWidth,
                renderHeight,
                displayWidth,
                displayHeight,
                displayFormat,
                verticalSync ? "true" : "false",
                viewportWidth,
                viewportHeight,
                antialiasingMode,
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