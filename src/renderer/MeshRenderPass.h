#pragma once

#include "PipelineSpec.h"
#include "RenderPass.h"
#include "ShaderProgram.h"
#include "../renderable/RenderUtils.h"
#include <array>
#include <optional>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

struct DescriptorBindingSpec {
  uint32_t binding = 0;
  vk::DescriptorType descriptorType = vk::DescriptorType::eUniformBuffer;
  uint32_t descriptorCount = 1;
  vk::ShaderStageFlags stageFlags = {};
};

struct MeshPassAttachmentConfig {
  bool useColorAttachment = true;
  bool useDepthAttachment = true;
  bool useMsaaColorAttachment = true;
  bool resolveToSwapchain = true;
  std::array<float, 4> clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
  float clearDepth = 1.0f;
  uint32_t clearStencil = 0;
  vk::AttachmentLoadOp colorLoadOp = vk::AttachmentLoadOp::eClear;
  vk::AttachmentStoreOp colorStoreOp = vk::AttachmentStoreOp::eStore;
  vk::AttachmentLoadOp depthLoadOp = vk::AttachmentLoadOp::eClear;
  vk::AttachmentStoreOp depthStoreOp = vk::AttachmentStoreOp::eDontCare;
};

struct VertexInputLayoutSpec {
  std::vector<vk::VertexInputBindingDescription> bindings;
  std::vector<vk::VertexInputAttributeDescription> attributes;
};

class MeshRenderPass : public RenderPass {
public:
  explicit MeshRenderPass(
      PipelineSpec pipelineSpec,
      MeshPassAttachmentConfig attachmentConfig = MeshPassAttachmentConfig())
      : spec(std::move(pipelineSpec)),
        attachments(std::move(attachmentConfig)) {}

  void initialize(DeviceContext &deviceContext,
                  SwapchainContext &swapchainContext) override {
    createDescriptorSetLayout(deviceContext);
    createPipelineLayout(deviceContext);
    createGraphicsPipeline(deviceContext, swapchainContext);
    createAttachmentResources(deviceContext, swapchainContext);
    initializePassResources(deviceContext, swapchainContext);
  }

  void recreate(DeviceContext &deviceContext,
                SwapchainContext &swapchainContext) override {
    createGraphicsPipeline(deviceContext, swapchainContext);
    createAttachmentResources(deviceContext, swapchainContext);
    recreatePassResources(deviceContext, swapchainContext);
  }

  void record(const RenderPassContext &context,
              const std::vector<RenderItem> &renderItems) override {
    transitionToRenderingLayouts(context);

    auto colorAttachment = buildColorAttachment(context);
    auto depthAttachment = buildDepthAttachment();

    vk::RenderingInfo renderingInfo = {
        .renderArea = {.offset = {0, 0},
                       .extent = context.swapchainContext.extent2D()},
        .layerCount = 1,
        .colorAttachmentCount = colorAttachment ? 1u : 0u,
        .pColorAttachments = colorAttachment ? &*colorAttachment : nullptr,
        .pDepthAttachment = depthAttachment ? &*depthAttachment : nullptr};

    context.commandBuffer.beginRendering(renderingInfo);
    context.commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                       *graphicsPipeline);
    context.commandBuffer.setViewport(
        0, vk::Viewport(0.0f, 0.0f,
                        static_cast<float>(context.swapchainContext.extent2D().width),
                        static_cast<float>(context.swapchainContext.extent2D().height),
                        0.0f, 1.0f));
    context.commandBuffer.setScissor(
        0, vk::Rect2D(vk::Offset2D(0, 0), context.swapchainContext.extent2D()));

    bindPassResources(context);

    for (const auto &renderItem : renderItems) {
      if (!shouldDrawRenderItem(renderItem) || renderItem.mesh == nullptr) {
        continue;
      }

      bindRenderItemResources(context, renderItem);
      drawRenderItem(context, renderItem);
    }

    context.commandBuffer.endRendering();
    transitionToFinalLayouts(context);
  }

  vk::raii::DescriptorSetLayout *descriptorSetLayout() override {
    return hasDescriptorSetLayout() ? &descriptorSetLayoutHandle : nullptr;
  }

