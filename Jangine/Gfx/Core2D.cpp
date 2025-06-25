#include "Core2d.hpp"
#include "Core.hpp"

namespace Jangine::Gfx
{
    Core2D::Core2D(Gfx::Core *gfx)
        : gfx(gfx),
          logger(Jangine::Core::GetLogger("Gfx::Core2D")),
          backBuffer(nullptr, std::ref(*gfx)),
          renderPipeline(nullptr, std::ref(*gfx)),
          compositePipeline(nullptr, std::ref(*gfx))
    {
        logger.Func(__func__);
    }

    Core2D::~Core2D()
    {
        logger.Func(__func__);
    }

    void Core2D::Create()
    {
    }

    void Core2D::DestroyRendering()
    {
        if (backBuffer)
        {
            backBuffer.reset();
        }
    }

    void Core2D::InitializeRendering(const DisplayParameters &wantedDisplayParameters)
    {
        logger.Func(__func__);

        // DestroyRendering();

        backBuffer = gfx->CreatePixelBuffer(
            wantedDisplayParameters.displayWidth,
            wantedDisplayParameters.displayHeight,
            wantedDisplayParameters.displayFormat,
            PixelBufferUsage::ColorAttachment | PixelBufferUsage::Sampled |
                PixelBufferUsage::TransferDst | PixelBufferUsage::TransferSrc);

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
             .stride = sizeof(Vertex),
             .inputRate = VertexInputRate::VERTEX}};
        mainParams.vertexInputAttributes = {
            {.location = 0,
             .binding = 0,
             .format = Format::RGB32,
             .offset = offsetof(Vertex, position)},
            {.location = 1,
             .binding = 0,
             .format = Format::RG32,
             .offset = offsetof(Vertex, uv)},
            {.location = 2,
             .binding = 0,
             .format = Format::RGBA32,
             .offset = offsetof(Vertex, color)}};
        mainParams.attachments = {
            {.format = Format::BGRA8,
             .blend = straightAlphaBlend}};

        renderPipeline = gfx->CreatePipeline(mainParams);

        // Composition pipeline (no vertex input)
        PipelineParameters compParams;
        compParams.shaderProgram = ThrowInvalidOperationIfNull(gfx->GetShaderProgram("built-in-2d-composition"),
                                                               "Shader program 'built-in-2d-composition' not found in cache.");
        compParams.pushConstantRanges = {};
        compParams.descriptorLayout = {
            {.binding = 0,
             .descriptorType = DescriptorType::SAMPLED_IMAGE,
             .descriptorCount = 1,
             .stages = ShaderStage::FRAGMENT},
            {.binding = 1,
             .descriptorType = DescriptorType::SAMPLER,
             .descriptorCount = 1,
             .stages = ShaderStage::FRAGMENT}};
        compParams.topology = PrimitiveTopology::TRIANGLE_LIST;
        compParams.cullMode = CullMode::NONE;
        compParams.frontFace = FrontFace::COUNTER_CLOCKWISE;
        compParams.depthTestEnabled = false;
        compParams.depthWriteEnabled = false;
        compParams.depthCompareOp = CompareOp::ALWAYS;
        compParams.vertexInputBindings = {};
        compParams.vertexInputAttributes = {};
        compParams.attachments = {
            {.format = Format::BGRA8,
             .blend = straightAlphaBlend}};

        compositePipeline = gfx->CreatePipeline(compParams);
    }

    void Core2D::Render(const Presenter &presenter)
    {
        (void)presenter; // Avoid unused parameter warning
    }

}