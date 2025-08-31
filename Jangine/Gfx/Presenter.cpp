#include "Presenter.hpp"
#include "Core.hpp"

#include <algorithm>
#include <vulkan/vulkan.h>

namespace Jangine::Gfx
{
    static void TransitionImageLayout(
        const Presenter::Image &image,
        VkCommandBuffer commandBuffer,
        VkImageLayout srcLayout,
        VkImageLayout dstLayout,
        VkPipelineStageFlags srcStage,
        VkPipelineStageFlags dstStage)
    {
        VkAccessFlags srcAccess = 0;
        switch (srcLayout)
        {
        case VK_IMAGE_LAYOUT_UNDEFINED:
            srcAccess = 0;
            break;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            srcAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            srcAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;
        default:
            throw std::runtime_error("Unsupported source layout.");
        }

        VkAccessFlags dstAccess = 0;
        switch (dstLayout)
        {
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            dstAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            dstAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
            dstAccess = 0;
            break;
        default:
            throw std::runtime_error("Unsupported destination layout.");
        }

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcAccessMask = srcAccess;
        barrier.dstAccessMask = dstAccess;
        barrier.oldLayout = srcLayout;
        barrier.newLayout = dstLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image.vulkanImage;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(
            commandBuffer,
            srcStage, dstStage,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier);
    }

    static void ThrowVulkanIfFailed(VkResult result, const std::string &message = "")
    {
        if (result != VK_SUCCESS)
        {
            throw Jangine::JangineException(message + " - Error code: " + std::to_string(result));
        }
    }