protected:
  virtual std::vector<DescriptorBindingSpec> descriptorBindings() const {
    return {};
  }

  virtual std::vector<vk::PushConstantRange> pushConstantRanges() const {
    return {};
  }

  virtual VertexInputLayoutSpec vertexInputLayout() const {
    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    return VertexInputLayoutSpec{
        .bindings = {bindingDescription},
        .attributes = {attributeDescriptions.begin(),
                       attributeDescriptions.end()}};
  }

  virtual void initializePassResources(DeviceContext &deviceContext,
                                       SwapchainContext &swapchainContext) {}

  virtual void recreatePassResources(DeviceContext &deviceContext,
                                     SwapchainContext &swapchainContext) {}

  virtual void bindPassResources(const RenderPassContext &context) {}

  virtual bool shouldDrawRenderItem(const RenderItem &renderItem) const {
    return true;
  }

  virtual void bindRenderItemResources(const RenderPassContext &context,
                                       const RenderItem &renderItem) {
    if (hasDescriptorSetLayout() &&
        renderItem.descriptorBindings != nullptr) {
      context.commandBuffer.bindDescriptorSets(
          vk::PipelineBindPoint::eGraphics, pipelineLayout, 0,
          *renderItem.descriptorBindings->descriptorSet(context.frameIndex),
          nullptr);
    }
  }

  virtual void drawRenderItem(const RenderPassContext &context,
                              const RenderItem &renderItem) {
    context.commandBuffer.bindVertexBuffers(0, *renderItem.mesh->getVertexBuffer(),
                                            {0});
    context.commandBuffer.bindIndexBuffer(*renderItem.mesh->getIndexBuffer(), 0,
                                          vk::IndexType::eUint32);
    context.commandBuffer.drawIndexed(renderItem.mesh->getIndices().size(), 1, 0, 0,
                                      0);
  }

  const PipelineSpec &pipelineSpec() const { return spec; }
  const MeshPassAttachmentConfig &attachmentConfig() const { return attachments; }

  vk::raii::PipelineLayout &pipelineLayoutHandle() { return pipelineLayout; }
  const vk::raii::PipelineLayout &pipelineLayoutHandle() const {
    return pipelineLayout;
  }

  vk::raii::DescriptorSetLayout &passDescriptorSetLayout() {
    return descriptorSetLayoutHandle;
  }

  const vk::raii::DescriptorSetLayout &passDescriptorSetLayout() const {
    return descriptorSetLayoutHandle;
  }

