#pragma once

#include "PipelineSpec.h"
#include "RenderPass.h"
#include "ShaderProgram.h"
#include "../renderable/RenderUtils.h"
#include <array>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

class OpaqueMeshPass : public RenderPass {
public:
  explicit OpaqueMeshPass(PipelineSpec pipelineSpec)
      : spec(std::move(pipelineSpec)) {}

  void initialize(DeviceContext &deviceContext,
                  SwapchainContext &swapchainContext) override {
    createDescriptorSetLayout(deviceContext);
    createGraphicsPipeline(deviceContext, swapchainContext);
    createColorResources(deviceContext, swapchainContext);
    createDepthResources(deviceContext, swapchainContext);
  }

  void recreate(DeviceContext &deviceContext,
                SwapchainContext &swapchainContext) override {
    createGraphicsPipeline(deviceContext, swapchainContext);
    createColorResources(deviceContext, swapchainContext);
    createDepthResources(deviceContext, swapchainContext);
  }

  void record(vk::raii::CommandBuffer &commandBuffer,
              SwapchainContext &swapchainContext,
              const std::vector<RenderItem> &renderItems, uint32_t frameIndex,
              uint32_t imageIndex) override {
    commandBuffer.begin({});

    transitionImageLayout(
        commandBuffer, swapchainContext.swapchainImages()[imageIndex],
        vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
        {}, vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::ImageAspectFlagBits::eColor);

    transitionImageLayout(commandBuffer, *colorImage,
                          vk::ImageLayout::eUndefined,
                          vk::ImageLayout::eColorAttachmentOptimal,
                          vk::AccessFlagBits2::eColorAttachmentWrite,
                          vk::AccessFlagBits2::eColorAttachmentWrite,
                          vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                          vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                          vk::ImageAspectFlagBits::eColor);

    transitionImageLayout(commandBuffer, *depthImage,
                          vk::ImageLayout::eUndefined,
                          vk::ImageLayout::eDepthAttachmentOptimal,
                          vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
                          vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
                          vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                              vk::PipelineStageFlagBits2::eLateFragmentTests,
                          vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                              vk::PipelineStageFlagBits2::eLateFragmentTests,
                          vk::ImageAspectFlagBits::eDepth);

    vk::ClearValue clearColor = {
        .color = {.float32 = std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}}};
    vk::ClearValue clearDepth = {
        .depthStencil = vk::ClearDepthStencilValue{1.0f, 0}};

    vk::RenderingAttachmentInfo colorAttachment = {
        .imageView = colorImageView,
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .resolveMode = vk::ResolveModeFlagBits::eAverage,
        .resolveImageView = swapchainContext.swapchainImageViews()[imageIndex],
        .resolveImageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .clearValue = clearColor};

    vk::RenderingAttachmentInfo depthAttachment = {
        .imageView = depthImageView,
        .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eDontCare,
        .clearValue = clearDepth};

    vk::RenderingInfo renderingInfo = {
        .renderArea = {.offset = {0, 0}, .extent = swapchainContext.extent2D()},
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachment,
        .pDepthAttachment = &depthAttachment};

    commandBuffer.beginRendering(renderingInfo);
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                               *graphicsPipeline);
    commandBuffer.setViewport(
        0, vk::Viewport(0.0f, 0.0f,
                        static_cast<float>(swapchainContext.extent2D().width),
                        static_cast<float>(swapchainContext.extent2D().height),
                        0.0f, 1.0f));
    commandBuffer.setScissor(
        0, vk::Rect2D(vk::Offset2D(0, 0), swapchainContext.extent2D()));

    for (const auto &renderItem : renderItems) {
      commandBuffer.bindVertexBuffers(0, *renderItem.mesh->getVertexBuffer(), {0});
      commandBuffer.bindIndexBuffer(*renderItem.mesh->getIndexBuffer(), 0,
                                    vk::IndexType::eUint32);
      commandBuffer.bindDescriptorSets(
          vk::PipelineBindPoint::eGraphics, pipelineLayout, 0,
          *renderItem.descriptorBindings->descriptorSet(frameIndex), nullptr);
      commandBuffer.drawIndexed(renderItem.mesh->getIndices().size(), 1, 0, 0, 0);
    }

    commandBuffer.endRendering();

    transitionImageLayout(
        commandBuffer, swapchainContext.swapchainImages()[imageIndex],
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageLayout::ePresentSrcKHR,
        vk::AccessFlagBits2::eColorAttachmentWrite, {},
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::PipelineStageFlagBits2::eBottomOfPipe,
        vk::ImageAspectFlagBits::eColor);

    commandBuffer.end();
  }

  vk::raii::DescriptorSetLayout &descriptorSetLayout() override {
    return descriptorSetLayoutHandle;
  }