    Presenter::Presenter(VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice, SpinLock &queueLock,
                         VkQueue queue, uint32_t queueFamilyIndex, VkSurfaceKHR surface)
        : vulkanInstance(instance), vulkanDevice(device), vulkanPhysicalDevice(physicalDevice),
          vulkanQueueLock(queueLock), vulkanQueue(queue), vulkanQueueFamilyIndex(queueFamilyIndex),
          vulkanSurface(surface), vulkanSwapchain(VK_NULL_HANDLE),
          logger(Jangine::Core::GetLogger("Gfx::Presenter"))
    {
        logger.Func(__func__);

        VkFenceCreateInfo fenceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0};
        ThrowVulkanIfFailed(vkCreateFence(vulkanDevice, &fenceCreateInfo, nullptr, &vulkanAcquireFence));
    }

    Presenter::~Presenter()
    {
        logger.Func(__func__);

        ReleasePerFrame();

        if (vulkanAcquireFence)
        {
            vkDestroyFence(vulkanDevice, vulkanAcquireFence, nullptr);
            vulkanAcquireFence = nullptr;
        }

        if (vulkanSwapchain)
        {
            vkDestroySwapchainKHR(vulkanDevice, vulkanSwapchain, nullptr);
            vulkanSwapchain = nullptr;
        }

        if (vulkanSurface)
        {
            vkDestroySurfaceKHR(vulkanInstance, vulkanSurface, nullptr);
            vulkanSurface = nullptr;
        }
    }

    void Presenter::InitializeRendering(const DisplayParameters &wantedDisplayParameters)
    {
        DisplayParameters currentDisplayParameters = displayParameters;
        displayParameters = wantedDisplayParameters;

        if (currentDisplayParameters.SurfaceSizeChanged(wantedDisplayParameters) ||
            currentDisplayParameters.SurfaceFormatChanged(wantedDisplayParameters) ||
            currentDisplayParameters.VerticalSyncChanged(wantedDisplayParameters))
        {
            InitializeSwapchain();
        }
    }

    bool_t Presenter::BeginFrame()
    {
        ThrowInvalidOperationIf(frameInProgress);

        uint32_t result = AcquireNextImage();
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
        {
            InitializeSwapchain();
            result = AcquireNextImage();
        }
        if (result != VK_SUCCESS)
        {
            std::scoped_lock lock(vulkanQueueLock);
            vkQueueWaitIdle(vulkanQueue);
            return false;
        }

        auto &frame = perFrameResources[currentFrameIndex];
        if (!frame.outputImage.vulkanImage)
            throw std::runtime_error("Output image is null!");

        VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VkCommandBuffer commandBuffer = frame.commandBuffer;
        ThrowVulkanIfFailed(vkBeginCommandBuffer(commandBuffer, &beginInfo), "vkBeginCommandBuffer failed");

        TransitionImageLayout(
            frame.outputImage,
            commandBuffer,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT);

        // Clear the output image
        VkClearColorValue clearColor = {
            displayParameters.clearColor.r,
            displayParameters.clearColor.g,
            displayParameters.clearColor.b,
            displayParameters.clearColor.a};
        VkImageSubresourceRange subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1};
        vkCmdClearColorImage(
            commandBuffer,
            frame.outputImage.vulkanImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            &clearColor,
            1,
            &subresourceRange);

        frameInProgress = true;
        return true;
    }

    void Presenter::EndFrame()
    {
        ThrowInvalidOperationIfNot(frameInProgress);

        auto &frame = perFrameResources[currentFrameIndex];
        if (!frame.outputImage.vulkanImage)
            throw std::runtime_error("Output image is null!");

        uint32_t localCurrentFrameIndex = currentFrameIndex;
        VkSwapchainKHR localSwapchain = vulkanSwapchain;
        VkSemaphore localSubmitSemaphore = frame.submitSemaphore;
        VkCommandBuffer localCommandBuffer = frame.commandBuffer;

        TransitionImageLayout(
            frame.outputImage,
            localCommandBuffer,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

        ThrowVulkanIfFailed(vkEndCommandBuffer(localCommandBuffer), "vkEndCommandBuffer failed");

        std::scoped_lock lock(vulkanQueueLock);

        VkPipelineStageFlags pipelineStages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submitInfo.pWaitDstStageMask = &pipelineStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &localCommandBuffer;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &localSubmitSemaphore;

        ThrowVulkanIfFailed(vkQueueSubmit(vulkanQueue, 1, &submitInfo, frame.submitFence), "vkQueueSubmit failed");

        VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &localSubmitSemaphore;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &localSwapchain;
        presentInfo.pImageIndices = &localCurrentFrameIndex;

        VkResult result = vkQueuePresentKHR(vulkanQueue, &presentInfo);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
        {
            logger.Warning("vkQueuePresentKHR returned VK_ERROR_OUT_OF_DATE_KHR or VK_SUBOPTIMAL_KHR, reinitializing swapchain.");
            InitializeSwapchain();
        }
        else if (result != VK_SUCCESS)
        {
            ThrowVulkanIfFailed(result, "vkQueuePresentKHR failed");
        }

        frameInProgress = false;
    }

    void Presenter::ReleaseSwapChainIfAny()
    {
        if (vulkanSwapchain)
        {
            ReleasePerFrame();

            vkDestroySwapchainKHR(vulkanDevice, vulkanSwapchain, nullptr);
            vulkanSwapchain = nullptr;
        }
    }

    void Presenter::ReleasePerFrame()
    {
        for (int i = 0; i < perFrameResources.size(); i++)
        {
            const auto &frame = perFrameResources[i];

            if (frame.submitSemaphore)
            {
                vkDestroySemaphore(vulkanDevice, frame.submitSemaphore, nullptr);
            }
            if (frame.submitFence)
            {
                vkDestroyFence(vulkanDevice, frame.submitFence, nullptr);
            }
            if (frame.outputImage.vulkanImageView)
            {
                vkDestroyImageView(vulkanDevice, frame.outputImage.vulkanImageView, nullptr);
            }
            if (frame.commandBuffer)
            {
                VkCommandBuffer commandBuffer = frame.commandBuffer;
                vkFreeCommandBuffers(vulkanDevice, frame.commandPool, 1, &commandBuffer);
            }
            if (frame.commandPool)
            {
                vkDestroyCommandPool(vulkanDevice, frame.commandPool, nullptr);
            }

            perFrameResources[i] = {};
        }
    }

    int32_t Presenter::AcquireNextImage()
    {
        if (!vulkanSwapchain)
        {
            return VK_ERROR_OUT_OF_DATE_KHR;
        }

        auto &previousFrame = perFrameResources[currentFrameIndex];

        // Wait for the previous frame to finish
        if (previousFrame.submitFence)
        {
            ThrowVulkanIfFailed(
                vkWaitForFences(vulkanDevice, 1, &previousFrame.submitFence, VK_TRUE, UINT64_MAX),
                "Failed to wait for previous frame fence.");
        }

        // Try to acquire the next image
        while (true)
        {
            constexpr uint64_t nanoSecondsToWait = 1'000'000; // 1 ms

            uint32_t nextFrameIndex = 0;
            VkResult acquireResult = vkAcquireNextImageKHR(
                vulkanDevice,
                vulkanSwapchain,
                nanoSecondsToWait,
                VK_NULL_HANDLE,
                vulkanAcquireFence,
                &nextFrameIndex);

            if (acquireResult == VK_SUCCESS || acquireResult == VK_SUBOPTIMAL_KHR)
            {
                ThrowVulkanIfFailed(
                    vkWaitForFences(vulkanDevice, 1, &vulkanAcquireFence, VK_TRUE, UINT64_MAX),
                    "Failed to wait for acquire fence.");
                ThrowVulkanIfFailed(
                    vkResetFences(vulkanDevice, 1, &vulkanAcquireFence),
                    "Failed to reset acquire fence.");

                currentFrameIndex = nextFrameIndex;
                break;
            }
            else if (acquireResult == VK_TIMEOUT)
            {
                logger.Warning("vkAcquireNextImageKHR: TIMEOUT");
                continue;
            }
            else
            {
                // If vkAcquireNextImageKHR does not successfully acquire an image, semaphore and fence are unaffected.
                return acquireResult;
            }
        }

        auto &nextFrame = perFrameResources[currentFrameIndex];

        // Reset the submission fence
        if (nextFrame.submitFence)
        {
            ThrowVulkanIfFailed(
                vkWaitForFences(vulkanDevice, 1, &nextFrame.submitFence, VK_TRUE, UINT64_MAX),
                "Failed to wait for next frame fence.");
            ThrowVulkanIfFailed(
                vkResetFences(vulkanDevice, 1, &nextFrame.submitFence),
                "Failed to reset next frame fence.");
        }

        // Reset the command pool
        if (nextFrame.commandPool)
        {
            ThrowVulkanIfFailed(
                vkResetCommandPool(vulkanDevice, nextFrame.commandPool, 0),
                "Failed to reset command pool.");
        }

        return VK_SUCCESS;
    }

    void Presenter::InitializeSwapchain()
    {
        vkDeviceWaitIdle(vulkanDevice);

        VkSurfaceCapabilitiesKHR surfaceCapabilities{};
        ThrowVulkanIfFailed(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vulkanPhysicalDevice, vulkanSurface, &surfaceCapabilities));

        VkExtent2D swapChainExtent{};
        if (surfaceCapabilities.currentExtent.width == UINT32_MAX)
        {
            swapChainExtent.width = std::clamp(
                static_cast<uint32_t>(displayParameters.surfaceWidth),
                surfaceCapabilities.minImageExtent.width,
                surfaceCapabilities.maxImageExtent.width);
            swapChainExtent.height = std::clamp(
                static_cast<uint32_t>(displayParameters.surfaceHeight),
                surfaceCapabilities.minImageExtent.height,
                surfaceCapabilities.maxImageExtent.height);
        }
        else if (surfaceCapabilities.currentExtent.width > 0 && surfaceCapabilities.currentExtent.height > 0)
        {
            swapChainExtent = surfaceCapabilities.currentExtent;
        }
        else
        {
            ReleaseSwapChainIfAny();
            return;
        }

        uint32_t surfaceFormatCount = 0;
        ThrowVulkanIfFailed(vkGetPhysicalDeviceSurfaceFormatsKHR(vulkanPhysicalDevice, vulkanSurface, &surfaceFormatCount, nullptr));
        if (surfaceFormatCount == 0)
            throw std::runtime_error("No surface formats found.");

        std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCount);
        ThrowVulkanIfFailed(vkGetPhysicalDeviceSurfaceFormatsKHR(vulkanPhysicalDevice, vulkanSurface, &surfaceFormatCount, surfaceFormats.data()));

        VkSurfaceFormatKHR outputSurfaceFormat{};
        if (surfaceFormatCount == 1 && surfaceFormats[0].format == VK_FORMAT_UNDEFINED)
        {
            outputSurfaceFormat.format = VK_FORMAT_B8G8R8A8_UNORM;
            outputSurfaceFormat.colorSpace = surfaceFormats[0].colorSpace;
        }
        else
        {
            bool found = false;
            for (const auto &fmt : surfaceFormats)
            {
                if (fmt.format == VK_FORMAT_B8G8R8A8_UNORM)
                {
                    outputSurfaceFormat = fmt;
                    found = true;
                    break;
                }
            }
            if (!found)
                outputSurfaceFormat = surfaceFormats[0];
        }

        this->swapChainFormat = outputSurfaceFormat.format;

        uint32_t presentModeCount = 0;
        ThrowVulkanIfFailed(vkGetPhysicalDeviceSurfacePresentModesKHR(vulkanPhysicalDevice, vulkanSurface, &presentModeCount, nullptr));
        std::vector<VkPresentModeKHR> presentModes(presentModeCount);
        ThrowVulkanIfFailed(vkGetPhysicalDeviceSurfacePresentModesKHR(vulkanPhysicalDevice, vulkanSurface, &presentModeCount, presentModes.data()));

        VkPresentModeKHR swapChainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
        if (!displayParameters.verticalSync)
        {
            for (auto mode : presentModes)
            {
                if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
                {
                    swapChainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
                    break;
                }
                if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR)
                {
                    swapChainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
                }
            }
        }

        uint32_t desiredNumberOfSwapChainImages = 2;
        if (desiredNumberOfSwapChainImages < surfaceCapabilities.minImageCount)
            throw std::runtime_error("Requested number of swap chain images is too low.");

        VkImageUsageFlags desiredSwapChainImageUsage =
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
            VK_IMAGE_USAGE_SAMPLED_BIT |
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
            VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        if ((surfaceCapabilities.supportedUsageFlags & desiredSwapChainImageUsage) != desiredSwapChainImageUsage)
            throw std::runtime_error("Desired swap chain image usage is not supported.");

        VkSwapchainCreateInfoKHR swapChainCreateInfo{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        swapChainCreateInfo.surface = vulkanSurface;
        swapChainCreateInfo.minImageCount = desiredNumberOfSwapChainImages;
        swapChainCreateInfo.imageFormat = outputSurfaceFormat.format;
        swapChainCreateInfo.imageColorSpace = outputSurfaceFormat.colorSpace;
        swapChainCreateInfo.imageExtent = swapChainExtent;
        swapChainCreateInfo.imageArrayLayers = 1;
        swapChainCreateInfo.imageUsage = desiredSwapChainImageUsage;
        swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapChainCreateInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapChainCreateInfo.presentMode = swapChainPresentMode;
        swapChainCreateInfo.clipped = VK_TRUE;
        swapChainCreateInfo.oldSwapchain = vulkanSwapchain;

        VkSwapchainKHR newSwapchain = VK_NULL_HANDLE;
        ThrowVulkanIfFailed(vkCreateSwapchainKHR(vulkanDevice, &swapChainCreateInfo, nullptr, &newSwapchain));

        ReleaseSwapChainIfAny();
        vulkanSwapchain = newSwapchain;

        uint32_t swapChainImageCount = 0;
        ThrowVulkanIfFailed(vkGetSwapchainImagesKHR(vulkanDevice, vulkanSwapchain, &swapChainImageCount, nullptr));
        std::vector<VkImage> swapChainImages(swapChainImageCount);
        ThrowVulkanIfFailed(vkGetSwapchainImagesKHR(vulkanDevice, vulkanSwapchain, &swapChainImageCount, swapChainImages.data()));

        perFrameResources.resize(swapChainImageCount);

        for (uint32_t i = 0; i < swapChainImageCount; ++i)
        {
            VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.image = swapChainImages[i];
            viewInfo.format = outputSurfaceFormat.format;
            viewInfo.components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;

            VkImageView imageView = VK_NULL_HANDLE;
            ThrowVulkanIfFailed(vkCreateImageView(vulkanDevice, &viewInfo, nullptr, &imageView));

            // Create command pool
            VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
            poolInfo.queueFamilyIndex = vulkanQueueFamilyIndex;
            poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
            VkCommandPool commandPool = VK_NULL_HANDLE;
            ThrowVulkanIfFailed(vkCreateCommandPool(vulkanDevice, &poolInfo, nullptr, &commandPool));

            // Allocate command buffer
            VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
            allocInfo.commandPool = commandPool;
            allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandBufferCount = 1;
            VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
            ThrowVulkanIfFailed(vkAllocateCommandBuffers(vulkanDevice, &allocInfo, &commandBuffer));

            // Create fence
            VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
            fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            VkFence submitFence = VK_NULL_HANDLE;
            ThrowVulkanIfFailed(vkCreateFence(vulkanDevice, &fenceInfo, nullptr, &submitFence));

            // Create semaphore
            VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
            VkSemaphore submitSemaphore = VK_NULL_HANDLE;
            ThrowVulkanIfFailed(vkCreateSemaphore(vulkanDevice, &semaphoreInfo, nullptr, &submitSemaphore));

            // Fill per-frame resource
            perFrameResources[i] = PerFrame{
                .commandPool = commandPool,
                .commandBuffer = commandBuffer,
                .submitFence = submitFence,
                .submitSemaphore = submitSemaphore,
                .outputImage = {
                    .width = swapChainExtent.width,
                    .height = swapChainExtent.height,
                    .vulkanImage = swapChainImages[i],
                    .vulkanImageView = imageView}};
        }
    }
}