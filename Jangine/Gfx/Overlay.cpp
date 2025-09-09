#include "Overlay.hpp"
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
    Overlay::Overlay(Gfx::Core &gfx)
        : gfx(gfx),
          fontCollection(gfx),
          logger(Jangine::Core::GetLogger("Gfx::Overlay")),
          backBuffer(nullptr, std::ref(gfx)),
          renderPipeline(nullptr, std::ref(gfx)),
          compositePipeline(nullptr, std::ref(gfx)),
          vertexBuffer(nullptr, std::ref(gfx)),
          indexBuffer(nullptr, std::ref(gfx)),
          nearestPixelSampler(nullptr, std::ref(gfx)),
          linearPixelSampler(nullptr, std::ref(gfx)),
          blurHorizontalPipeline(nullptr, std::ref(gfx)),
          blurVerticalPipeline(nullptr, std::ref(gfx)),
          blurSampler(nullptr, std::ref(gfx)),
          blurIntermediateBuffer(nullptr, std::ref(gfx)),
          blurBuffer(),
          blurNoiseBuffer(nullptr, std::ref(gfx)),
          blurNoiseSampler(nullptr, std::ref(gfx)),
          placeholderBuffer(nullptr, std::ref(gfx))
    {
        logger.Func(__func__);

        InitializeCommandBufferPool();
    }

    Overlay::~Overlay()
    {
        logger.Func(__func__);
    }

    void Overlay::Create()
    {
        logger.Func(__func__);

        defaultFont = fontCollection.GetFont(Text::FontKey("Quantico-Regular", 32));

        uint8_t data[4] = {255, 255, 255, 255};
        placeholderBuffer = gfx.CreatePixelBuffer(
            Span(data),
            1,
            1,
            Format::RGBA8,
            PixelBufferUsage::Sampled);

        vertexBuffer = gfx.CreateMemoryBuffer(
            MAX_VERTICES * sizeof(Vertex2f),
            MemoryBufferUsage::Vertex,
            MemoryAccess::Write);

        indexBuffer = gfx.CreateMemoryBuffer(
            MAX_INDICES * sizeof(uint16_t),
            MemoryBufferUsage::Index,
            MemoryAccess::Write);

        PixelSamplerParameters nearestParameters = {
            .minFilter = SamplerFilter::NEAREST,
            .magFilter = SamplerFilter::NEAREST,
            .mipmapMode = SamplerMipmapMode::NEAREST,
            .addressModeU = SamplerAddressMode::CLAMP_TO_BORDER,
            .addressModeV = SamplerAddressMode::CLAMP_TO_BORDER,
            .borderColor = BorderColor::FLOAT_OPAQUE_WHITE};
        nearestPixelSampler = gfx.CreatePixelSampler(nearestParameters);

        PixelSamplerParameters linearParameters = {
            .minFilter = SamplerFilter::LINEAR,
            .magFilter = SamplerFilter::LINEAR,
            .mipmapMode = SamplerMipmapMode::LINEAR,
            .addressModeU = SamplerAddressMode::CLAMP_TO_EDGE,
            .addressModeV = SamplerAddressMode::CLAMP_TO_EDGE,
            .borderColor = BorderColor::FLOAT_OPAQUE_BLACK};
        linearPixelSampler = gfx.CreatePixelSampler(linearParameters);

        // --------------------- BLUR
        PixelSamplerParameters blurSamplerParameters = {
            .minFilter = SamplerFilter::LINEAR,
            .magFilter = SamplerFilter::LINEAR,
            .mipmapMode = SamplerMipmapMode::LINEAR,
            .addressModeU = SamplerAddressMode::REPEAT,
            .addressModeV = SamplerAddressMode::REPEAT,
            .borderColor = BorderColor::FLOAT_OPAQUE_BLACK};
        blurNoiseSampler = gfx.CreatePixelSampler(blurSamplerParameters);

        // Use a fixed seed for repeatability, or std::random_device for more randomness
        std::mt19937 rng(42); // Fixed seed
        std::uniform_int_distribution<int> dist(0, 255);

        int noiseSize = 128;
        int noiseDataSize = noiseSize * noiseSize * 4;
        std::vector<std::byte> noiseData(noiseDataSize);
        for (int i = 0; i < noiseDataSize; ++i)
        {
            noiseData[i] = static_cast<std::byte>(dist(rng));
        }

        blurNoiseBuffer = gfx.CreatePixelBuffer(
            Span(noiseData),
            noiseSize,
            noiseSize,
            Format::RGBA8,
            PixelBufferUsage::Sampled);
        // --------------------- BLUR
    }

    void Overlay::PrepareRender(const Presenter &presenter, const DisplayParameters &in_displayParameters)
    {
        Presenter::SurfaceInfo surfaceInfo = presenter.GetSurfaceInfo();

        bool_t recreateBackBuffer = !backBuffer;
        if (backBuffer)
        {
            recreateBackBuffer = backBuffer->width != surfaceInfo.width ||
                                 backBuffer->height != surfaceInfo.height ||
                                 backBuffer->format != surfaceInfo.format;
        }

        displayParameters = in_displayParameters;

        if (recreateBackBuffer)
        {
            backBuffer = gfx.CreatePixelBuffer(
                surfaceInfo.width,
                surfaceInfo.height,
                surfaceInfo.format,
                PixelBufferUsage::ColorAttachment | PixelBufferUsage::Sampled |
                    PixelBufferUsage::TransferDst | PixelBufferUsage::TransferSrc);

            // --------------------- BLUR
            blurIntermediateBuffer = gfx.CreatePixelBuffer(
                surfaceInfo.width,
                surfaceInfo.height,
                surfaceInfo.format,
                PixelBufferUsage::ColorAttachment | PixelBufferUsage::Sampled |
                    PixelBufferUsage::TransferDst | PixelBufferUsage::TransferSrc,
                Aspect::Color);
            blurBuffer = gfx.CreatePixelBuffer(
                surfaceInfo.width,
                surfaceInfo.height,
                surfaceInfo.format,
                PixelBufferUsage::ColorAttachment | PixelBufferUsage::Sampled |
                    PixelBufferUsage::TransferDst | PixelBufferUsage::TransferSrc,
                Aspect::Color);
            // ---------------------
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
            mainParams.shaderProgram = ThrowInvalidOperationIfNull(gfx.GetShaderProgram("built-in-2d"),
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
                 .format = Format::U32,
                 .offset = offsetof(Vertex2f, color)}};
            mainParams.attachments = {
                {.format = Format::BGRA8,
                 .blend = straightAlphaBlend}};

            renderPipeline = gfx.CreatePipeline(mainParams);
        }

        // Composition pipeline (no vertex input)
        if (!compositePipeline)
        {
            PipelineParameters compositionParams;
            compositionParams.shaderProgram = ThrowInvalidOperationIfNull(gfx.GetShaderProgram("built-in-2d-composition"),
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

            compositePipeline = gfx.CreatePipeline(compositionParams);
        }

        if (!blurHorizontalPipeline)
        {
            PipelineParameters blurPipelineParams;
            blurPipelineParams.shaderProgram = ThrowInvalidOperationIfNull(gfx.GetShaderProgram("built-in-blur"),
                                                                           "Shader program 'built-in-blur' not found in cache.");
            blurPipelineParams.pushConstantRanges = {
                {.stageFlags = ShaderStage::FRAGMENT,
                 .offset = 0,
                 .size = sizeof(BlurPushConstants)}};
            blurPipelineParams.descriptorLayout = {
                {.binding = 0,
                 .descriptorType = DescriptorType::SAMPLED_IMAGE,
                 .descriptorCount = 1,
                 .stages = ShaderStage::FRAGMENT},
                {.binding = 1,
                 .descriptorType = DescriptorType::SAMPLER,
                 .descriptorCount = 1,
                 .stages = ShaderStage::FRAGMENT},
                {.binding = 2,
                 .descriptorType = DescriptorType::SAMPLED_IMAGE,
                 .descriptorCount = 1,
                 .stages = ShaderStage::FRAGMENT},
                {.binding = 3,
                 .descriptorType = DescriptorType::SAMPLER,
                 .descriptorCount = 1,
                 .stages = ShaderStage::FRAGMENT}};
            blurPipelineParams.topology = PrimitiveTopology::TRIANGLE_LIST;
            blurPipelineParams.cullMode = CullMode::NONE;
            blurPipelineParams.frontFace = FrontFace::COUNTER_CLOCKWISE;
            blurPipelineParams.depthTestEnabled = false;
            blurPipelineParams.depthWriteEnabled = false;
            blurPipelineParams.depthCompareOp = CompareOp::ALWAYS;
            blurPipelineParams.vertexInputBindings = {};
            blurPipelineParams.vertexInputAttributes = {};
            blurPipelineParams.attachments = {
                {.format = Format::BGRA8,
                 .blend = straightAlphaBlend}};

            struct BlurSpecialization
            {
                uint32_t direction;
            };

            blurPipelineParams.specialization = {
                {.stage = ShaderStage::FRAGMENT,
                 .entries = {{.id = 0, .offset = 0, .size = sizeof(uint32_t)}},
                 .data = PipelineParameters::Specialization::ReadData(BlurSpecialization{1})}};
            blurHorizontalPipeline = gfx.CreatePipeline(blurPipelineParams);

            blurPipelineParams.specialization = {
                {.stage = ShaderStage::FRAGMENT,
                 .entries = {{.id = 0, .offset = 0, .size = sizeof(uint32_t)}},
                 .data = PipelineParameters::Specialization::ReadData(BlurSpecialization{0})}};
            blurVerticalPipeline = gfx.CreatePipeline(blurPipelineParams);

            PixelSamplerParameters samplerParams{
                .minFilter = SamplerFilter::NEAREST,
                .magFilter = SamplerFilter::NEAREST,
                .mipmapMode = SamplerMipmapMode::NEAREST,
                .addressModeU = SamplerAddressMode::CLAMP_TO_EDGE,
                .addressModeV = SamplerAddressMode::CLAMP_TO_EDGE,
                .addressModeW = SamplerAddressMode::CLAMP_TO_EDGE,
                .borderColor = BorderColor::FLOAT_OPAQUE_WHITE,
            };

            blurSampler = gfx.CreatePixelSampler(samplerParams);
        }

        isReady = true;
    }

    void Overlay::Render(const Presenter &presenter, double_t absoluteTime, float_t deltaTime)
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
            gfx.WriteMemoryBuffer(vertexBuffer.get(), currentCommandBuffer->GetVertexData());
            gfx.WriteMemoryBuffer(indexBuffer.get(), currentCommandBuffer->GetIndexData());
        }

        auto batches = currentCommandBuffer->GetBatches();
        for (const auto &batch : batches)
        {
            auto commands = currentCommandBuffer->GetBatchCommands(batch);
            RecordBatch(presenter.GetCurrentCommandBuffer(), commands, backBuffer.get());
        }

        FinishFrame(presenter);
    }

    void Overlay::RecordBatch(Gfx::CommandBuffer commandBuffer, std::span<const CommandBuffer::Command> commands, PixelBuffer *targetBuffer)
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

        VkDeviceSize vertexBufferOffset = 0;
        vkCmdBindVertexBuffers(vulkanCommandBuffer, 0, 1, &vertexBuffer->vulkanBuffer, &vertexBufferOffset);
        vkCmdBindIndexBuffer(vulkanCommandBuffer, indexBuffer->vulkanBuffer, 0, VK_INDEX_TYPE_UINT16);

        for (const auto &command : commands)
        {
            VkImageView commandTexture = placeholderBuffer->vulkanImageView;
            VkSampler commandSampler = nearestPixelSampler->vulkanHandle;

            PushConstants pushConstants{
                .scale = Eigen::Vector2f(2.0f / targetExtent.width, 2.0f / targetExtent.height),
                .isSdf = 0};

            if (command.texture)
            {
                commandTexture = command.texture->vulkanImageView;
                commandSampler = linearPixelSampler->vulkanHandle;
            }
            else if (command.font)
            {
                commandTexture = command.font->GetPixelBuffer()->vulkanImageView;
                commandSampler = linearPixelSampler->vulkanHandle;
                pushConstants.isSdf = 1;
                pushConstants.sdfRange = command.fontWidth;
            }

            if (command.scissor)
            {
                Rectangle scissorRectangle = command.scissor.value();
                VkRect2D commandScissor{
                    .offset = {.x = static_cast<int32_t>(scissorRectangle.left),
                               .y = static_cast<int32_t>(scissorRectangle.top)},
                    .extent = {.width = static_cast<uint32_t>(scissorRectangle.width()),
                               .height = static_cast<uint32_t>(scissorRectangle.height())}};
                vkCmdSetScissor(vulkanCommandBuffer, 0, 1, &commandScissor);
            }
            else
            {
                vkCmdSetScissor(vulkanCommandBuffer, 0, 1, &scissor);
            }

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

            gfx.PushDescriptorSets(
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
        gfx.FullBarrier(commandBuffer);
    }

    void Overlay::PrepareFrame(const Presenter &presenter)
    {
        Gfx::CommandBuffer commandBuffer = presenter.GetCurrentCommandBuffer();

        gfx.PixelBufferBarrier(commandBuffer,
                               backBuffer.get(),
                               ImageLayout::UNDEFINED,
                               ImageLayout::TRANSFER_DST_OPTIMAL);
        gfx.ClearPixelBuffer(commandBuffer,
                             backBuffer.get(),
                             Color::Transparent);
        gfx.PixelBufferBarrier(commandBuffer,
                               backBuffer.get(),
                               ImageLayout::TRANSFER_DST_OPTIMAL,
                               ImageLayout::COLOR_ATTACHMENT_OPTIMAL);

        // --------------------- BLUR
        Presenter::Image presentationBuffer = presenter.GetCurrentPresentationBuffer();

        gfx.PixelBufferBarrier(commandBuffer,
                               presentationBuffer,
                               ImageLayout::TRANSFER_DST_OPTIMAL,
                               ImageLayout::SHADER_READ_ONLY_OPTIMAL);
        {
            gfx.PixelBufferBarrier(commandBuffer,
                                   blurIntermediateBuffer.get(),
                                   ImageLayout::UNDEFINED,
                                   ImageLayout::COLOR_ATTACHMENT_OPTIMAL);

            BlurPushConstants blurPushConstants{
                .scale = 2.0f,
                .strength = 1.5f};

            VkRenderingAttachmentInfo colorAttachment = {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .pNext = nullptr,
                .imageView = blurIntermediateBuffer->vulkanImageView,
                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .resolveMode = VK_RESOLVE_MODE_NONE,
                .resolveImageView = VK_NULL_HANDLE,
                .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE};
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

            vkCmdBeginRendering(commandBuffer.vulkanHandle, &renderingInfo);
            vkCmdBindPipeline(commandBuffer.vulkanHandle, VK_PIPELINE_BIND_POINT_GRAPHICS, blurVerticalPipeline->vulkanHandle);
            vkCmdSetViewport(commandBuffer.vulkanHandle, 0, 1, &viewport);
            vkCmdSetScissor(commandBuffer.vulkanHandle, 0, 1, &scissor);

            VkDescriptorImageInfo sourcePixelBuffer{
                .sampler = nullptr,
                .imageView = presentationBuffer.vulkanImageView,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            VkDescriptorImageInfo sourceSampler{
                .sampler = blurSampler->vulkanHandle,
                .imageView = nullptr,
                .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED};
            VkDescriptorImageInfo noisePixelBuffer{
                .sampler = nullptr,
                .imageView = blurNoiseBuffer->vulkanImageView,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            VkDescriptorImageInfo noiseSampler{
                .sampler = blurNoiseSampler->vulkanHandle,
                .imageView = nullptr,
                .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED};
            VkWriteDescriptorSet descriptorWrites[4] = {
                {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                 .pNext = nullptr,
                 .dstSet = VK_NULL_HANDLE,
                 .dstBinding = 0,
                 .dstArrayElement = 0,
                 .descriptorCount = 1,
                 .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                 .pImageInfo = &sourcePixelBuffer,
                 .pBufferInfo = nullptr,
                 .pTexelBufferView = nullptr},
                {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                 .pNext = nullptr,
                 .dstSet = VK_NULL_HANDLE,
                 .dstBinding = 1,
                 .dstArrayElement = 0,
                 .descriptorCount = 1,
                 .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
                 .pImageInfo = &sourceSampler,
                 .pBufferInfo = nullptr,
                 .pTexelBufferView = nullptr},
                {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                 .pNext = nullptr,
                 .dstSet = VK_NULL_HANDLE,
                 .dstBinding = 2,
                 .dstArrayElement = 0,
                 .descriptorCount = 1,
                 .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                 .pImageInfo = &noisePixelBuffer,
                 .pBufferInfo = nullptr,
                 .pTexelBufferView = nullptr},
                {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                 .pNext = nullptr,
                 .dstSet = VK_NULL_HANDLE,
                 .dstBinding = 3,
                 .dstArrayElement = 0,
                 .descriptorCount = 1,
                 .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
                 .pImageInfo = &noiseSampler,
                 .pBufferInfo = nullptr,
                 .pTexelBufferView = nullptr}};
            gfx.PushDescriptorSets(
                commandBuffer,
                blurVerticalPipeline.get(),
                4,
                descriptorWrites);

            vkCmdPushConstants(commandBuffer.vulkanHandle,
                               blurVerticalPipeline->vulkanLayout,
                               VK_SHADER_STAGE_FRAGMENT_BIT,
                               0,
                               sizeof(BlurPushConstants),
                               &blurPushConstants);
            vkCmdDraw(commandBuffer.vulkanHandle, 3, 1, 0, 0);

            vkCmdEndRendering(commandBuffer.vulkanHandle);

            gfx.PixelBufferBarrier(commandBuffer,
                                   blurIntermediateBuffer.get(),
                                   ImageLayout::COLOR_ATTACHMENT_OPTIMAL,
                                   ImageLayout::SHADER_READ_ONLY_OPTIMAL);
            gfx.PixelBufferBarrier(commandBuffer,
                                   blurBuffer.get(),
                                   ImageLayout::UNDEFINED,
                                   ImageLayout::COLOR_ATTACHMENT_OPTIMAL);

            colorAttachment = {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .pNext = nullptr,
                .imageView = blurBuffer->vulkanImageView,
                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .resolveMode = VK_RESOLVE_MODE_NONE,
                .resolveImageView = VK_NULL_HANDLE,
                .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE};
            colorAttachments[0] = colorAttachment;

            renderingInfo = {
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

            vkCmdBeginRendering(commandBuffer.vulkanHandle, &renderingInfo);
            vkCmdBindPipeline(commandBuffer.vulkanHandle, VK_PIPELINE_BIND_POINT_GRAPHICS, blurHorizontalPipeline->vulkanHandle);
            vkCmdSetViewport(commandBuffer.vulkanHandle, 0, 1, &viewport);
            vkCmdSetScissor(commandBuffer.vulkanHandle, 0, 1, &scissor);

            sourcePixelBuffer = {
                .sampler = nullptr,
                .imageView = blurIntermediateBuffer->vulkanImageView,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            sourceSampler = {
                .sampler = blurSampler->vulkanHandle,
                .imageView = nullptr,
                .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED};
            descriptorWrites[0] =
                {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                 .pNext = nullptr,
                 .dstSet = VK_NULL_HANDLE,
                 .dstBinding = 0,
                 .dstArrayElement = 0,
                 .descriptorCount = 1,
                 .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                 .pImageInfo = &sourcePixelBuffer,
                 .pBufferInfo = nullptr,
                 .pTexelBufferView = nullptr};
            descriptorWrites[1] = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = nullptr,
                .dstSet = VK_NULL_HANDLE,
                .dstBinding = 1,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
                .pImageInfo = &sourceSampler,
                .pBufferInfo = nullptr,
                .pTexelBufferView = nullptr};
            gfx.PushDescriptorSets(
                commandBuffer,
                blurHorizontalPipeline.get(),
                4,
                descriptorWrites);
            vkCmdPushConstants(commandBuffer.vulkanHandle,
                               blurHorizontalPipeline->vulkanLayout,
                               VK_SHADER_STAGE_FRAGMENT_BIT,
                               0,
                               sizeof(BlurPushConstants),
                               &blurPushConstants);
            vkCmdDraw(commandBuffer.vulkanHandle, 3, 1, 0, 0);

            vkCmdEndRendering(commandBuffer.vulkanHandle);

            gfx.PixelBufferBarrier(commandBuffer,
                                   blurBuffer.get(),
                                   ImageLayout::COLOR_ATTACHMENT_OPTIMAL,
                                   ImageLayout::SHADER_READ_ONLY_OPTIMAL);
        }

        gfx.PixelBufferBarrier(commandBuffer,
                               presentationBuffer,
                               ImageLayout::SHADER_READ_ONLY_OPTIMAL,
                               ImageLayout::TRANSFER_DST_OPTIMAL);
        // --------------------- BLUR
    }

    void Overlay::FinishFrame(const Presenter &presenter)
    {
        Gfx::CommandBuffer commandBuffer = presenter.GetCurrentCommandBuffer();
        Presenter::Image presentationBuffer = presenter.GetCurrentPresentationBuffer();

        gfx.PixelBufferBarrier(commandBuffer,
                               presentationBuffer,
                               ImageLayout::TRANSFER_DST_OPTIMAL,
                               ImageLayout::COLOR_ATTACHMENT_OPTIMAL);
        gfx.PixelBufferBarrier(commandBuffer,
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

        gfx.PushDescriptorSets(
            commandBuffer,
            compositePipeline.get(),
            2,
            descriptorWrites);

        vkCmdDraw(vulkanCommandBuffer, 4, 1, 0, 0);

        vkCmdEndRendering(vulkanCommandBuffer);

        // FIXME: Stop cheating!
        gfx.FullBarrier(commandBuffer);
        gfx.PixelBufferBarrier(
            commandBuffer,
            presentationBuffer,
            ImageLayout::COLOR_ATTACHMENT_OPTIMAL,
            ImageLayout::TRANSFER_DST_OPTIMAL);
    }
}