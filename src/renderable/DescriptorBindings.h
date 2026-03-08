#pragma once

#include "FrameUniforms.h"
#include "Sampler.h"
#include "Texture.h"
#include <array>
#include <vector>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

class DescriptorBindings {
public:
  void create(DeviceContext &deviceContext,
              const vk::raii::DescriptorSetLayout &descriptorSetLayout,
              FrameUniforms &frameUniforms, Texture &texture, Sampler &sampler,
              uint32_t framesInFlight) {
    std::array poolSize{
        vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer,
                               framesInFlight),
        vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler,
                               framesInFlight)};
    vk::DescriptorPoolCreateInfo poolInfo{
        .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        .maxSets = framesInFlight,
        .poolSizeCount = static_cast<uint32_t>(poolSize.size()),
        .pPoolSizes = poolSize.data()};
    descriptorPool =
        vk::raii::DescriptorPool(deviceContext.deviceHandle(), poolInfo);

    std::vector<vk::DescriptorSetLayout> layouts(framesInFlight,
                                                 descriptorSetLayout);
    vk::DescriptorSetAllocateInfo allocInfo{
        .descriptorPool = descriptorPool,
        .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
        .pSetLayouts = layouts.data()};

    descriptorSets.clear();
    descriptorSets =
        deviceContext.deviceHandle().allocateDescriptorSets(allocInfo);

    for (uint32_t i = 0; i < framesInFlight; i++) {
      vk::DescriptorBufferInfo bufferInfo{.buffer = frameUniforms.buffer(i),
                                          .offset = 0,
                                          .range = sizeof(UniformBufferObject)};
      vk::DescriptorImageInfo imageInfo{
          .sampler = sampler.handle(),
          .imageView = texture.imageView(),
          .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal};
      std::array descriptorWrites{
          vk::WriteDescriptorSet{.dstSet = descriptorSets[i],
                                 .dstBinding = 0,
                                 .dstArrayElement = 0,
                                 .descriptorCount = 1,
                                 .descriptorType =
                                     vk::DescriptorType::eUniformBuffer,
                                 .pBufferInfo = &bufferInfo},
          vk::WriteDescriptorSet{.dstSet = descriptorSets[i],
                                 .dstBinding = 1,
                                 .dstArrayElement = 0,
                                 .descriptorCount = 1,
                                 .descriptorType =
                                     vk::DescriptorType::eCombinedImageSampler,
                                 .pImageInfo = &imageInfo}};
      deviceContext.deviceHandle().updateDescriptorSets(descriptorWrites, {});
    }
  }

  vk::raii::DescriptorSet &descriptorSet(uint32_t frameIndex) {
    return descriptorSets[frameIndex];
  }

private:
  vk::raii::DescriptorPool descriptorPool = nullptr;
  std::vector<vk::raii::DescriptorSet> descriptorSets;
};
