#include "Core2d.hpp"
#include "Core.hpp"

// Disable warnings for external Vulkan header
#if defined(_MSC_VER)
#pragma warning(push, 0)
#elif defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#endif

#include <vulkan/vulkan.h>

#if defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace Jangine::Gfx
{
    Core2D::Core2D(Gfx::Core *gfx)
        : gfx(gfx),
          fontCollection(gfx),
          logger(Jangine::Core::GetLogger("Gfx::Core2D")),
          backBuffer(nullptr, std::ref(*gfx)),
          renderPipeline(nullptr, std::ref(*gfx)),
          compositePipeline(nullptr, std::ref(*gfx)),
          vertexBuffer(nullptr, std::ref(*gfx)),
          indexBuffer(nullptr, std::ref(*gfx)),
          nearestPixelSampler(nullptr, std::ref(*gfx)),
          linearPixelSampler(nullptr, std::ref(*gfx)),
          placeholderBuffer(nullptr, std::ref(*gfx))
    {
        logger.Func(__func__);

        InitializeCommandBufferPool();
    }

    Core2D::~Core2D()
    {
        logger.Func(__func__);
    }

    void Core2D::Create()
    {
        logger.Func(__func__);

        defaultFont = &fontCollection.GetFont(Text::FontKey("BuiltIn", 32));
        defaultShaper = defaultFont->CreateTextShaper();

        uint8_t data[4] = {255, 255, 255, 255};
        placeholderBuffer = gfx->CreatePixelBuffer(
            std::span<const std::byte>(reinterpret_cast<const std::byte *>(data), sizeof(data)),
            1,
            1,
            Format::RGBA8,
            PixelBufferUsage::Sampled);

        vertexBuffer = gfx->CreateMemoryBuffer(
            MAX_VERTICES * sizeof(Vertex2f),
            MemoryBufferUsage::Vertex,
            MemoryAccess::Write);

        indexBuffer = gfx->CreateMemoryBuffer(
            MAX_INDICES * sizeof(uint16_t),
            MemoryBufferUsage::Index,
            MemoryAccess::Write);

        PixelSamplerParameters nearestParameters = {
            .minFilter = SamplerFilter::NEAREST,
            .magFilter = SamplerFilter::NEAREST,
            .mipmapMode = SamplerMipmapMode::NEAREST,
            .addressModeU = SamplerAddressMode::CLAMP_TO_BORDER,
            .addressModeV = SamplerAddressMode::CLAMP_TO_BORDER,
            .addressModeW = SamplerAddressMode::CLAMP_TO_BORDER,
            .anisotropyEnable = false,
            .maxAnisotropy = 1.0f,
            .borderColor = BorderColor::FLOAT_OPAQUE_WHITE};
        nearestPixelSampler = gfx->CreatePixelSampler(nearestParameters);

        PixelSamplerParameters linearParameters = {
            .minFilter = SamplerFilter::LINEAR,
            .magFilter = SamplerFilter::LINEAR,
            .mipmapMode = SamplerMipmapMode::LINEAR,
            .addressModeU = SamplerAddressMode::CLAMP_TO_EDGE,
            .addressModeV = SamplerAddressMode::CLAMP_TO_EDGE,
            .addressModeW = SamplerAddressMode::CLAMP_TO_EDGE,
            .anisotropyEnable = false,
            .maxAnisotropy = 1.0f,
            .borderColor = BorderColor::FLOAT_OPAQUE_BLACK};
        linearPixelSampler = gfx->CreatePixelSampler(linearParameters);
    }

    void Core2D::InitializeRendering(const DisplayParameters &wantedDisplayParameters)
    {
        logger.Func(__func__);

        bool_t outputSizeChanged = displayParameters.SurfaceSizeChanged(wantedDisplayParameters);
        displayParameters = wantedDisplayParameters;

        bool_t recreateBackBuffer = !backBuffer || outputSizeChanged;
        if (recreateBackBuffer)
        {
            backBuffer = gfx->CreatePixelBuffer(
                displayParameters.surfaceWidth,
                displayParameters.surfaceHeight,
                displayParameters.surfaceFormat,
                PixelBufferUsage::ColorAttachment | PixelBufferUsage::Sampled |
                    PixelBufferUsage::TransferDst | PixelBufferUsage::TransferSrc);
        }

        PipelineParameters::AttachmentBlend straightAlphaBlend = {
            .blendEnable = true,
            .srcColorBlendFactor = BlendFactor::SRC_ALPHA,
            .dstColorBlendFactor = BlendFactor::ONE_MINUS_SRC_ALPHA,
            .colorBlendOp = BlendOp::ADD,
            .srcAlphaBlendFactor = BlendFactor::ONE,
            .dstAlphaBlendFactor = BlendFactor::ONE_MINUS_SRC_ALPHA,
            .alphaBlendOp = BlendOp::ADD,
            .colorWriteMask = ColorComponent::R | ColorComponent::G |
                              ColorComponent::B | ColorComponent::A};

        // Main 2D pipeline
        if (!renderPipeline)
        {
            PipelineParameters mainParams;
            mainParams.shaderProgram = ThrowInvalidOperationIfNull(gfx->GetShaderProgram("built-in-2d"),
                                                                   "Shader program 'built-in-2d' not found in cache.");
            mainParams.pushConstantRanges = {
                {.stageFlags = ShaderStage::VERTEX | ShaderStage::FRAGMENT,
                 .offset = 0,
                 .size = sizeof(PushConstants)}};
            mainParams.descriptorLayout = {
                {.binding = 0,
                 .descriptorType = DescriptorType::SAMPLED_IMAGE,
                 .descriptorCount = 1,
                 .stages = ShaderStage::FRAGMENT},
                {.binding = 1,
                 .descriptorType = DescriptorType::SAMPLER,
                 .descriptorCount = 1,
                 .stages = ShaderStage::FRAGMENT}};
            mainParams.topology = PrimitiveTopology::TRIANGLE_LIST;
            mainParams.cullMode = CullMode::NONE;
            mainParams.frontFace = FrontFace::COUNTER_CLOCKWISE;
            mainParams.depthTestEnabled = false;
            mainParams.depthWriteEnabled = false;
            mainParams.depthCompareOp = CompareOp::ALWAYS;
            mainParams.vertexInputBindings = {
                {.binding = 0,
                 .stride = sizeof(Vertex2f),
                 .inputRate = VertexInputRate::VERTEX}};
            mainParams.vertexInputAttributes = {
                {.location = 0,
                 .binding = 0,
                 .format = Format::RG32,
                 .offset = offsetof(Vertex2f, position)},
                {.location = 1,
                 .binding = 0,
                 .format = Format::RG32,
                 .offset = offsetof(Vertex2f, uv)},
                {.location = 2,
                 .binding = 0,
                 .format = Format::RGBA32,
                 .offset = offsetof(Vertex2f, color)}};
            mainParams.attachments = {
                {.format = Format::BGRA8,
                 .blend = straightAlphaBlend}};

            renderPipeline = gfx->CreatePipeline(mainParams);
        }

        // Composition pipeline (no vertex input)
        if (!compositePipeline)
        {
            PipelineParameters compositionParams;
            compositionParams.shaderProgram = ThrowInvalidOperationIfNull(gfx->GetShaderProgram("built-in-2d-composition"),
                                                                          "Shader program 'built-in-2d-composition' not found in cache.");
            compositionParams.pushConstantRanges = {};
            compositionParams.descriptorLayout = {
                {.binding = 0,
                 .descriptorType = DescriptorType::SAMPLED_IMAGE,
                 .descriptorCount = 1,
                 .stages = ShaderStage::FRAGMENT},
                {.binding = 1,
                 .descriptorType = DescriptorType::SAMPLER,
                 .descriptorCount = 1,
                 .stages = ShaderStage::FRAGMENT}};
            compositionParams.topology = PrimitiveTopology::TRIANGLE_LIST;
            compositionParams.cullMode = CullMode::NONE;
            compositionParams.frontFace = FrontFace::COUNTER_CLOCKWISE;
            compositionParams.depthTestEnabled = false;
            compositionParams.depthWriteEnabled = false;
            compositionParams.depthCompareOp = CompareOp::ALWAYS;
            compositionParams.vertexInputBindings = {};
            compositionParams.vertexInputAttributes = {};
            compositionParams.attachments = {
                {.format = Format::BGRA8,
                 .blend = straightAlphaBlend}};

            compositePipeline = gfx->CreatePipeline(compositionParams);
        }
    }

    void Core2D::Render(const Presenter &presenter, double_t absoluteTime, float_t deltaTime)
    {
        // Avoid unused parameter warning
        (void)absoluteTime; 
        (void)deltaTime;

        bool_t commandBufferChanged = false;

        {
            std::scoped_lock lock(commandBufferQueueLock);
            if (!commandBufferQueue.empty())
            {
                if (currentCommandBuffer)
                {
                    ReturnCommandBuffer(currentCommandBuffer);
                    currentCommandBuffer = nullptr;
                }

                commandBufferChanged = true;
                currentCommandBuffer = commandBufferQueue.front();
                commandBufferQueue.pop();
            }
        }

        if (!currentCommandBuffer)
        {
            return;
        }

        PrepareFrame(presenter);

        if (commandBufferChanged)
        {
            gfx->WriteMemoryBuffer(vertexBuffer.get(), currentCommandBuffer->GetVertexData());
            gfx->WriteMemoryBuffer(indexBuffer.get(), currentCommandBuffer->GetIndexData());
        }

        auto batches = currentCommandBuffer->GetBatches();
        for (const auto &batch : batches)
        {
            auto commands = currentCommandBuffer->GetBatchCommands(batch);
            RecordBatch(presenter.GetCurrentCommandBuffer(), commands, backBuffer.get());
        }

        FinishFrame(presenter);
    }

    void Core2D::RecordBatch(CommandBuffer commandBuffer, std::span<const CommandBuffer2D::Command> commands, PixelBuffer *targetBuffer)
    {
        VkCommandBuffer vulkanCommandBuffer = commandBuffer.vulkanHandle;

        VkRenderingAttachmentInfo colorAttachment{
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext = nullptr,
            .imageView = targetBuffer->vulkanImageView,
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .resolveMode = VK_RESOLVE_MODE_NONE,
            .resolveImageView = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = VkClearValue{.color = {{0.f, 0.f, 0.f, 0.f}}}};

        VkRenderingAttachmentInfo colorAttachments[] = {colorAttachment};

        VkExtent2D targetExtent = {.width = targetBuffer->width, .height = targetBuffer->height};

        VkRenderingInfo renderingInfo{
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .pNext = nullptr,
            .flags = 0,
            .renderArea = {{0, 0}, {targetExtent.width, targetExtent.height}},
            .layerCount = 1,
            .viewMask = 0,
            .colorAttachmentCount = 1,
            .pColorAttachments = colorAttachments,
            .pDepthAttachment = nullptr,
            .pStencilAttachment = nullptr};

        VkViewport viewport{
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float_t>(targetExtent.width),
            .height = static_cast<float_t>(targetExtent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f};
        VkRect2D scissor{{0, 0}, {targetExtent.width, targetExtent.height}};

        vkCmdBeginRendering(vulkanCommandBuffer, &renderingInfo);
        vkCmdBindPipeline(vulkanCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderPipeline->vulkanHandle);
        vkCmdSetViewport(vulkanCommandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(vulkanCommandBuffer, 0, 1, &scissor);

        VkDeviceSize vertexBufferOffset = 0;
        vkCmdBindVertexBuffers(vulkanCommandBuffer, 0, 1, &vertexBuffer->vulkanBuffer, &vertexBufferOffset);
        vkCmdBindIndexBuffer(vulkanCommandBuffer, indexBuffer->vulkanBuffer, 0, VK_INDEX_TYPE_UINT16);

        for (const auto &command : commands)
        {
            VkImageView commandTexture = placeholderBuffer->vulkanImageView;
            VkSampler commandSampler = nearestPixelSampler->vulkanHandle;

            if (command.texture)
            {
                commandTexture = command.texture->vulkanImageView;
                commandSampler = linearPixelSampler->vulkanHandle;
            }
            else if (command.font)
            {
                commandTexture = command.font->GetPixelBuffer()->vulkanImageView;
                commandSampler = linearPixelSampler->vulkanHandle;
            }

            PushConstants pushConstants{
                .scale = Eigen::Vector2f(2.0f / targetExtent.width, 2.0f / targetExtent.height),
                .smoothing = 1};

            VkDescriptorImageInfo imageInfo{
                .sampler = VK_NULL_HANDLE,
                .imageView = commandTexture ? commandTexture : VK_NULL_HANDLE,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            VkDescriptorImageInfo samplerInfo{
                .sampler = commandSampler,
                .imageView = VK_NULL_HANDLE,
                .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED};

            VkWriteDescriptorSet descriptorWrites[2] = {
                {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                 .pNext = nullptr,
                 .dstSet = VK_NULL_HANDLE,
                 .dstBinding = 0,
                 .dstArrayElement = 0,
                 .descriptorCount = 1,
                 .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                 .pImageInfo = &imageInfo,
                 .pBufferInfo = nullptr,
                 .pTexelBufferView = nullptr},
                {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                 .pNext = nullptr,
                 .dstSet = VK_NULL_HANDLE,
                 .dstBinding = 1,
                 .dstArrayElement = 0,
                 .descriptorCount = 1,
                 .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
                 .pImageInfo = &samplerInfo,
                 .pBufferInfo = nullptr,
                 .pTexelBufferView = nullptr}};

            gfx->PushDescriptorSets(
                commandBuffer,
                renderPipeline.get(),
                2,
                descriptorWrites);

            vkCmdPushConstants(
                vulkanCommandBuffer,
                renderPipeline->vulkanLayout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                sizeof(PushConstants),
                &pushConstants);

            vkCmdDrawIndexed(
                vulkanCommandBuffer,
                command.indexCount,
                1,
                command.indexOffset,
                0,
                0);
        }

        vkCmdEndRendering(vulkanCommandBuffer);

        // FIXME: Stop cheating!
        gfx->FullBarrier(commandBuffer);
    }

    void Core2D::PrepareFrame(const Presenter &presenter)
    {
        CommandBuffer commandBuffer = presenter.GetCurrentCommandBuffer();

        gfx->PixelBufferBarrier(commandBuffer,
                                backBuffer.get(),
                                ImageLayout::UNDEFINED,
                                ImageLayout::TRANSFER_DST_OPTIMAL);
        gfx->ClearPixelBuffer(commandBuffer,
                              backBuffer.get(),
                              Color::Transparent);
        gfx->PixelBufferBarrier(commandBuffer,
                                backBuffer.get(),
                                ImageLayout::TRANSFER_DST_OPTIMAL,
                                ImageLayout::COLOR_ATTACHMENT_OPTIMAL);
    }

    void Core2D::FinishFrame(const Presenter &presenter)
    {
        CommandBuffer commandBuffer = presenter.GetCurrentCommandBuffer();
        Presenter::Image presentationBuffer = presenter.GetCurrentPresentationBuffer();

        gfx->PixelBufferBarrier(commandBuffer,
                                presentationBuffer,
                                ImageLayout::TRANSFER_DST_OPTIMAL,
                                ImageLayout::COLOR_ATTACHMENT_OPTIMAL);
        gfx->PixelBufferBarrier(commandBuffer,
                                backBuffer.get(),
                                ImageLayout::COLOR_ATTACHMENT_OPTIMAL,
                                ImageLayout::SHADER_READ_ONLY_OPTIMAL);

        VkCommandBuffer vulkanCommandBuffer = commandBuffer.vulkanHandle;

        // Composite backBuffer to the presentation buffer using the composition pipeline
        VkRenderingAttachmentInfo colorAttachment{
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext = nullptr,
            .imageView = presentationBuffer.vulkanImageView,
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .resolveMode = VK_RESOLVE_MODE_NONE,
            .resolveImageView = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = VkClearValue{.color = {{0.f, 0.f, 0.f, 0.f}}}};

        VkRenderingAttachmentInfo colorAttachments[] = {colorAttachment};

        VkExtent2D extent = {.width = presentationBuffer.width, .height = presentationBuffer.height};
        VkRenderingInfo renderingInfo{
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .pNext = nullptr,
            .flags = 0,
            .renderArea = {{0, 0}, {extent.width, extent.height}},
            .layerCount = 1,
            .viewMask = 0,
            .colorAttachmentCount = 1,
            .pColorAttachments = colorAttachments,
            .pDepthAttachment = nullptr,
            .pStencilAttachment = nullptr};

        VkViewport viewport{
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float_t>(extent.width),
            .height = static_cast<float_t>(extent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f};
        VkRect2D scissor{{0, 0}, {extent.width, extent.height}};

        vkCmdBeginRendering(vulkanCommandBuffer, &renderingInfo);

        vkCmdBindPipeline(vulkanCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, compositePipeline->vulkanHandle);
        vkCmdSetViewport(vulkanCommandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(vulkanCommandBuffer, 0, 1, &scissor);

        VkDescriptorImageInfo imageInfo{
            .sampler = VK_NULL_HANDLE,
            .imageView = backBuffer->vulkanImageView,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkDescriptorImageInfo samplerInfo{
            .sampler = nearestPixelSampler->vulkanHandle,
            .imageView = VK_NULL_HANDLE,
            .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED};

        VkWriteDescriptorSet descriptorWrites[2] = {
            {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
             .pNext = nullptr,
             .dstSet = VK_NULL_HANDLE,
             .dstBinding = 0,
             .dstArrayElement = 0,
             .descriptorCount = 1,
             .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
             .pImageInfo = &imageInfo,
             .pBufferInfo = nullptr,
             .pTexelBufferView = nullptr},
            {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
             .pNext = nullptr,
             .dstSet = VK_NULL_HANDLE,
             .dstBinding = 1,
             .dstArrayElement = 0,
             .descriptorCount = 1,
             .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
             .pImageInfo = &samplerInfo,
             .pBufferInfo = nullptr,
             .pTexelBufferView = nullptr}};

        gfx->PushDescriptorSets(
            commandBuffer,
            compositePipeline.get(),
            2,
            descriptorWrites);

        vkCmdDraw(vulkanCommandBuffer, 4, 1, 0, 0);

        vkCmdEndRendering(vulkanCommandBuffer);

        // FIXME: Stop cheating!
        gfx->FullBarrier(commandBuffer);
        gfx->PixelBufferBarrier(
            commandBuffer,
            presentationBuffer,
            ImageLayout::COLOR_ATTACHMENT_OPTIMAL,
            ImageLayout::TRANSFER_DST_OPTIMAL);
    }
}