#pragma once

#include "../Shared.hpp"
#include "Shared.hpp"

#include <memory>

namespace Jangine::Logging
{
    class Logger;
}

namespace Jangine::Gfx
{
    class Core;

    class Presenter
    {
    public:
        struct Image
        {
            uint32_t width;
            uint32_t height;
            
            VkImage vulkanImage = nullptr;
            VkImageView vulkanImageView = nullptr;
        };

    private:
        struct PerFrame
        {
            VkCommandPool commandPool = nullptr;
            VkCommandBuffer commandBuffer = nullptr;
            VkFence submitFence = nullptr;
            VkSemaphore submitSemaphore = nullptr;
            Image outputImage;
        };

    public:
        Presenter(VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice, SpinLock &queueLock,
                  VkQueue queue, uint32_t queueFamilyIndex, VkSurfaceKHR surface);

        ~Presenter();

        void InitializeRendering(const DisplayParameters &displayParameters);
        [[nodiscard]] bool_t BeginFrame();
        void EndFrame();

        [[nodiscard]] inline CommandBuffer GetCurrentCommandBuffer() const
        {
            ThrowInvalidOperationIfNot(frameInProgress);
            return CommandBuffer(perFrameResources[currentFrameIndex].commandBuffer);
        }

        [[nodiscard]] inline Image GetCurrentPresentationBuffer() const
        {
            ThrowInvalidOperationIfNot(frameInProgress);
            return perFrameResources[currentFrameIndex].outputImage;
        }

    private:
        void
        ReleaseSwapChainIfAny();
        void ReleasePerFrame();

        [[nodiscard]] int32_t AcquireNextImage();
        void InitializeSwapchain();

        VkInstance vulkanInstance = nullptr;
        VkDevice vulkanDevice = nullptr;
        VkPhysicalDevice vulkanPhysicalDevice = nullptr;
        SpinLock &vulkanQueueLock;
        VkQueue vulkanQueue = nullptr;
        uint32_t vulkanQueueFamilyIndex = 0;

        VkSurfaceKHR vulkanSurface = nullptr;
        VkSwapchainKHR vulkanSwapchain = nullptr;
        uint32_t swapChainFormat;

        VkFence vulkanAcquireFence = nullptr;

        bool_t frameInProgress = false;
        uint32_t currentFrameIndex = 0;

        std::vector<PerFrame> perFrameResources;

        DisplayParameters displayParameters;

        const Logging::Logger &logger;
    };
}