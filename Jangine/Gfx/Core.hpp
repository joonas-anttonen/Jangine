#pragma once

#include "../Shared.hpp"
#include "../Core.hpp"

#include <array>
#include <vector>
#include <span>
#include <string>

// Forward declarations for Vulkan handles
typedef struct VkInstance_T *VkInstance;
typedef struct VkDevice_T *VkDevice;
typedef struct VkPhysicalDevice_T *VkPhysicalDevice;
typedef struct VkQueue_T *VkQueue;
typedef struct VkCommandPool_T *VkCommandPool;
typedef struct VkQueryPool_T *VkQueryPool;
typedef struct VkDebugUtilsMessengerEXT_T *VkDebugUtilsMessengerEXT;

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

    struct Guid
    {
        std::array<uint8_t, 16> bytes{};
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
            return name + " [Vulkan: " + vulkan.ToString() + "] [Driver: " + driver.ToString() + "]";
        }
    };

    struct ApiParameters
    {
        bool_t enableDebugging = false;

        Version appVersion;
        Version appEngineVersion;
        Version requiredApiVersion;

        std::string appName;
        std::string appEngineName;
    };

    struct Parameters
    {
        PhysicalDevice physicalDevice;
    };

    class Core
    {
        struct ApiCapabilities
        {
            bool_t debugging;
            bool_t timestamps;
        };

    public:
        Core(const ApiParameters &params);
        ~Core();

        std::vector<PhysicalDevice> GetPhysicalDevices() const;

        PhysicalDevice SelectOptimalPhysicalDevice(std::span<const PhysicalDevice> physicalDevices) const
        {
            ThrowInvalidOperationIf(physicalDevices.empty(), "No physical devices available");

            PhysicalDevice optimalDevice = physicalDevices[0];

            for (const auto &device : physicalDevices)
            {
                if (device.type == PhysicalDeviceType::Discrete)
                {
                    return device;
                }
                if (device.type == PhysicalDeviceType::Integrated)
                {
                    optimalDevice = device;
                }
            }

            return optimalDevice;
        }

        void Create(const Parameters &params);

    private:
        void CreateInstance(const ApiParameters &params);
        void CreateDevice(const Parameters &params);
        void CreateQueryPool(uint32_t capacity);

        void ResolveDeviceSampleCount();
        void ResolveDeviceDepthFormat();

        VkInstance instance = nullptr;
        VkDevice device = nullptr;
        VkPhysicalDevice physicalDevice = nullptr;
        VkQueue generalQueue = nullptr;
        uint32_t generalQueueFamilyIndex = 0;
        VkCommandPool commandPool = nullptr;
        VkQueryPool queryPool = nullptr;
        VkDebugUtilsMessengerEXT debugMessenger = nullptr;

        const Logging::Logger &logger;

        ApiCapabilities capabilities;

        Samples deviceSampleCount = Samples::X1;
        Format deviceDepthFormat = Format::Undefined;
    };
}
