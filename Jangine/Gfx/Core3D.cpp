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
    Core3D::Core3D(Gfx::Core &gfx)
        : gfx(gfx),
          shapeVertexBuffer(nullptr, std::ref(gfx)),
          shapeInstanceBuffer(nullptr, std::ref(gfx)),
          shapeIndexBuffer(nullptr, std::ref(gfx)),
          perSceneBuffer(nullptr, std::ref(gfx)),
          perMeshBuffer(nullptr, std::ref(gfx)),
          perShapeMeshBuffer(nullptr, std::ref(gfx)),
          renderBuffer(nullptr, std::ref(gfx)),
          depthBuffer(nullptr, std::ref(gfx)),
          motionBuffer(nullptr, std::ref(gfx)),
          displayBuffer(nullptr, std::ref(gfx)),
          meshPipeline(nullptr, std::ref(gfx)),
          oitCompositionPipeline(nullptr, std::ref(gfx)),
          oitDataBuffer(nullptr, std::ref(gfx)),
          oitNodeBuffer(nullptr, std::ref(gfx)),
          oitNodeHeadBuffer(nullptr, std::ref(gfx)),
          shapePipeline(nullptr, std::ref(gfx)),
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

        // --------------------- SHAPES
        shapeVertexBuffer = gfx.CreateMemoryBuffer(
            4 * sizeof(ShapeVertex),
            MemoryBufferUsage::Vertex | MemoryBufferUsage::TransferDst,
            MemoryAccess::None);

        shapeInstanceBuffer = gfx.CreateMemoryBuffer(
            128 * sizeof(ShapeLineInstance),
            MemoryBufferUsage::Vertex | MemoryBufferUsage::TransferDst,
            MemoryAccess::Write);

        shapeIndexBuffer = gfx.CreateMemoryBuffer(
            6 * sizeof(uint32_t),
            MemoryBufferUsage::Index | MemoryBufferUsage::TransferDst,
            MemoryAccess::None);

        uint32_t quadIndices[6] = {0, 1, 2, 0, 2, 3};
        ShapeVertex quadVertices[4] = {
            {{1.0f, -1.0f, 0.0f}},
            {{1.0f, 1.0f, 0.0f}},
            {{-1.0f, 1.0f, 0.0f}},
            {{-1.0f, -1.0f, 0.0f}}};
        gfx.StageToMemoryBuffer(shapeVertexBuffer.get(), Span(quadVertices));
        gfx.StageToMemoryBuffer(shapeIndexBuffer.get(), Span(quadIndices));

        Eigen::Isometry3f worldTransformTEST = Eigen::Isometry3f::Identity();
        worldTransformTEST.linear() = Eigen::AngleAxisf(-Math::PI / 2.0f, Eigen::Vector3f::UnitX()).toRotationMatrix();
        worldTransformTEST.translation() = Eigen::Vector3f(0, 0, 0);

        Eigen::Isometry3f rotateToXZ = Eigen::Isometry3f::Identity();
        rotateToXZ.linear() = Eigen::AngleAxisf(-Math::PI / 2.0f, Eigen::Vector3f::UnitX()).toRotationMatrix();

        // For debugging, add some instances of a line
        std::vector<ShapeLineInstance> lineInstances = {
            {.Transform = worldTransformTEST.matrix().transpose(),
             .Start = Eigen::Vector3f(0, 0, 0),
             .End = Eigen::Vector3f(.5f, 0, 0),
             .ColorStart = Color::Red.ToUInt(),
             .ColorEnd = Color::Red.ToUInt(),
             .Thickness = 4.f,
             .ScaleSpace = 1,
             .Alignment = 1},
            {.Transform = worldTransformTEST.matrix().transpose(),
             .Start = Eigen::Vector3f(0, 0, 0),
             .End = Eigen::Vector3f(0, .5f, 0),
             .ColorStart = Color::Green.ToUInt(),
             .ColorEnd = Color::Green.ToUInt(),
             .Thickness = 4.f,
             .ScaleSpace = 1,
             .Alignment = 1},
            {.Transform = worldTransformTEST.matrix().transpose(),
             .Start = Eigen::Vector3f(0, 0, 0),
             .End = Eigen::Vector3f(0, 0, .5f),
             .ColorStart = Color::Blue.ToUInt(),
             .ColorEnd = Color::Blue.ToUInt(),
             .Thickness = 4.f,
             .ScaleSpace = 1,
             .Alignment = 1}};
        gfx.WriteMemoryBuffer(shapeInstanceBuffer.get(), Span(lineInstances));
        // --------------------- SHAPES

        perSceneBuffer = gfx.CreateMemoryBuffer(
            sizeof(PerSceneData),
            MemoryBufferUsage::Uniform,
            MemoryAccess::Write);

        perMeshBuffer = gfx.CreateMemoryBuffer(
            Math::AlignUp(sizeof(PerDrawData), gfx.GetCapabilities().uniformBufferOffsetAlignment) * 1024,
            MemoryBufferUsage::Uniform,
            MemoryAccess::Write);

        perShapeMeshBuffer = gfx.CreateMemoryBuffer(
            sizeof(PerLineMeshData) * 10,
            MemoryBufferUsage::Uniform,
            MemoryAccess::Write);

        /*Jangine::IO::Gltf::Model model{};
        Jangine::IO::Status modelStatus = Jangine::IO::Gltf::LoadFromFile("c:/users/jant/desktop/Skytrack.glb", model);
        //Jangine::IO::Status modelStatus = Jangine::IO::Gltf::LoadFromFile("c:/users/jant/desktop/CUBE.glb", model);
        if (modelStatus == Jangine::IO::Status::SUCCESS)
        {
            Jangine::IO::Gltf::Model cubeModel{};
            modelStatus = Jangine::IO::Gltf::LoadFromFile("c:/users/jant/desktop/Skytrack_CUBE.glb", cubeModel);
            if (modelStatus == Jangine::IO::Status::SUCCESS)
            {
                model.Append(cubeModel);
            }

            Import(model);

            Jangine::IO::Gltf::Model exportModel{};
            Export(exportModel);
            Jangine::IO::Status exportStatus = Jangine::IO::Gltf::SaveToFile("c:/users/jant/desktop/Skytrack_exported.glb", exportModel);
            if (exportStatus != Jangine::IO::Status::SUCCESS)
            {
                logger.Error("Failed to export GLTF model.", __func__);
            }

            Clear();

            Jangine::IO::Gltf::Model readbackModel{};
            Jangine::IO::Status readbackStatus = Jangine::IO::Gltf::LoadFromFile("c:/users/jant/desktop/Skytrack_exported.glb", readbackModel);
            if (readbackStatus != Jangine::IO::Status::SUCCESS)
            {
                logger.Error("Failed to read back exported GLTF model.", __func__);
            }

            Import(readbackModel);
        }*/

        
        Jangine::IO::Urdf::Model urdf = Jangine::IO::Urdf::Model{};
        Jangine::IO::Gltf::Model urdfGltf = Jangine::IO::Gltf::Model{};
        Jangine::IO::Status urdfStatus = Jangine::IO::Urdf::LoadFromFile("c:/users/jant/desktop/urdf_curated/skytrack/model.urdf", urdf, urdfGltf, [](const std::string &msg)
                                                                         { Jangine::Core::GetLogger("Gfx::Core3D").Warning(msg, "Urdf::LoadFromFile"); });
        // Jangine::IO::Status urdfStatus = Jangine::IO::Urdf::LoadFromFile("c:/users/jant/desktop/urdf_curated/process_center_vrwp_c/model.urdf", urdf, urdfGltf, [](const std::string &msg)
        //                                                                  { Jangine::Core::GetLogger("Gfx::Core3D").Warning(msg, "Urdf::LoadFromFile"); });
        if (urdfStatus == Jangine::IO::Status::SUCCESS)
        {
            Import(urdfGltf);

            // Transform the entire scene to match the URDF coordinate system (Z up)
            Eigen::Isometry3f worldTransform = Eigen::Isometry3f::Identity();
            worldTransform.linear() = Eigen::AngleAxisf(-Math::PI / 2.0f, Eigen::Vector3f::UnitX()).toRotationMatrix();
            worldTransform.translation() = Eigen::Vector3f(0, 0, 0);

            scene.SetRelativeTransform(scene.GetWorld().GetId(), worldTransform);

            Jangine::IO::Gltf::Model exportModel{};
            Export(exportModel);
            Jangine::IO::Status exportStatus = Jangine::IO::Gltf::SaveToFile("c:/users/jant/desktop/urdf_export.glb", exportModel);
            if (exportStatus != Jangine::IO::Status::SUCCESS)
            {
                logger.Error("Failed to export GLTF model.", __func__);
            }
            //
            // scene.Clear();
            //
            // Jangine::IO::Gltf::Model readbackModel{};
            // Jangine::IO::Status readbackStatus = Jangine::IO::Gltf::LoadFromFile("c:/users/jant/desktop/urdf_export.glb", readbackModel);
            // if (readbackStatus != Jangine::IO::Status::SUCCESS)
            //{
            //    logger.Error("Failed to read back exported GLTF model.", __func__);
            //}
            //
            // Import(readbackModel);
        }       
    }

    void Core3D::PrepareRender(const Presenter &presenter, const DisplayParameters &in_displayParameters)
    {
        bool_t antialiasingModeChanged = displayParameters.antialiasingMode != in_displayParameters.antialiasingMode;
        bool_t renderSizeChanged = displayParameters.renderSize != in_displayParameters.renderSize;
        bool_t displaySizeChanged = displayParameters.displaySize != in_displayParameters.displaySize;

        displayParameters = in_displayParameters;

        bool_t renderResourcesNull = !renderBuffer || !depthBuffer || !motionBuffer;
        bool_t initializeRender = renderResourcesNull || antialiasingModeChanged || renderSizeChanged;

        bool_t displayResourcesNull = !displayBuffer;
        bool_t initializeDisplay = displayResourcesNull || displaySizeChanged;

        if (initializeRender)
        {
            renderBuffer = gfx.CreatePixelBuffer(
                displayParameters.renderSize.width,
                displayParameters.renderSize.height,
                Format::RGBA32,
                PixelBufferUsage::ColorAttachment | PixelBufferUsage::Sampled |
                    PixelBufferUsage::TransferDst | PixelBufferUsage::TransferSrc,
                Aspect::Color);

            depthBuffer = gfx.CreatePixelBuffer(
                displayParameters.renderSize.width,
                displayParameters.renderSize.height,
                gfx.GetDeviceDepthFormat(),
                PixelBufferUsage::DepthAttachment | PixelBufferUsage::Sampled |
                    PixelBufferUsage::TransferDst | PixelBufferUsage::TransferSrc,
                Aspect::Depth);

            motionBuffer = gfx.CreatePixelBuffer(
                displayParameters.renderSize.width,
                displayParameters.renderSize.height,
                Format::R16G16,
                PixelBufferUsage::ColorAttachment | PixelBufferUsage::Sampled |
                    PixelBufferUsage::TransferDst | PixelBufferUsage::TransferSrc,
                Aspect::Color);

            // --------------------- OIT
            size_t oitNodeCount = displayParameters.renderSize.width * displayParameters.renderSize.height * MAX_OIT_NODES_PER_PIXEL;
            size_t oitStorageSize = sizeof(OITNode) * oitNodeCount;
            size_t maxStorageSize = gfx.GetCapabilities().maxStorageBufferRange;
            if (oitStorageSize > maxStorageSize)
            {
                oitNodeCount = maxStorageSize / sizeof(OITNode);
                logger.Warning(std::format("Requested OIT storage size {} exceeds device maximum storage buffer range {}. Reducing OIT node count to {}.", oitStorageSize, maxStorageSize, oitNodeCount), __func__);
            }

            oitDataBuffer = gfx.CreateMemoryBuffer(
                sizeof(OITData),
                MemoryBufferUsage::Storage | MemoryBufferUsage::TransferDst,
                MemoryAccess::None);
            OITData oitData = {
                .count = 0,
                .maxNodeCount = static_cast<uint32_t>(oitNodeCount)};
            gfx.StageToMemoryBuffer(oitDataBuffer.get(), Span(oitData));

            oitNodeBuffer = gfx.CreateMemoryBuffer(
                sizeof(OITNode) * oitNodeCount,
                MemoryBufferUsage::Storage,
                MemoryAccess::None);

            oitNodeHeadBuffer = gfx.CreatePixelBuffer(
                displayParameters.renderSize.width,
                displayParameters.renderSize.height,
                Format::U32,
                PixelBufferUsage::Storage | PixelBufferUsage::TransferDst | PixelBufferUsage::Sampled,
                Aspect::Color);
            // ---------------------
        }

        if (initializeDisplay)
        {
            displayBuffer = gfx.CreatePixelBuffer(
                displayParameters.displaySize.width,
                displayParameters.displaySize.height,
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
            shapePipelineParams.shaderProgram = ThrowInvalidOperationIfNull(gfx.GetShaderProgram("built-in-shape-line"),
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
                 .inputRate = VertexInputRate::VERTEX},
                {.binding = 1,
                 .stride = sizeof(ShapeLineInstance),
                 .inputRate = VertexInputRate::INSTANCE}};
            shapePipelineParams.vertexInputAttributes = {
                {.location = 0,
                 .binding = 0,
                 .format = Format::RGB32,
                 .offset = offsetof(ShapeVertex, Position)},
                {.location = 1,
                 .binding = 1,
                 .format = Format::RGBA32,
                 .offset = offsetof(ShapeLineInstance, Transform) + 0},
                {.location = 2,
                 .binding = 1,
                 .format = Format::RGBA32,
                 .offset = offsetof(ShapeLineInstance, Transform) + 16},
                {.location = 3,
                 .binding = 1,
                 .format = Format::RGBA32,
                 .offset = offsetof(ShapeLineInstance, Transform) + 32},
                {.location = 4,
                 .binding = 1,
                 .format = Format::RGBA32,
                 .offset = offsetof(ShapeLineInstance, Transform) + 48},
                {.location = 5,
                 .binding = 1,
                 .format = Format::RGB32,
                 .offset = offsetof(ShapeLineInstance, Start)},
                {.location = 6,
                 .binding = 1,
                 .format = Format::RGB32,
                 .offset = offsetof(ShapeLineInstance, End)},
                {.location = 7,
                 .binding = 1,
                 .format = Format::U32,
                 .offset = offsetof(ShapeLineInstance, ColorStart)},
                {.location = 8,
                 .binding = 1,
                 .format = Format::U32,
                 .offset = offsetof(ShapeLineInstance, ColorEnd)},
                {.location = 9,
                 .binding = 1,
                 .format = Format::R32,
                 .offset = offsetof(ShapeLineInstance, Thickness)},
                {.location = 10,
                 .binding = 1,
                 .format = Format::U32,
                 .offset = offsetof(ShapeLineInstance, ScaleSpace)},
                {.location = 11,
                 .binding = 1,
                 .format = Format::U32,
                 .offset = offsetof(ShapeLineInstance, Alignment)}};
            shapePipelineParams.attachments = {
                {.format = Format::RGBA32,
                 .blend = straightAlphaBlend}};

            shapePipeline = gfx.CreatePipeline(shapePipelineParams);
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
            meshPipelineParams.shaderProgram = ThrowInvalidOperationIfNull(gfx.GetShaderProgram("built-in-mesh"),
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
                 .stages = ShaderStage::VERTEX | ShaderStage::FRAGMENT},
                {.binding = 2,
                 .descriptorType = DescriptorType::STORAGE_BUFFER,
                 .descriptorCount = 1,
                 .stages = ShaderStage::FRAGMENT},
                {.binding = 3,
                 .descriptorType = DescriptorType::STORAGE_BUFFER,
                 .descriptorCount = 1,
                 .stages = ShaderStage::FRAGMENT},
                {.binding = 4,
                 .descriptorType = DescriptorType::STORAGE_BUFFER,
                 .descriptorCount = 1,
                 .stages = ShaderStage::FRAGMENT},
                {.binding = 5,
                 .descriptorType = DescriptorType::STORAGE_IMAGE,
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
                 .stride = sizeof(MeshBuffer::Vertex),
                 .inputRate = VertexInputRate::VERTEX}};
            meshPipelineParams.vertexInputAttributes = {
                {.location = 0,
                 .binding = 0,
                 .format = Format::RGB32,
                 .offset = offsetof(MeshBuffer::Vertex, position)},
                {.location = 1,
                 .binding = 0,
                 .format = Format::RGB32,
                 .offset = offsetof(MeshBuffer::Vertex, normal)},
                {.location = 2,
                 .binding = 0,
                 .format = Format::RG32,
                 .offset = offsetof(MeshBuffer::Vertex, uv)}};
            meshPipelineParams.attachments = {
                {.format = Format::RGBA32,
                 .blend = straightAlphaBlend}};

            meshPipeline = gfx.CreatePipeline(meshPipelineParams);

            meshPipelineParams.shaderProgram = ThrowInvalidOperationIfNull(gfx.GetShaderProgram("built-in-oit-composition"),
                                                                           "Shader program 'built-in-oit-composition' not found in cache.");
            meshPipelineParams.pushConstantRanges = {};
            meshPipelineParams.descriptorLayout = {
                {.binding = 0,
                 .descriptorType = DescriptorType::STORAGE_BUFFER,
                 .descriptorCount = 1,
                 .stages = ShaderStage::FRAGMENT},
                {.binding = 1,
                 .descriptorType = DescriptorType::SAMPLED_IMAGE,
                 .descriptorCount = 1,
                 .stages = ShaderStage::FRAGMENT},
                {.binding = 2,
                 .descriptorType = DescriptorType::SAMPLED_IMAGE,
                 .descriptorCount = 1,
                 .stages = ShaderStage::FRAGMENT}};
            meshPipelineParams.cullMode = CullMode::NONE;
            meshPipelineParams.frontFace = FrontFace::COUNTER_CLOCKWISE;
            meshPipelineParams.depthTestEnabled = false;
            meshPipelineParams.depthWriteEnabled = false;
            meshPipelineParams.depthCompareOp = CompareOp::LESS_OR_EQUAL;
            meshPipelineParams.vertexInputBindings = {};
            meshPipelineParams.vertexInputAttributes = {};
            meshPipelineParams.attachments = {
                {.format = Format::RGBA32,
                 .blend = straightAlphaBlend}};

            oitCompositionPipeline = gfx.CreatePipeline(meshPipelineParams);
        }

        auto &surfaceInfo = presenter.GetSurfaceInfo();
        float_t aspectRatio = static_cast<float_t>(surfaceInfo.width) / static_cast<float_t>(surfaceInfo.height);
        camera.SetOrthographic(aspectRatio, camera.GetOrthographicFoV(), -100.0f, 100.0f);
        //camera.SetPerspective(aspectRatio, Math::PI / 4.0f, 0.1f, 100.0f);
    }

    void Core3D::Render(const Presenter &presenter, double_t absoluteTime, float_t deltaTime)
    {
        auto &surfaceInfo = presenter.GetSurfaceInfo();

        VkExtent2D renderExtent = {.width = renderBuffer->width, .height = renderBuffer->height};

        float_t viewportX = 0.0f;
        float_t viewportY = 0.0f;
        float_t viewportWidth = static_cast<float_t>(renderExtent.width);
        float_t viewportHeight = static_cast<float_t>(renderExtent.height);

        Rectangle currentViewport = gfx.GetViewport();
        viewportX = Math::Map(currentViewport.left, 0.0f, static_cast<float_t>(surfaceInfo.width), 0.0f, static_cast<float_t>(renderBuffer->width));
        viewportY = Math::Map(currentViewport.top, 0.0f, static_cast<float_t>(surfaceInfo.height), 0.0f, static_cast<float_t>(renderBuffer->height));
        viewportWidth = Math::Map(currentViewport.width(), 0.0f, static_cast<float_t>(surfaceInfo.width), 0.0f, static_cast<float_t>(renderBuffer->width));
        viewportHeight = Math::Map(currentViewport.height(), 0.0f, static_cast<float_t>(surfaceInfo.height), 0.0f, static_cast<float_t>(renderBuffer->height));

        viewportX = std::clamp(viewportX, 0.0f, static_cast<float_t>(renderExtent.width));
        viewportY = std::clamp(viewportY, 0.0f, static_cast<float_t>(renderExtent.height));
        viewportWidth = std::clamp(viewportWidth, 0.0f, static_cast<float_t>(renderExtent.width) - viewportX);
        viewportHeight = std::clamp(viewportHeight, 0.0f, static_cast<float_t>(renderExtent.height) - viewportY);

        VkViewport viewport{
            .x = viewportX,
            .y = viewportY + viewportHeight,
            .width = viewportWidth,
            .height = -viewportHeight,
            .minDepth = 0.0f,
            .maxDepth = 1.0f};
        VkRect2D scissor{{0, 0}, {renderExtent.width, renderExtent.height}};

        camera.Update(gfx.GetUserInput(), deltaTime);

        scene.Update(absoluteTime, deltaTime);

        PerSceneData sceneData{};
        sceneData.ViewProjection = camera.GetViewProjectionMatrix();
        sceneData.View = camera.GetViewMatrix();
        sceneData.ViewInverse = camera.GetInverseViewMatrix();
        sceneData.ViewPosition = camera.GetPosition();
        sceneData.Screen = Eigen::Vector2f(
            static_cast<float_t>(displayParameters.renderSize.width),
            static_cast<float_t>(displayParameters.renderSize.height));

        gfx.WriteMemoryBuffer(perSceneBuffer.get(), Span(sceneData));

        CommandBuffer commandBuffer = presenter.GetCurrentCommandBuffer();

        gfx.PixelBufferBarrier(commandBuffer,
                               renderBuffer.get(),
                               ImageLayout::UNDEFINED,
                               ImageLayout::COLOR_ATTACHMENT_OPTIMAL);

        gfx.PixelBufferBarrier(commandBuffer,
                               depthBuffer.get(),
                               ImageLayout::UNDEFINED,
                               ImageLayout::DEPTH_ATTACHMENT_OPTIMAL);

        gfx.PixelBufferBarrier(commandBuffer,
                               motionBuffer.get(),
                               ImageLayout::UNDEFINED,
                               ImageLayout::COLOR_ATTACHMENT_OPTIMAL);

        // --------------------- OIT
        gfx.PixelBufferBarrier(commandBuffer,
                               oitNodeHeadBuffer.get(),
                               ImageLayout::UNDEFINED,
                               ImageLayout::GENERAL);

        gfx.ClearPixelBuffer(commandBuffer,
                             oitNodeHeadBuffer.get(),
                             0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
                             ImageLayout::GENERAL);

        gfx.FillBuffer(commandBuffer, oitDataBuffer.get(), 0, 0, sizeof(uint32_t));

        gfx.TransferBarrier(commandBuffer);
        // ---------------------

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
                          1.0f}}};

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

        PerDrawData perDrawData{};
        uint32_t drawCount = 0;

        const auto &nodes = scene.GetNodes();
        for (size_t i = 1; i < nodes.size(); i++)
        {
            const Node &node = nodes[i];
            Mesh::Id associatedMeshId = node.GetMeshId();

            if (!associatedMeshId)
                continue;

            perDrawData.Transform = node.GetWorldTransform().matrix();

            auto overrideColor = node.GetOverrideColor();
            bool_t hasOverrideColor = overrideColor.has_value();
            bool_t overrideColorIsTransparent = hasOverrideColor && (overrideColor->a < 1.0f);
            perDrawData.Color = hasOverrideColor ? overrideColor->ToUInt() : 0xffffffff;

            const Mesh *mesh = scene.GetMesh(associatedMeshId);
            const MeshBuffer *meshBuffer = mesh->GetBuffer();

            gfx.BindBuffers(commandBuffer, meshBuffer->GetVertexBuffer(), meshBuffer->GetIndexBuffer());

            for (const auto &primitive : mesh->GetPrimitives())
            {
                perDrawData.MaterialIndex = hasOverrideColor ? -1 : primitive.materialIndex;

                bool_t primitiveIsTransparent = primitive.materialHasTransparency || overrideColorIsTransparent;
                vkCmdSetDepthWriteEnable(commandBuffer.vulkanHandle, !primitiveIsTransparent);

                size_t perMeshOffset = drawCount++ * Math::AlignUp(sizeof(PerDrawData), gfx.GetCapabilities().uniformBufferOffsetAlignment);
                gfx.WriteMemoryBuffer(perMeshBuffer.get(), Span(perDrawData), perMeshOffset);

                VkDescriptorBufferInfo perSceneBufferInfo{
                    .buffer = perSceneBuffer->vulkanBuffer,
                    .offset = 0,
                    .range = sizeof(PerSceneData)};
                VkDescriptorBufferInfo perMeshBufferInfo{
                    .buffer = perMeshBuffer->vulkanBuffer,
                    .offset = perMeshOffset,
                    .range = sizeof(PerDrawData)};
                VkDescriptorBufferInfo perMaterialBufferInfo{
                    .buffer = meshBuffer->GetMaterialBuffer()->vulkanBuffer,
                    .offset = 0,
                    .range = VK_WHOLE_SIZE};
                VkDescriptorBufferInfo perOITDataBufferInfo{
                    .buffer = oitDataBuffer->vulkanBuffer,
                    .offset = 0,
                    .range = VK_WHOLE_SIZE};
                VkDescriptorBufferInfo perOITNodeBufferInfo{
                    .buffer = oitNodeBuffer->vulkanBuffer,
                    .offset = 0,
                    .range = VK_WHOLE_SIZE};
                VkDescriptorImageInfo perOITHeadBuffer{
                    .sampler = VK_NULL_HANDLE,
                    .imageView = oitNodeHeadBuffer->vulkanImageView,
                    .imageLayout = VK_IMAGE_LAYOUT_GENERAL};

                VkWriteDescriptorSet descriptorWrites[6] = {
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
                     .pTexelBufferView = nullptr},
                    {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                     .pNext = nullptr,
                     .dstSet = VK_NULL_HANDLE,
                     .dstBinding = 3,
                     .dstArrayElement = 0,
                     .descriptorCount = 1,
                     .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                     .pImageInfo = nullptr,
                     .pBufferInfo = &perOITDataBufferInfo,
                     .pTexelBufferView = nullptr},
                    {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                     .pNext = nullptr,
                     .dstSet = VK_NULL_HANDLE,
                     .dstBinding = 4,
                     .dstArrayElement = 0,
                     .descriptorCount = 1,
                     .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                     .pImageInfo = nullptr,
                     .pBufferInfo = &perOITNodeBufferInfo,
                     .pTexelBufferView = nullptr},
                    {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                     .pNext = nullptr,
                     .dstSet = VK_NULL_HANDLE,
                     .dstBinding = 5,
                     .dstArrayElement = 0,
                     .descriptorCount = 1,
                     .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                     .pImageInfo = &perOITHeadBuffer,
                     .pBufferInfo = nullptr,
                     .pTexelBufferView = nullptr}};

                gfx.PushDescriptorSets(
                    commandBuffer,
                    meshPipeline.get(),
                    std::size(descriptorWrites),
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

        gfx.PixelBufferBarrier(commandBuffer,
                               depthBuffer.get(),
                               ImageLayout::DEPTH_ATTACHMENT_OPTIMAL,
                               ImageLayout::SHADER_READ_ONLY_OPTIMAL);

        // --------------------- OIT
#if 1
        {
            colorAttachment = {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .pNext = nullptr,
                .imageView = renderBuffer->vulkanImageView,
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
                .renderArea = {{0, 0}, {renderExtent.width, renderExtent.height}},
                .layerCount = 1,
                .viewMask = 0,
                .colorAttachmentCount = 1,
                .pColorAttachments = colorAttachments,
                .pDepthAttachment = nullptr,
                .pStencilAttachment = nullptr};

            vkCmdBeginRendering(commandBuffer.vulkanHandle, &renderingInfo);
            vkCmdBindPipeline(commandBuffer.vulkanHandle, VK_PIPELINE_BIND_POINT_GRAPHICS, oitCompositionPipeline->vulkanHandle);
            vkCmdSetViewport(commandBuffer.vulkanHandle, 0, 1, &viewport);
            vkCmdSetScissor(commandBuffer.vulkanHandle, 0, 1, &scissor);
            vkCmdSetDepthWriteEnable(commandBuffer.vulkanHandle, false);

            VkDescriptorBufferInfo perOITNodeBufferInfo{
                .buffer = oitNodeBuffer->vulkanBuffer,
                .offset = 0,
                .range = VK_WHOLE_SIZE};
            VkDescriptorImageInfo perOITHeadBuffer{
                .sampler = VK_NULL_HANDLE,
                .imageView = oitNodeHeadBuffer->vulkanImageView,
                .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
            VkDescriptorImageInfo perDepthBuffer{
                .sampler = VK_NULL_HANDLE,
                .imageView = depthBuffer->vulkanImageView,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

            VkWriteDescriptorSet descriptorWrites[3] = {
                {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                 .pNext = nullptr,
                 .dstSet = VK_NULL_HANDLE,
                 .dstBinding = 0,
                 .dstArrayElement = 0,
                 .descriptorCount = 1,
                 .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                 .pImageInfo = nullptr,
                 .pBufferInfo = &perOITNodeBufferInfo,
                 .pTexelBufferView = nullptr},
                {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                 .pNext = nullptr,
                 .dstSet = VK_NULL_HANDLE,
                 .dstBinding = 1,
                 .dstArrayElement = 0,
                 .descriptorCount = 1,
                 .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                 .pImageInfo = &perOITHeadBuffer,
                 .pBufferInfo = nullptr,
                 .pTexelBufferView = nullptr},
                {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                 .pNext = nullptr,
                 .dstSet = VK_NULL_HANDLE,
                 .dstBinding = 2,
                 .dstArrayElement = 0,
                 .descriptorCount = 1,
                 .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                 .pImageInfo = &perDepthBuffer,
                 .pBufferInfo = nullptr,
                 .pTexelBufferView = nullptr}};
            gfx.PushDescriptorSets(
                commandBuffer,
                oitCompositionPipeline.get(),
                std::size(descriptorWrites),
                descriptorWrites);

            vkCmdDraw(commandBuffer.vulkanHandle, 3, 1, 0, 0);

            vkCmdEndRendering(commandBuffer.vulkanHandle);
        }
#endif

        // --------------------- SHAPES
        {
            colorAttachment = {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .pNext = nullptr,
                .imageView = renderBuffer->vulkanImageView,
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
            vkCmdSetDepthWriteEnable(commandBuffer.vulkanHandle, false);

            VkDescriptorBufferInfo perSceneBufferInfo{
                .buffer = perSceneBuffer->vulkanBuffer,
                .offset = 0,
                .range = sizeof(PerSceneData)};

            VkWriteDescriptorSet descriptorWrites[1] = {
                {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                 .pNext = nullptr,
                 .dstSet = VK_NULL_HANDLE,
                 .dstBinding = 0,
                 .dstArrayElement = 0,
                 .descriptorCount = 1,
                 .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                 .pImageInfo = nullptr,
                 .pBufferInfo = &perSceneBufferInfo,
                 .pTexelBufferView = nullptr}};

            gfx.PushDescriptorSets(
                commandBuffer,
                shapePipeline.get(),
                std::size(descriptorWrites),
                descriptorWrites);

            VkDeviceSize vtxBufferOffset = 0;
            vkCmdBindVertexBuffers(commandBuffer.vulkanHandle, 0, 1, &shapeVertexBuffer->vulkanBuffer, &vtxBufferOffset);
            vkCmdBindVertexBuffers(commandBuffer.vulkanHandle, 1, 1, &shapeInstanceBuffer->vulkanBuffer, &vtxBufferOffset);
            vkCmdBindIndexBuffer(commandBuffer.vulkanHandle, shapeIndexBuffer->vulkanBuffer, 0, VK_INDEX_TYPE_UINT32);

            vkCmdDrawIndexed(commandBuffer.vulkanHandle, 6, 3, 0, 0, 0);

            vkCmdEndRendering(commandBuffer.vulkanHandle);
        }
        // --------------------- SHAPES

        // render -> display
        {
            gfx.PixelBufferBarrier(commandBuffer,
                                   displayBuffer.get(),
                                   ImageLayout::UNDEFINED,
                                   ImageLayout::TRANSFER_DST_OPTIMAL);
            gfx.PixelBufferBarrier(commandBuffer,
                                   renderBuffer.get(),
                                   ImageLayout::COLOR_ATTACHMENT_OPTIMAL,
                                   ImageLayout::TRANSFER_SRC_OPTIMAL);
            gfx.BlitPixelBuffer(commandBuffer,
                                renderBuffer.get(),
                                displayBuffer.get(),
                                BlitFilter::LINEAR,
                                ImageLayout::TRANSFER_SRC_OPTIMAL,
                                ImageLayout::TRANSFER_DST_OPTIMAL);
        }

        // display -> presentation
        {
            Presenter::Image presentationBuffer = presenter.GetCurrentPresentationBuffer();

            gfx.PixelBufferBarrier(commandBuffer,
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

    void Animation::Apply(Scene &scene)
    {
        for (const auto &channel : channels)
        {
            Node::Id nodeId = channel.targetNode;
            scene.AssertId(nodeId);
            Node &node = scene.GetNode(nodeId);

            switch (channel.targetPath)
            {
            case Channel::Path::TRANSLATION:
            {
                Eigen::Vector3f sampledTranslation = channel.sampler.Sample<Eigen::Vector3f>(currentTime, Animation::Vector3Value, Animation::Vector3Lerp);

                Eigen::Isometry3f newRelativeTransform = node.GetRelativeTransform();
                newRelativeTransform.translation() = sampledTranslation;
                node.SetRelativeTransform(newRelativeTransform);
                break;
            }
            case Channel::Path::ROTATION:
            {
                Eigen::Quaternionf sampledRotation = channel.sampler.Sample<Eigen::Quaternionf>(currentTime, Animation::QuaternionValue, Animation::QuaternionSlerp);

                Eigen::Isometry3f newRelativeTransform = node.GetRelativeTransform();
                newRelativeTransform.linear() = sampledRotation.toRotationMatrix();
                node.SetRelativeTransform(newRelativeTransform);
                break;
            }
            case Channel::Path::COLOR:
            {
                Color sampledColor = channel.sampler.Sample<Color>(currentTime, Animation::ColorValue, Animation::ColorLerp);
                node.SetOverrideColor(sampledColor);
                break;
            }
            default:
                break;
            }
        }
    }
}