#pragma once

#include "MeshRenderPass.h"

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

class OpaqueMeshPass : public MeshRenderPass {
public:
  explicit OpaqueMeshPass(PipelineSpec pipelineSpec)
      : MeshRenderPass(std::move(pipelineSpec)) {}

private:
  std::vector<DescriptorBindingSpec> descriptorBindings() const override {
    return {
        DescriptorBindingSpec{.binding = 0,
                              .descriptorType = vk::DescriptorType::eUniformBuffer,
                              .descriptorCount = 1,
                              .stageFlags = vk::ShaderStageFlagBits::eVertex},
        DescriptorBindingSpec{
            .binding = 1,
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eFragment}};
  }
};
