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
          shapeVertexBuffer(nullptr, std::ref(*gfx)),
          shapeIndexBuffer(nullptr, std::ref(*gfx)),
          perSceneBuffer(nullptr, std::ref(*gfx)),
          perMeshBuffer(nullptr, std::ref(*gfx)),
          perShapeMeshBuffer(nullptr, std::ref(*gfx)),
          renderBuffer(nullptr, std::ref(*gfx)),
          depthBuffer(nullptr, std::ref(*gfx)),
          motionBuffer(nullptr, std::ref(*gfx)),
          displayBuffer(nullptr, std::ref(*gfx)),
          meshPipeline(nullptr, std::ref(*gfx)),
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

        shapeVertexBuffer = gfx->CreateMemoryBuffer(
            MAX_VERTICES * sizeof(ShapeVertex),
            MemoryBufferUsage::Vertex,
            MemoryAccess::Write);

        shapeIndexBuffer = gfx->CreateMemoryBuffer(
            MAX_INDICES * sizeof(uint16_t),
            MemoryBufferUsage::Index,
            MemoryAccess::Write);

        perSceneBuffer = gfx->CreateMemoryBuffer(
            sizeof(PerSceneData),
            MemoryBufferUsage::Uniform,
            MemoryAccess::Write);

        perMeshBuffer = gfx->CreateMemoryBuffer(
            Math::AlignUp<uint32_t>(sizeof(PerMeshData), gfx->GetCapabilities().uniformBufferOffsetAlignment) * 1024,
            MemoryBufferUsage::Uniform,
            MemoryAccess::Write);

        perShapeMeshBuffer = gfx->CreateMemoryBuffer(
            sizeof(PerDiscMeshData) * 10,
            MemoryBufferUsage::Uniform,
            MemoryAccess::Write);

        Gfx::IO::Gltf::Model model{};
        Gfx::IO::Status modelStatus = Gfx::IO::Gltf::LoadFromFile("c:/users/jant/desktop/VRWP-C.glb", model);

        if (modelStatus == Gfx::IO::Status::SUCCESS)
        {
            Add(model);
        }
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
            shapePipelineParams.cullMode = CullMode::BACK;
            shapePipelineParams.frontFace = FrontFace::CLOCKWISE;
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

        if (!meshPipeline)
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

            PipelineParameters meshPipelineParams;
            meshPipelineParams.shaderProgram = ThrowInvalidOperationIfNull(gfx->GetShaderProgram("built-in-mesh"),
                                                                           "Shader program 'built-in-mesh' not found in cache.");
            meshPipelineParams.pushConstantRanges = {};
            meshPipelineParams.descriptorLayout = {
                {.binding = 0,
                 .descriptorType = DescriptorType::UNIFORM_BUFFER,
                 .descriptorCount = 1,
                 .stages = ShaderStage::VERTEX | ShaderStage::FRAGMENT},
                {.binding = 1,
                 .descriptorType = DescriptorType::UNIFORM_BUFFER,
                 .descriptorCount = 1,
                 .stages = ShaderStage::VERTEX},
                {.binding = 2,
                 .descriptorType = DescriptorType::STORAGE_BUFFER,
                 .descriptorCount = 1,
                 .stages = ShaderStage::FRAGMENT}};
            meshPipelineParams.topology = PrimitiveTopology::TRIANGLE_LIST;
            meshPipelineParams.cullMode = CullMode::NONE;
            meshPipelineParams.frontFace = FrontFace::COUNTER_CLOCKWISE;
            meshPipelineParams.depthTestEnabled = true;
            meshPipelineParams.depthWriteEnabled = true;
            meshPipelineParams.depthCompareOp = CompareOp::LESS_OR_EQUAL;
            meshPipelineParams.vertexInputBindings = {
                {.binding = 0,
                 .stride = sizeof(MeshVertex),
                 .inputRate = VertexInputRate::VERTEX}};
            meshPipelineParams.vertexInputAttributes = {
                {.location = 0,
                 .binding = 0,
                 .format = Format::RGB32,
                 .offset = offsetof(MeshVertex, position)},
                {.location = 1,
                 .binding = 0,
                 .format = Format::RGB32,
                 .offset = offsetof(MeshVertex, normal)},
                {.location = 2,
                 .binding = 0,
                 .format = Format::RG32,
                 .offset = offsetof(MeshVertex, uv)}};
            meshPipelineParams.attachments = {
                {.format = Format::RGBA32,
                 .blend = straightAlphaBlend}};

            meshPipeline = gfx->CreatePipeline(meshPipelineParams);
        }

        camera.SetOrthographic(displayParameters.GetAspectRatio(), 10, -100.0f, 100.0f);
        //camera.SetPerspective(displayParameters.GetAspectRatio(), Math::PI / 4.0f, 0.0f, 100.0f);
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

        camera.Update(gfx->GetUserInput(), deltaTime);

        scene.Update();

        PerSceneData sceneData{};
        sceneData.ViewProjection = camera.GetViewProjectionMatrix();
        sceneData.View = camera.GetViewMatrix();
        sceneData.ViewInverse = camera.GetInverseViewMatrix();
        sceneData.ViewPosition = camera.GetPosition();
        sceneData.Screen = Eigen::Vector2f(
            static_cast<float_t>(displayParameters.renderWidth),
            static_cast<float_t>(displayParameters.renderHeight));

        std::span<const std::byte> perSceneDataSpan(reinterpret_cast<const std::byte *>(&sceneData), sizeof(sceneData));
        gfx->WriteMemoryBuffer(perSceneBuffer.get(), perSceneDataSpan);

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
        VkRenderingAttachmentInfo depthAttachment{
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext = nullptr,
            .imageView = depthBuffer->vulkanImageView,
            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            .resolveMode = VK_RESOLVE_MODE_NONE,
            .resolveImageView = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = VkClearValue{
                .depthStencil = {1.0f, 0}}};

        VkRenderingInfo renderingInfo{
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .pNext = nullptr,
            .flags = 0,
            .renderArea = {{0, 0}, {renderExtent.width, renderExtent.height}},
            .layerCount = 1,
            .viewMask = 0,
            .colorAttachmentCount = 1,
            .pColorAttachments = colorAttachments,
            .pDepthAttachment = &depthAttachment,
            .pStencilAttachment = nullptr};

        vkCmdBeginRendering(commandBuffer.vulkanHandle, &renderingInfo);
        vkCmdBindPipeline(commandBuffer.vulkanHandle, VK_PIPELINE_BIND_POINT_GRAPHICS, meshPipeline->vulkanHandle);
        vkCmdSetViewport(commandBuffer.vulkanHandle, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer.vulkanHandle, 0, 1, &scissor);

        PerMeshData perMeshData{};
        uint32_t drawCount = 0;

        const auto &nodes = scene.GetNodes();
        for (size_t i = 1; i < nodes.size(); i++)
        {
            const Node *node = nodes[i];

            if (node->GetType() != Node::Type::Mesh)
                continue;

            const MeshNode *meshNode = dynamic_cast<const MeshNode *>(node);
            const Mesh *mesh = scene.GetMesh(meshNode->GetMeshId());
            const MeshBuffer *meshBuffer = mesh->GetBuffer();

            VkDeviceSize vtxBufferOffset = 0;
            VkBuffer vtxBuffers[] = {meshBuffer->GetVertexBuffer()->vulkanBuffer};
            vkCmdBindVertexBuffers(commandBuffer.vulkanHandle, 0, 1, vtxBuffers, &vtxBufferOffset);
            vkCmdBindIndexBuffer(commandBuffer.vulkanHandle, meshBuffer->GetIndexBuffer()->vulkanBuffer, 0, VK_INDEX_TYPE_UINT32);

            for (const auto &primitive : mesh->GetPrimitives())
            {
                perMeshData.Transform = node->GetWorldTransform().matrix();
                perMeshData.MaterialIndex = primitive.materialIndex;

                // TODO: Fix alignment issues
                std::span<const std::byte> perMeshDataSpan(reinterpret_cast<const std::byte *>(&perMeshData), sizeof(perMeshData));
                size_t alignment = gfx->GetCapabilities().uniformBufferOffsetAlignment;
                size_t offset = drawCount++ * Math::AlignUp(sizeof(PerMeshData), alignment);
                gfx->WriteMemoryBuffer(perMeshBuffer.get(), perMeshDataSpan, static_cast<uint32_t>(offset));

                VkDescriptorBufferInfo perSceneBufferInfo{
                    .buffer = perSceneBuffer->vulkanBuffer,
                    .offset = 0,
                    .range = sizeof(PerSceneData)};

                VkDescriptorBufferInfo perMeshBufferInfo{
                    .buffer = perMeshBuffer->vulkanBuffer,
                    .offset = offset,
                    .range = sizeof(PerMeshData)};

                VkDescriptorBufferInfo perMaterialBufferInfo{
                    .buffer = meshBuffer->GetMaterialBuffer()->vulkanBuffer,
                    .offset = 0,
                    .range = VK_WHOLE_SIZE};

                VkWriteDescriptorSet descriptorWrites[3] = {
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
                     .pTexelBufferView = nullptr},
                    {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                     .pNext = nullptr,
                     .dstSet = VK_NULL_HANDLE,
                     .dstBinding = 2,
                     .dstArrayElement = 0,
                     .descriptorCount = 1,
                     .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                     .pImageInfo = nullptr,
                     .pBufferInfo = &perMaterialBufferInfo,
                     .pTexelBufferView = nullptr}};

                gfx->PushDescriptorSets(
                    commandBuffer,
                    meshPipeline.get(),
                    3,
                    descriptorWrites);

                vkCmdDrawIndexed(
                    commandBuffer.vulkanHandle,
                    primitive.indexCount,
                    1,
                    primitive.indexOffset,
                    0,
                    0);
            }
        }

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

