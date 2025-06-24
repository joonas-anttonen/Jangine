#pragma once

#include "../Shared.hpp"
#include "Shared.hpp"
#include "../Color.hpp"
#include "../Core.hpp"

#include <array>
#include <vector>
#include <span>
#include <string>

namespace Jangine::Gfx
{
    class Presenter;
    class Core2D;
    class Core3D;

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

        Core(const Core &) = delete;
        Core &operator=(const Core &) = delete;
        Core(Core &&) = delete;
        Core &operator=(Core &&) = delete;

        std::vector<PhysicalDevice> GetPhysicalDevices() const;

        PhysicalDevice SelectOptimalPhysicalDevice(std::span<const PhysicalDevice> physicalDevices) const
        {
            ThrowInvalidOperationIf(physicalDevices.empty(), "No physical devices available");

            PhysicalDevice optimalDevice = physicalDevices[0];

            for (const auto &d : physicalDevices)
            {
                if (d.type == PhysicalDeviceType::Discrete)
                {
                    return d;
                }
                if (d.type == PhysicalDeviceType::Integrated)
                {
                    optimalDevice = d;
                }
            }

            return optimalDevice;
        }

        void Create(const Parameters &params);
        void CreatePresenter(const Surface &surface);
        void_t *GetSurfaceCreationHandle() const
        {
            return reinterpret_cast<void_t *>(vulkanInstance);
        }

        void InitializeRendering(const DisplayParameters &displayParameters);

        const DisplayParameters &GetCurrentDisplayParameters()
        {
            std::lock_guard<SpinLock> lock(displayParametersLock);
            return currentDisplayParameters;
        }
        void SetDisplayParameters(const DisplayParameters &displayParameters)
        {
            std::lock_guard<SpinLock> lock(displayParametersLock);
            this->wantedDisplayParameters = displayParameters;

            // Print debug information
            logger.Debug(displayParameters.ToString(), __func__);
        }

        void Render(double_t absoluteTime, float_t deltaTime);

        Handle<PixelBuffer> CreatePixelBuffer(
            uint32_t width,
            uint32_t height,
            Format format,
            PixelBufferUsage usage,
            Aspect aspect = Aspect::Color,
            Samples samples = Samples::X1);

        void DestroyPixelBuffer(PixelBuffer *pixelBuffer);
        void operator()(PixelBuffer *pb) { DestroyPixelBuffer(pb); }

    private:
        void CreateInstance(const ApiParameters &params);
        void CreateDevice(const Parameters &params);
        void CreateQueryPool(uint32_t capacity);
        void CreateMemoryAllocator();

        void ResolveDeviceSampleCount();
        void ResolveDeviceDepthFormat();

        VkInstance vulkanInstance = nullptr;
        VkDevice vulkanDevice = nullptr;
        VkPhysicalDevice vulkanPhysicalDevice = nullptr;
        SpinLock vulkanQueueLock;
        VkQueue vulkanQueue = nullptr;
        uint32_t vulkanQueueFamilyIndex = 0;
        VkCommandPool vulkanCommandPool = nullptr;
        VkQueryPool vulkanQueryPool = nullptr;
        VkDebugUtilsMessengerEXT debugMessenger = nullptr;

        VmaAllocator vulkanMemoryAllocator = nullptr;
        size_t vulkanMemoryAllocatorAllocatedBytes = 0;

        bool_t pendingScreenCapture = false;

        Presenter *presenter = nullptr;
        Core2D *core2D = nullptr;
        Core3D *core3D = nullptr;

        const Logging::Logger &logger;

        ApiCapabilities capabilities;

        SpinLock displayParametersLock;
        std::optional<DisplayParameters> wantedDisplayParameters;
        DisplayParameters currentDisplayParameters = {
            .renderWidth = 1280,
            .renderHeight = 720,
        };

        Samples deviceSampleCount = Samples::X1;
        Format deviceDepthFormat = Format::Undefined;
    };
}
