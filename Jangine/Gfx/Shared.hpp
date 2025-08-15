#pragma once

#include <Eigen/Core>

#include "../Shared.hpp"
#include "../Color.hpp"
#include "Enums.hpp"

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
typedef struct VkSampler_T *VkSampler;
typedef struct VkBuffer_T *VkBuffer;
struct VkWriteDescriptorSet;

namespace Jangine::Gfx
{
    struct Surface
    {
        void_t *vulkanHandle = nullptr;
        uint32_t width = 0;
        uint32_t height = 0;
    };

    struct CommandBuffer
    {
        friend class Core;
        friend class Core2D;
        friend class Presenter;

    private:
        explicit CommandBuffer(VkCommandBuffer commandBuffer)
            : vulkanHandle(commandBuffer) {}

        VkCommandBuffer vulkanHandle;
    };

    struct PixelSampler
    {
        VkSampler vulkanHandle;
    };

    struct PixelSamplerParameters
    {
        SamplerFilter minFilter;
        SamplerFilter magFilter;
        SamplerMipmapMode mipmapMode;
        SamplerAddressMode addressModeU;
        SamplerAddressMode addressModeV;
        SamplerAddressMode addressModeW;
        uint32_t anisotropyEnable;
        float maxAnisotropy;
        BorderColor borderColor;
    };

    struct Pipeline
    {
        VkPipeline vulkanHandle;
        VkPipelineLayout vulkanLayout;
        VkDescriptorSetLayout vulkanDescriptorSetLayout;
    };

    struct ShaderProgram;

    struct PipelineParameters
    {
        struct VertexInputBinding
        {
            uint32_t binding;
            uint32_t stride;
            VertexInputRate inputRate;
        };

        struct VertexInputAttribute
        {
            uint32_t location;
            uint32_t binding;
            Format format;
            uint32_t offset;
        };

        struct AttachmentBlend
        {
            uint32_t blendEnable;
            BlendFactor srcColorBlendFactor;
            BlendFactor dstColorBlendFactor;
            BlendOp colorBlendOp;
            BlendFactor srcAlphaBlendFactor;
            BlendFactor dstAlphaBlendFactor;
            BlendOp alphaBlendOp;
            ColorComponent colorWriteMask;
        };

        struct Attachment
        {
            Format format;
            AttachmentBlend blend;
        };

        struct PushConstantRange
        {
            ShaderStage stageFlags;
            uint32_t offset;
            uint32_t size;
        };

        struct DescriptorBinding
        {
            uint32_t binding;
            DescriptorType descriptorType;
            uint32_t descriptorCount;
            ShaderStage stages;
            void_t *immutableSamplers;
        };

        const ShaderProgram *shaderProgram;

        PrimitiveTopology topology;
        FrontFace frontFace;
        CullMode cullMode;
        bool_t depthTestEnabled;
        bool_t depthWriteEnabled;
        CompareOp depthCompareOp;

        std::vector<VertexInputBinding> vertexInputBindings;
        std::vector<VertexInputAttribute> vertexInputAttributes;

        std::vector<Attachment> attachments;

        std::vector<PushConstantRange> pushConstantRanges;
        std::vector<DescriptorBinding> descriptorLayout;
    };

    struct PhysicalDevice
    {
        std::string name;
        Version vulkan;
        VkPhysicalDevice vulkanHandle;
        Version driver;
        PhysicalDeviceType type;
        Guid id;

        std::string ToString() const
        {
            return std::format("{} [Vulkan: {}] [Driver: {}]", name, vulkan.ToString(), driver.ToString());
        }
    };

    struct Vertex2f
    {
        Eigen::Vector2f position;
        Eigen::Vector2f uv;
        Eigen::Vector4f color;
    };

    struct Rectangle
    {
        float left, top, right, bottom;

        Rectangle() = default;
        Rectangle(float left, float top, float right, float bottom)
            : left(left), top(top), right(right), bottom(bottom) {}

        float width() const { return right - left; }
        float height() const { return bottom - top; }

        Eigen::Vector2f position() const { return {left, top}; }
        Eigen::Vector2f extent() const { return {width(), height()}; }
        Eigen::Vector2f center() const { return {left + width() / 2.0f, top + height() / 2.0f}; }

        bool Contains(const Eigen::Vector2f &p) const
        {
            return (left <= p.x()) && (top <= p.y()) && (right >= p.x()) && (bottom >= p.y());
        }

        Rectangle CenterOn(const Eigen::Vector2f &p) const
        {
            return FromXYWH(p.x() - width() / 2.0f, p.y() - height() / 2.0f, width(), height());
        }