private:
  PipelineSpec spec;
  ShaderProgram shaderProgram;

  vk::raii::DescriptorSetLayout descriptorSetLayoutHandle = nullptr;
  vk::raii::PipelineLayout pipelineLayout = nullptr;
  vk::raii::Pipeline graphicsPipeline = nullptr;

  vk::raii::Image colorImage = nullptr;
  vk::raii::DeviceMemory colorImageMemory = nullptr;
  vk::raii::ImageView colorImageView = nullptr;

  vk::raii::Image depthImage = nullptr;
  vk::raii::DeviceMemory depthImageMemory = nullptr;
  vk::raii::ImageView depthImageView = nullptr;

  void createDescriptorSetLayout(DeviceContext &deviceContext) {
    std::array bindings = {
        vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1,
                                      vk::ShaderStageFlagBits::eVertex, nullptr),
        vk::DescriptorSetLayoutBinding(
            1, vk::DescriptorType::eCombinedImageSampler, 1,
            vk::ShaderStageFlagBits::eFragment, nullptr)};

    vk::DescriptorSetLayoutCreateInfo layoutInfo{
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()};
    descriptorSetLayoutHandle =
        vk::raii::DescriptorSetLayout(deviceContext.deviceHandle(), layoutInfo);
  }

  void createGraphicsPipeline(DeviceContext &deviceContext,
                              SwapchainContext &swapchainContext) {
    shaderProgram.load(deviceContext, spec.shaderPath, spec.vertexEntry,
                       spec.fragmentEntry);
    auto shaderStages = shaderProgram.stages();

    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &bindingDescription,
        .vertexAttributeDescriptionCount =
            static_cast<uint32_t>(attributeDescriptions.size()),
        .pVertexAttributeDescriptions = attributeDescriptions.data()};
    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
        .topology = spec.topology,
        .primitiveRestartEnable = vk::False};
    vk::PipelineViewportStateCreateInfo viewportState{.viewportCount = 1,
                                                      .scissorCount = 1};
    vk::PipelineRasterizationStateCreateInfo rasterizer{
        .depthClampEnable = vk::False,
        .rasterizerDiscardEnable = vk::False,
        .polygonMode = spec.polygonMode,
        .cullMode = spec.cullMode,
        .frontFace = spec.frontFace,
        .depthBiasEnable = vk::False,
        .lineWidth = 1.0f};
    vk::PipelineMultisampleStateCreateInfo multisampling{
        .rasterizationSamples = deviceContext.msaaSampleCount(),
        .sampleShadingEnable = vk::False};
    vk::PipelineDepthStencilStateCreateInfo depthStencil{
        .depthTestEnable = spec.enableDepthTest,
        .depthWriteEnable = spec.enableDepthWrite,
        .depthCompareOp = vk::CompareOp::eLess,
        .depthBoundsTestEnable = vk::False,
        .stencilTestEnable = vk::False};
    vk::PipelineColorBlendAttachmentState colorBlendAttachment{
        .blendEnable = spec.enableBlending,
        .srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
        .dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
        .colorBlendOp = vk::BlendOp::eAdd,
        .srcAlphaBlendFactor = vk::BlendFactor::eOne,
        .dstAlphaBlendFactor = vk::BlendFactor::eZero,
        .alphaBlendOp = vk::BlendOp::eAdd,
        .colorWriteMask =
            vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};
    vk::PipelineColorBlendStateCreateInfo colorBlending{
        .logicOpEnable = vk::False,
        .logicOp = vk::LogicOp::eCopy,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment};
    std::vector dynamicStates = {vk::DynamicState::eViewport,
                                 vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dynamicState{
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data()};

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
        .setLayoutCount = 1,
        .pSetLayouts = &*descriptorSetLayoutHandle,
        .pushConstantRangeCount = 0};

    pipelineLayout =
        vk::raii::PipelineLayout(deviceContext.deviceHandle(),
                                 pipelineLayoutInfo);

    vk::Format depthFormat = deviceContext.findDepthFormat();

    vk::StructureChain<vk::GraphicsPipelineCreateInfo,
                       vk::PipelineRenderingCreateInfo>
        pipelineCreateInfoChain = {
            {.stageCount = static_cast<uint32_t>(shaderStages.size()),
             .pStages = shaderStages.data(),
             .pVertexInputState = &vertexInputInfo,
             .pInputAssemblyState = &inputAssembly,
             .pViewportState = &viewportState,
             .pRasterizationState = &rasterizer,
             .pMultisampleState = &multisampling,
             .pDepthStencilState = &depthStencil,
             .pColorBlendState = &colorBlending,
             .pDynamicState = &dynamicState,
             .layout = pipelineLayout,
             .renderPass = nullptr},
            {.colorAttachmentCount = 1,
             .pColorAttachmentFormats =
                 &swapchainContext.surfaceFormatInfo().format,
             .depthAttachmentFormat = depthFormat}};

    graphicsPipeline =
        vk::raii::Pipeline(deviceContext.deviceHandle(), nullptr,
                           pipelineCreateInfoChain.get<
                               vk::GraphicsPipelineCreateInfo>());
  }

  void createColorResources(DeviceContext &deviceContext,
                            SwapchainContext &swapchainContext) {
    vk::Format colorFormat = swapchainContext.surfaceFormatInfo().format;

    RenderUtils::createImage(
        deviceContext, swapchainContext.extent2D().width,
        swapchainContext.extent2D().height, 1, deviceContext.msaaSampleCount(),
        colorFormat, vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eTransientAttachment |
            vk::ImageUsageFlagBits::eColorAttachment,
        vk::MemoryPropertyFlagBits::eDeviceLocal, colorImage, colorImageMemory);
    colorImageView = createImageView(deviceContext, colorImage, colorFormat,
                                     vk::ImageAspectFlagBits::eColor, 1);
  }

  void createDepthResources(DeviceContext &deviceContext,
                            SwapchainContext &swapchainContext) {
    vk::Format depthFormat = deviceContext.findDepthFormat();

    RenderUtils::createImage(
        deviceContext, swapchainContext.extent2D().width,
        swapchainContext.extent2D().height, 1, deviceContext.msaaSampleCount(),
        depthFormat, vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eDepthStencilAttachment,
        vk::MemoryPropertyFlagBits::eDeviceLocal, depthImage, depthImageMemory);
    depthImageView = createImageView(deviceContext, depthImage, depthFormat,
                                     vk::ImageAspectFlagBits::eDepth, 1);
  }

  vk::raii::ImageView createImageView(DeviceContext &deviceContext,
                                      const vk::raii::Image &image,
                                      vk::Format format,
                                      vk::ImageAspectFlags aspectFlags,
                                      uint32_t mipLevels) {
    vk::ImageViewCreateInfo viewInfo{
        .image = image,
        .viewType = vk::ImageViewType::e2D,
        .format = format,
        .subresourceRange = {aspectFlags, 0, mipLevels, 0, 1}};
    return vk::raii::ImageView(deviceContext.deviceHandle(), viewInfo);
  }

  void transitionImageLayout(vk::raii::CommandBuffer &commandBuffer,
                             vk::Image image, vk::ImageLayout oldLayout,
                             vk::ImageLayout newLayout,
                             vk::AccessFlags2 srcAccessMask,
                             vk::AccessFlags2 dstAccessMask,
                             vk::PipelineStageFlags2 srcStageMask,
                             vk::PipelineStageFlags2 dstStageMask,
                             vk::ImageAspectFlags imageAspectFlags) {
    vk::ImageMemoryBarrier2 barrier = {
        .srcStageMask = srcStageMask,
        .srcAccessMask = srcAccessMask,
        .dstStageMask = dstStageMask,
        .dstAccessMask = dstAccessMask,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {.aspectMask = imageAspectFlags,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1}};
    vk::DependencyInfo dependencyInfo = {.dependencyFlags = {},
                                         .imageMemoryBarrierCount = 1,
                                         .pImageMemoryBarriers = &barrier};
    commandBuffer.pipelineBarrier2(dependencyInfo);
  }
};
