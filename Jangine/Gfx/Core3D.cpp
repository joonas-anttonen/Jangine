#include "Core3D.hpp"

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
    Core3D::Core3D(Gfx::Core *gfx)
        : gfx(gfx),
          vertexBuffer(nullptr, std::ref(*gfx)),
          indexBuffer(nullptr, std::ref(*gfx)),
          perSceneBuffer(nullptr, std::ref(*gfx)),
          perMeshBuffer(nullptr, std::ref(*gfx)),
          renderBuffer(nullptr, std::ref(*gfx)),
          depthBuffer(nullptr, std::ref(*gfx)),
          motionBuffer(nullptr, std::ref(*gfx)),
          displayBuffer(nullptr, std::ref(*gfx)),
          shapePipeline(nullptr, std::ref(*gfx)),
          logger(Jangine::Core::GetLogger("Gfx::Core3D"))
    {
        logger.Func(__func__);
    }

    Core3D::~Core3D()
    {
        logger.Func(__func__);
    }

    void Core3D::Create()
    {
        logger.Func(__func__);

        vertexBuffer = gfx->CreateMemoryBuffer(
            MAX_VERTICES * sizeof(ShapeVertex),
            MemoryBufferUsage::Vertex,
            MemoryAccess::Write);

        indexBuffer = gfx->CreateMemoryBuffer(
            MAX_INDICES * sizeof(uint16_t),
            MemoryBufferUsage::Index,
            MemoryAccess::Write);

        perSceneBuffer = gfx->CreateMemoryBuffer(
            sizeof(PerSceneData),
            MemoryBufferUsage::Uniform,
            MemoryAccess::Write);

        perMeshBuffer = gfx->CreateMemoryBuffer(
            sizeof(PerMeshData) * 10,
            MemoryBufferUsage::Uniform,
            MemoryAccess::Write);
    }

    void Core3D::InitializeRendering(const DisplayParameters &wantedDisplayParameters)
    {
        logger.Func(__func__);

        bool_t antialiasingModeChanged = displayParameters.AntialiasingModeChanged(wantedDisplayParameters);
        bool_t renderSizeChanged = displayParameters.RenderSizeChanged(wantedDisplayParameters);
        bool_t displaySizeChanged = displayParameters.DisplaySizeChanged(wantedDisplayParameters);
        bool_t surfaceFormatChanged = displayParameters.SurfaceFormatChanged(wantedDisplayParameters);

        displayParameters = wantedDisplayParameters;

        bool_t renderResourcesNull = !renderBuffer || !depthBuffer || !motionBuffer;
        bool_t initializeRender = renderResourcesNull || antialiasingModeChanged || renderSizeChanged || surfaceFormatChanged;

        bool_t displayResourcesNull = !displayBuffer;
        bool_t initializeDisplay = displayResourcesNull || displaySizeChanged || surfaceFormatChanged;

        if (initializeRender)
        {
            logger.Debug(
                std::format(
                    "Initializing render buffers: antialiasingModeChanged: {}, renderSizeChanged: {}, surfaceFormatChanged: {}",
                    antialiasingModeChanged,
                    renderSizeChanged,
                    surfaceFormatChanged),
                __func__);

            renderBuffer = gfx->CreatePixelBuffer(
                wantedDisplayParameters.renderWidth,
                wantedDisplayParameters.renderHeight,
                Format::RGBA32,
                PixelBufferUsage::ColorAttachment | PixelBufferUsage::Sampled |
                    PixelBufferUsage::TransferDst | PixelBufferUsage::TransferSrc,
                Aspect::Color);

            depthBuffer = gfx->CreatePixelBuffer(
                wantedDisplayParameters.renderWidth,
                wantedDisplayParameters.renderHeight,
                gfx->GetDeviceDepthFormat(),
                PixelBufferUsage::DepthAttachment | PixelBufferUsage::Sampled |
                    PixelBufferUsage::TransferDst | PixelBufferUsage::TransferSrc,
                Aspect::Depth);

            motionBuffer = gfx->CreatePixelBuffer(
                wantedDisplayParameters.renderWidth,
                wantedDisplayParameters.renderHeight,
                Format::R16G16,
                PixelBufferUsage::ColorAttachment | PixelBufferUsage::Sampled |
                    PixelBufferUsage::TransferDst | PixelBufferUsage::TransferSrc,
                Aspect::Color);
        }

        if (initializeDisplay)
        {
            logger.Debug(
                std::format(
                    "Initializing display buffer: displaySizeChanged: {}, surfaceFormatChanged: {}",
                    displaySizeChanged,
                    surfaceFormatChanged),
                __func__);

            displayBuffer = gfx->CreatePixelBuffer(
                wantedDisplayParameters.displayWidth,
                wantedDisplayParameters.displayHeight,
                Format::RGBA32,
                PixelBufferUsage::ColorAttachment | PixelBufferUsage::Sampled |
                    PixelBufferUsage::TransferDst | PixelBufferUsage::TransferSrc,
                Aspect::Color);
        }

        if (!shapePipeline)
        {
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

            PipelineParameters shapePipelineParams;
            shapePipelineParams.shaderProgram = ThrowInvalidOperationIfNull(gfx->GetShaderProgram("built-in-shape-disc"),
                                                                            "Shader program 'built-in-shape-disc' not found in cache.");
            shapePipelineParams.pushConstantRanges = {};
            shapePipelineParams.descriptorLayout = {
                {.binding = 0,
                 .descriptorType = DescriptorType::UNIFORM_BUFFER,
                 .descriptorCount = 1,
                 .stages = ShaderStage::VERTEX | ShaderStage::FRAGMENT},
                {.binding = 1,
                 .descriptorType = DescriptorType::UNIFORM_BUFFER,
                 .descriptorCount = 1,
                 .stages = ShaderStage::VERTEX | ShaderStage::FRAGMENT}};
            shapePipelineParams.topology = PrimitiveTopology::TRIANGLE_LIST;
            shapePipelineParams.cullMode = CullMode::NONE;
            shapePipelineParams.frontFace = FrontFace::COUNTER_CLOCKWISE;
            shapePipelineParams.depthTestEnabled = false;
            shapePipelineParams.depthWriteEnabled = false;
            shapePipelineParams.depthCompareOp = CompareOp::ALWAYS;
            shapePipelineParams.vertexInputBindings = {
                {.binding = 0,
                 .stride = sizeof(ShapeVertex),
                 .inputRate = VertexInputRate::VERTEX}};
            shapePipelineParams.vertexInputAttributes = {
                {.location = 0,
                 .binding = 0,
                 .format = Format::RGB32,
                 .offset = offsetof(ShapeVertex, Position)},
                {.location = 1,
                 .binding = 0,
                 .format = Format::RG32,
                 .offset = offsetof(ShapeVertex, UV)}};
            shapePipelineParams.attachments = {
                {.format = Format::RGBA32,
                 .blend = straightAlphaBlend}};

            shapePipeline = gfx->CreatePipeline(shapePipelineParams);
        }
    }

    void Core3D::Render(const Presenter &presenter, double_t absoluteTime, float_t deltaTime)
    {
        // Avoid unused parameter warning
        (void)absoluteTime;
        (void)deltaTime;

        VkExtent2D renderExtent = {.width = renderBuffer->width, .height = renderBuffer->height};

        float_t viewportX = 0.0f;
        float_t viewportY = 0.0f;
        float_t viewportWidth = static_cast<float_t>(renderExtent.width);
        float_t viewportHeight = static_cast<float_t>(renderExtent.height);

        VkViewport viewport{
            .x = viewportX,
            .y = viewportY + viewportHeight,
            .width = viewportWidth,
            .height = -viewportHeight,
            .minDepth = 0.0f,
            .maxDepth = 1.0f};
        VkRect2D scissor{{0, 0}, {renderExtent.width, renderExtent.height}};

         //float_t orthoWidth = 10.0f;
         //float_t orthoHeight = orthoWidth / displayParameters.GetAspectRatio();
         //camera.SetOrthographic(orthoWidth, orthoHeight, 0.1f, 100.0f);
         camera.SetPerspective(Math::pi / 4.0f, displayParameters.GetAspectRatio(), 0.1f, 100.0f);

        UserInput input;
        camera.Update(input, deltaTime);

        PerSceneData sceneData{};
        sceneData.ViewProjection = camera.GetViewProjectionMatrix();
        sceneData.View = camera.GetViewMatrix();
        sceneData.ViewInverse = camera.GetInverseViewMatrix();
        sceneData.ViewPosition = camera.GetPosition();
        sceneData.Screen = Eigen::Vector2f(
            static_cast<float_t>(displayParameters.renderWidth),
            static_cast<float_t>(displayParameters.renderHeight));

        std::span<const uint8_t> perSceneDataSpan(reinterpret_cast<const uint8_t *>(&sceneData), sizeof(sceneData));
        gfx->WriteMemoryBuffer(perSceneBuffer.get(), perSceneDataSpan);

        Eigen::Isometry3f testIsometry;
        testIsometry.setIdentity();
        testIsometry.linear() = Eigen::AngleAxisf(0.0f, Eigen::Vector3f::UnitY()).toRotationMatrix();

        PerMeshData meshData{};
        meshData.Transform = testIsometry.matrix();
        meshData.Alignment = 0; // 0 = no alignment, 1 = billboard
        meshData.Color = Eigen::Vector4f(1.0f, 1.0f, 1.0f, 1.0f);
        meshData.Thickness = 0.01f;

        //meshData.ColorEnd = Eigen::Vector4f(1.0f, 1.0f, 1.0f, 1.0f);
        //meshData.Start = Eigen::Vector3f(0.0f, 0.0f, 0.0f);
        //meshData.End = Eigen::Vector3f(252.0f, 0.1f, 10.0f);

        meshData.AngleStart = 0.0f;
        meshData.AngleEnd = 0.0f;
        meshData.ColorInnerEnd = Eigen::Vector4f(1.0f, 1.0f, 1.0f, 1.0f);
        meshData.ColorOuterStart = Eigen::Vector4f(1.0f, 1.0f, 1.0f, 1.0f);
        meshData.ColorOuterEnd = Eigen::Vector4f(1.0f, 1.0f, 1.0f, 1.0f);
        meshData.Radius = 1.0f;

        std::span<const uint8_t> perMeshDataSpan(reinterpret_cast<const uint8_t *>(&meshData), sizeof(meshData));
        gfx->WriteMemoryBuffer(perMeshBuffer.get(), perMeshDataSpan);

        // Add a single quad
        uint16_t quadIndices[6] = {0, 1, 2, 0, 2, 3};
        ShapeVertex quadVertices[4] = {
            {{1.0f, -1.0f, 0.0f}, {-1.0f, -1.0f}},
            {{1.0f, 1.0f, 0.0f}, {-1.0f, 1.0f}},
            {{-1.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
            {{-1.0f, -1.0f, 0.0f}, {1.0f, -1.0f}}};
        std::span<const uint8_t> vertexDataSpan(reinterpret_cast<const uint8_t *>(quadVertices), sizeof(quadVertices));
        gfx->WriteMemoryBuffer(vertexBuffer.get(), vertexDataSpan);

        std::span<const uint8_t> indexDataSpan(reinterpret_cast<const uint8_t *>(quadIndices), sizeof(quadIndices));
        gfx->WriteMemoryBuffer(indexBuffer.get(), indexDataSpan);

        CommandBuffer commandBuffer = presenter.GetCurrentCommandBuffer();

        gfx->PixelBufferBarrier(commandBuffer,
                                renderBuffer.get(),
                                ImageLayout::UNDEFINED,
                                ImageLayout::COLOR_ATTACHMENT_OPTIMAL);

        VkRenderingAttachmentInfo colorAttachment{
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext = nullptr,
            .imageView = renderBuffer->vulkanImageView,
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .resolveMode = VK_RESOLVE_MODE_NONE,
            .resolveImageView = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = VkClearValue{
                .color = {displayParameters.clearColor.r,
                          displayParameters.clearColor.g,
                          displayParameters.clearColor.b,
                          displayParameters.clearColor.a}}};

        VkRenderingAttachmentInfo colorAttachments[] = {colorAttachment};

        VkRenderingInfo renderingInfo{
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .pNext = nullptr,
            .flags = 0,
            .renderArea = {{0, 0}, {renderExtent.width, renderExtent.height}},
            .layerCount = 1,
            .viewMask = 0,
            .colorAttachmentCount = 1,
            .pColorAttachments = colorAttachments,
            .pDepthAttachment = nullptr,
            .pStencilAttachment = nullptr};

        vkCmdBeginRendering(commandBuffer.vulkanHandle, &renderingInfo);
        vkCmdBindPipeline(commandBuffer.vulkanHandle, VK_PIPELINE_BIND_POINT_GRAPHICS, shapePipeline->vulkanHandle);
        vkCmdSetViewport(commandBuffer.vulkanHandle, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer.vulkanHandle, 0, 1, &scissor);

        VkDeviceSize vertexBufferOffset = 0;
        vkCmdBindVertexBuffers(commandBuffer.vulkanHandle, 0, 1, &vertexBuffer->vulkanBuffer, &vertexBufferOffset);
        vkCmdBindIndexBuffer(commandBuffer.vulkanHandle, indexBuffer->vulkanBuffer, 0, VK_INDEX_TYPE_UINT16);

        VkDescriptorBufferInfo perSceneBufferInfo{
            .buffer = perSceneBuffer->vulkanBuffer,
            .offset = 0,
            .range = sizeof(PerSceneData)};

        VkDescriptorBufferInfo perMeshBufferInfo{
            .buffer = perMeshBuffer->vulkanBuffer,
            .offset = 0,
            .range = sizeof(PerMeshData)};

        VkWriteDescriptorSet descriptorWrites[2] = {
            {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
             .pNext = nullptr,
             .dstSet = VK_NULL_HANDLE,
             .dstBinding = 0,
             .dstArrayElement = 0,
             .descriptorCount = 1,
             .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
             .pImageInfo = nullptr,
             .pBufferInfo = &perSceneBufferInfo,
             .pTexelBufferView = nullptr},
            {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
             .pNext = nullptr,
             .dstSet = VK_NULL_HANDLE,
             .dstBinding = 1,
             .dstArrayElement = 0,
             .descriptorCount = 1,
             .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
             .pImageInfo = nullptr,
             .pBufferInfo = &perMeshBufferInfo,
             .pTexelBufferView = nullptr}};

        gfx->PushDescriptorSets(
            commandBuffer,
            shapePipeline.get(),
            2,
            descriptorWrites);

        vkCmdDrawIndexed(
            commandBuffer.vulkanHandle,
            6,
            1,
            0,
            0,
            0);

        vkCmdEndRendering(commandBuffer.vulkanHandle);

        // FIXME: Stop cheating!
        gfx->FullBarrier(commandBuffer);

        gfx->PixelBufferBarrier(commandBuffer,
                                renderBuffer.get(),
                                ImageLayout::COLOR_ATTACHMENT_OPTIMAL,
                                ImageLayout::TRANSFER_SRC_OPTIMAL);
        gfx->PixelBufferBarrier(commandBuffer,
                                displayBuffer.get(),
                                ImageLayout::UNDEFINED,
                                ImageLayout::TRANSFER_DST_OPTIMAL);

        // render -> display
        {
            VkImageBlit blitRegion{
                .srcSubresource = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = 0,
                    .baseArrayLayer = 0,
                    .layerCount = 1},
                .srcOffsets = {{0, 0, 0}, {static_cast<int32_t>(renderBuffer->width), static_cast<int32_t>(renderBuffer->height), 1}},
                .dstSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1},
                .dstOffsets = {{0, 0, 0}, {static_cast<int32_t>(displayBuffer->width), static_cast<int32_t>(displayBuffer->height), 1}}};

            vkCmdBlitImage(commandBuffer.vulkanHandle,
                           renderBuffer->vulkanImage,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           displayBuffer->vulkanImage,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1,
                           &blitRegion,
                           VK_FILTER_LINEAR);
        }

        // display -> presentation
        {
            Presenter::Image presentationBuffer = presenter.GetCurrentPresentationBuffer();
            // gfx->PixelBufferBarrier(commandBuffer,
            //                         presentationBuffer,
            //                         ImageLayout::UNDEFINED,
            //                         ImageLayout::TRANSFER_DST_OPTIMAL);

            gfx->PixelBufferBarrier(commandBuffer,
                                    displayBuffer.get(),
                                    ImageLayout::TRANSFER_DST_OPTIMAL,
                                    ImageLayout::TRANSFER_SRC_OPTIMAL);

            VkImageBlit blitRegion{
                .srcSubresource = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = 0,
                    .baseArrayLayer = 0,
                    .layerCount = 1},
                .srcOffsets = {{0, 0, 0}, {static_cast<int32_t>(displayBuffer->width), static_cast<int32_t>(displayBuffer->height), 1}},
                .dstSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1},
                .dstOffsets = {{0, 0, 0}, {static_cast<int32_t>(presentationBuffer.width), static_cast<int32_t>(presentationBuffer.height), 1}}};

            vkCmdBlitImage(commandBuffer.vulkanHandle,
                           displayBuffer->vulkanImage,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           presentationBuffer.vulkanImage,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1,
                           &blitRegion,
                           VK_FILTER_LINEAR);
        }
    }
}