private:
  PipelineSpec spec;
  MeshPassAttachmentConfig attachments;
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
    auto bindingSpecs = descriptorBindings();
    if (bindingSpecs.empty()) {
      descriptorSetLayoutHandle = nullptr;
      return;
    }

    std::vector<vk::DescriptorSetLayoutBinding> bindings;
    bindings.reserve(bindingSpecs.size());
    for (const auto &bindingSpec : bindingSpecs) {
      bindings.emplace_back(bindingSpec.binding, bindingSpec.descriptorType,
                            bindingSpec.descriptorCount,
                            bindingSpec.stageFlags, nullptr);
    }

    vk::DescriptorSetLayoutCreateInfo layoutInfo{
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()};
    descriptorSetLayoutHandle =
        vk::raii::DescriptorSetLayout(deviceContext.deviceHandle(), layoutInfo);
  }

  void createPipelineLayout(DeviceContext &deviceContext) {
    auto pushConstants = pushConstantRanges();

    const vk::DescriptorSetLayout *setLayout =
        hasDescriptorSetLayout() ? &*descriptorSetLayoutHandle : nullptr;
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
        .setLayoutCount = setLayout != nullptr ? 1u : 0u,
        .pSetLayouts = setLayout,
        .pushConstantRangeCount = static_cast<uint32_t>(pushConstants.size()),
        .pPushConstantRanges = pushConstants.data()};

    pipelineLayout = vk::raii::PipelineLayout(deviceContext.deviceHandle(),
                                              pipelineLayoutInfo);
  }

  void createGraphicsPipeline(DeviceContext &deviceContext,
                              SwapchainContext &swapchainContext) {
    shaderProgram.load(deviceContext, spec.shaderPath, spec.vertexEntry,
                       spec.fragmentEntry);
    auto shaderStages = shaderProgram.stages();

    auto vertexLayout = vertexInputLayout();
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
        .vertexBindingDescriptionCount =
            static_cast<uint32_t>(vertexLayout.bindings.size()),
        .pVertexBindingDescriptions = vertexLayout.bindings.data(),
        .vertexAttributeDescriptionCount =
            static_cast<uint32_t>(vertexLayout.attributes.size()),
        .pVertexAttributeDescriptions = vertexLayout.attributes.data()};
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
        .rasterizationSamples = rasterizationSampleCount(deviceContext),
        .sampleShadingEnable = vk::False};
    vk::PipelineDepthStencilStateCreateInfo depthStencil{
        .depthTestEnable = spec.enableDepthTest && attachments.useDepthAttachment,
        .depthWriteEnable = spec.enableDepthWrite && attachments.useDepthAttachment,
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
        .attachmentCount = attachments.useColorAttachment ? 1u : 0u,
        .pAttachments =
            attachments.useColorAttachment ? &colorBlendAttachment : nullptr};
    std::vector dynamicStates = {vk::DynamicState::eViewport,
                                 vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dynamicState{
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data()};

    vk::Format colorFormat = swapchainContext.surfaceFormatInfo().format;
    vk::Format depthFormat = attachments.useDepthAttachment
                                 ? deviceContext.findDepthFormat()
                                 : vk::Format::eUndefined;
    uint32_t colorAttachmentCount = attachments.useColorAttachment ? 1u : 0u;
    const vk::Format *colorAttachmentFormats =
        attachments.useColorAttachment ? &colorFormat : nullptr;

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
            {.colorAttachmentCount = colorAttachmentCount,
             .pColorAttachmentFormats = colorAttachmentFormats,
             .depthAttachmentFormat = depthFormat}};

    graphicsPipeline =
        vk::raii::Pipeline(deviceContext.deviceHandle(), nullptr,
                           pipelineCreateInfoChain.get<
                               vk::GraphicsPipelineCreateInfo>());
  }

  void createAttachmentResources(DeviceContext &deviceContext,
                                 SwapchainContext &swapchainContext) {
    if (attachments.useColorAttachment && attachments.useMsaaColorAttachment) {
      createColorResources(deviceContext, swapchainContext);
    } else {
      colorImage = nullptr;
      colorImageMemory = nullptr;
      colorImageView = nullptr;
    }

    if (attachments.useDepthAttachment) {
      createDepthResources(deviceContext, swapchainContext);
    } else {
      depthImage = nullptr;
      depthImageMemory = nullptr;
      depthImageView = nullptr;
    }
  }

  void createColorResources(DeviceContext &deviceContext,
                            SwapchainContext &swapchainContext) {
    vk::Format colorFormat = swapchainContext.surfaceFormatInfo().format;

    RenderUtils::createImage(
        deviceContext, swapchainContext.extent2D().width,
        swapchainContext.extent2D().height, 1, rasterizationSampleCount(deviceContext),
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
        swapchainContext.extent2D().height, 1, rasterizationSampleCount(deviceContext),
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

  void transitionToRenderingLayouts(const RenderPassContext &context) {
    if (writesToSwapchain()) {
      transitionImageLayout(
          context.commandBuffer,
          context.swapchainContext.swapchainImages()[context.imageIndex],
          vk::ImageLayout::eUndefined,
          vk::ImageLayout::eColorAttachmentOptimal, {},
          vk::AccessFlagBits2::eColorAttachmentWrite,
          vk::PipelineStageFlagBits2::eColorAttachmentOutput,
          vk::PipelineStageFlagBits2::eColorAttachmentOutput,
          vk::ImageAspectFlagBits::eColor);
    }

    if (attachments.useColorAttachment && attachments.useMsaaColorAttachment) {
      transitionImageLayout(
          context.commandBuffer, *colorImage, vk::ImageLayout::eUndefined,
          vk::ImageLayout::eColorAttachmentOptimal,
          vk::AccessFlagBits2::eColorAttachmentWrite,
          vk::AccessFlagBits2::eColorAttachmentWrite,
          vk::PipelineStageFlagBits2::eColorAttachmentOutput,
          vk::PipelineStageFlagBits2::eColorAttachmentOutput,
          vk::ImageAspectFlagBits::eColor);
    }

    if (attachments.useDepthAttachment) {
      transitionImageLayout(
          context.commandBuffer, *depthImage, vk::ImageLayout::eUndefined,
          vk::ImageLayout::eDepthAttachmentOptimal,
          vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
          vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
          vk::PipelineStageFlagBits2::eEarlyFragmentTests |
              vk::PipelineStageFlagBits2::eLateFragmentTests,
          vk::PipelineStageFlagBits2::eEarlyFragmentTests |
              vk::PipelineStageFlagBits2::eLateFragmentTests,
          vk::ImageAspectFlagBits::eDepth);
    }
  }

  void transitionToFinalLayouts(const RenderPassContext &context) {
    if (!writesToSwapchain()) {
      return;
    }

    transitionImageLayout(
        context.commandBuffer,
        context.swapchainContext.swapchainImages()[context.imageIndex],
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageLayout::ePresentSrcKHR,
        vk::AccessFlagBits2::eColorAttachmentWrite, {},
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::PipelineStageFlagBits2::eBottomOfPipe,
        vk::ImageAspectFlagBits::eColor);
  }

  std::optional<vk::RenderingAttachmentInfo>
  buildColorAttachment(const RenderPassContext &context) const {
    if (!attachments.useColorAttachment) {
      return std::nullopt;
    }

    vk::ClearValue clearColor = {
        .color = {.float32 = attachments.clearColor}};

    if (attachments.useMsaaColorAttachment) {
      return vk::RenderingAttachmentInfo{
          .imageView = colorImageView,
          .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
          .resolveMode = attachments.resolveToSwapchain
                             ? vk::ResolveModeFlagBits::eAverage
                             : vk::ResolveModeFlagBits::eNone,
          .resolveImageView =
              attachments.resolveToSwapchain
                  ? context.swapchainContext.swapchainImageViews()[context.imageIndex]
                  : vk::ImageView(),
          .resolveImageLayout =
              attachments.resolveToSwapchain
                  ? vk::ImageLayout::eColorAttachmentOptimal
                  : vk::ImageLayout::eUndefined,
          .loadOp = attachments.colorLoadOp,
          .storeOp = attachments.colorStoreOp,
          .clearValue = clearColor};
    }

    return vk::RenderingAttachmentInfo{
        .imageView = context.swapchainContext.swapchainImageViews()[context.imageIndex],
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp = attachments.colorLoadOp,
        .storeOp = attachments.colorStoreOp,
        .clearValue = clearColor};
  }

  std::optional<vk::RenderingAttachmentInfo> buildDepthAttachment() const {
    if (!attachments.useDepthAttachment) {
      return std::nullopt;
    }

    vk::ClearValue clearDepth = {
        .depthStencil = vk::ClearDepthStencilValue{attachments.clearDepth,
                                                   attachments.clearStencil}};
    return vk::RenderingAttachmentInfo{
        .imageView = depthImageView,
        .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
        .loadOp = attachments.depthLoadOp,
        .storeOp = attachments.depthStoreOp,
        .clearValue = clearDepth};
  }

  vk::SampleCountFlagBits
  rasterizationSampleCount(DeviceContext &deviceContext) const {
    return attachments.useMsaaColorAttachment ? deviceContext.msaaSampleCount()
                                              : vk::SampleCountFlagBits::e1;
  }

  bool writesToSwapchain() const {
    return (attachments.useColorAttachment && !attachments.useMsaaColorAttachment) ||
           attachments.resolveToSwapchain;
  }

  bool hasDescriptorSetLayout() const {
    return static_cast<vk::DescriptorSetLayout>(descriptorSetLayoutHandle) !=
           VK_NULL_HANDLE;
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