        Rectangle CenterOn(const Rectangle &r) const
        {
            Eigen::Vector2f c = r.center();
            return FromXYWH(c.x() - width() / 2.0f, c.y() - height() / 2.0f, width(), height());
        }

        Rectangle Crop(float l, float t, float r, float b) const
        {
            return Rectangle(left + l, top + t, right - r, bottom - b);
        }

        Rectangle Scale(const Eigen::Vector2f &v) const
        {
            return Rectangle(left, top, left + width() * v.x(), top + height() * v.y());
        }

        Rectangle Scale(float x, float y) const
        {
            return Rectangle(left, top, left + width() * x, top + height() * y);
        }

        Rectangle Move(const Eigen::Vector2f &v) const
        {
            return FromXYWH(left + v.x(), top + v.y(), width(), height());
        }

        Rectangle Move(float x, float y) const
        {
            return FromXYWH(left + x, top + y, width(), height());
        }

        Rectangle Clamp(const Rectangle &other) const
        {
            return Rectangle(
                std::max(left, other.left),
                std::max(top, other.top),
                std::min(right, other.right),
                std::min(bottom, other.bottom));
        }

        static Rectangle FromPositionSize(const Eigen::Vector2f &position, const Eigen::Vector2f &size)
        {
            return Rectangle(position.x(), position.y(), position.x() + size.x(), position.y() + size.y());
        }

        static Rectangle FromXYWH(float x, float y, float w, float h)
        {
            return Rectangle(x, y, x + w, y + h);
        }

        static Rectangle FromLTRB(float l, float t, float r, float b)
        {
            return Rectangle(l, t, r, b);
        }
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

    struct DisplayParameters
    {
        uint32_t renderWidth = 0;
        uint32_t renderHeight = 0;
        uint32_t displayWidth = 0;
        uint32_t displayHeight = 0;
        uint32_t displayRefreshRate = 0;
        Format surfaceFormat = Format::Undefined;
        bool_t verticalSync = false;
        uint32_t surfaceWidth = 0;
        uint32_t surfaceHeight = 0;
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
                "render: {}x{}, display: {}x{}, surface: {}x{} [{}], vsync: {}",
                renderWidth,
                renderHeight,
                displayWidth,
                displayHeight,
                surfaceWidth,
                surfaceHeight,
                surfaceFormat,
                verticalSync ? "true" : "false");
        }

        bool_t AntialiasingModeChanged(const DisplayParameters &other) const
        {
            return antialiasingMode != other.antialiasingMode;
        }

        bool_t RenderSizeChanged(const DisplayParameters &other) const
        {
            return renderWidth != other.renderWidth || renderHeight != other.renderHeight;
        }

        bool_t SurfaceSizeChanged(const DisplayParameters &other) const
        {
            return surfaceWidth != other.surfaceWidth || surfaceHeight != other.surfaceHeight;
        }

        bool_t DisplaySizeChanged(const DisplayParameters &other) const
        {
            return displayWidth != other.displayWidth || displayHeight != other.displayHeight;
        }

        bool_t SurfaceFormatChanged(const DisplayParameters &other) const
        {
            return surfaceFormat != other.surfaceFormat;
        }

        bool_t VerticalSyncChanged(const DisplayParameters &other) const
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

        bool_t SurfaceFormatChanged(Format format) const
        {
            return surfaceFormat != format;
        }
    };

    class Core;
    class PixelBuffer
    {
        friend class Core;
        friend class Core2D;

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

        PixelBuffer(const PixelBuffer &) = delete;
        PixelBuffer &operator=(const PixelBuffer &) = delete;
        PixelBuffer(PixelBuffer &&) = delete;
        PixelBuffer &operator=(PixelBuffer &&) = delete;

    public:
        const uint32_t width;
        const uint32_t height;
        const Format format;

    private:
        const PixelBufferUsage usage;
        const Aspect aspect;
        const Samples samples;

        const VkImage vulkanImage;
        const VkImageView vulkanImageView;

        const VmaAllocation vulkanAllocation;
    };

    class MemoryBuffer
    {
        friend class Core;
        friend class Core2D;

    private:
        MemoryBuffer(
            uint32_t size,
            VkBuffer vulkanBuffer,
            VmaAllocation vulkanAllocation)
            : size(size),
              vulkanBuffer(vulkanBuffer),
              vulkanAllocation(vulkanAllocation) {}

        MemoryBuffer(const MemoryBuffer &) = delete;
        MemoryBuffer &operator=(const MemoryBuffer &) = delete;
        MemoryBuffer(MemoryBuffer &&) = delete;
        MemoryBuffer &operator=(MemoryBuffer &&) = delete;

    public:
        const uint32_t size;

    private:
        const VkBuffer vulkanBuffer;
        const VmaAllocation vulkanAllocation;
    };
}