/*{
    Eigen::Isometry3f testIsometry;
    testIsometry.setIdentity();
    testIsometry.linear() = Eigen::AngleAxisf(0.0f, Eigen::Vector3f::UnitY()).toRotationMatrix();
    testIsometry.translation() = Eigen::Vector3f(0.0f, 0.0f, 0.0f);

    PerDiscMeshData meshData{};
    meshData.Transform = testIsometry.matrix();
    meshData.Alignment = 0; // 0 = no alignment, 1 = billboard
    meshData.Color = Eigen::Vector4f(1.0f, 1.0f, 1.0f, 1.0f);
    meshData.Thickness = 0.01f;

    // meshData.ColorEnd = Eigen::Vector4f(1.0f, 1.0f, 1.0f, 1.0f);
    // meshData.Start = Eigen::Vector3f(0.0f, 0.0f, 0.0f);
    // meshData.End = Eigen::Vector3f(252.0f, 0.1f, 10.0f);

    meshData.AngleStart = 0.0f;
    meshData.AngleEnd = 0.0f;
    meshData.ColorInnerEnd = Eigen::Vector4f(1.0f, 1.0f, 1.0f, 1.0f);
    meshData.ColorOuterStart = Eigen::Vector4f(1.0f, 1.0f, 1.0f, 1.0f);
    meshData.ColorOuterEnd = Eigen::Vector4f(1.0f, 1.0f, 1.0f, 1.0f);
    meshData.Radius = 0.5f;

    std::span<const std::byte> perMeshDataSpan(reinterpret_cast<const std::byte *>(&meshData), sizeof(meshData));
    gfx->WriteMemoryBuffer(perShapeMeshBuffer.get(), perMeshDataSpan);

    // Add a single quad
    uint16_t quadIndices[6] = {0, 1, 2, 0, 2, 3};
    ShapeVertex quadVertices[4] = {
        {{1.0f, -1.0f, 0.0f}, {-1.0f, -1.0f}},
        {{1.0f, 1.0f, 0.0f}, {-1.0f, 1.0f}},
        {{-1.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
        {{-1.0f, -1.0f, 0.0f}, {1.0f, -1.0f}}};
    std::span<const std::byte> vertexDataSpan(reinterpret_cast<const std::byte *>(quadVertices), sizeof(quadVertices));
    gfx->WriteMemoryBuffer(shapeVertexBuffer.get(), vertexDataSpan);

    std::span<const std::byte> indexDataSpan(reinterpret_cast<const std::byte *>(quadIndices), sizeof(quadIndices));
    gfx->WriteMemoryBuffer(shapeIndexBuffer.get(), indexDataSpan);
}*/