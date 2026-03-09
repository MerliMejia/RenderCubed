#pragma once

#include "../backend/DeviceContext.h"
#include "../backend/SwapchainContext.h"
#include "../renderable/DescriptorBindings.h"
#include "../renderable/Mesh.h"
#include <vector>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

struct RenderItem {
  Mesh *mesh = nullptr;
  DescriptorBindings *descriptorBindings = nullptr;
};

class RenderPass {
public:
  virtual ~RenderPass() = default;

  virtual void initialize(DeviceContext &deviceContext,
                          SwapchainContext &swapchainContext) = 0;
  virtual void recreate(DeviceContext &deviceContext,
                        SwapchainContext &swapchainContext) = 0;
  virtual void record(vk::raii::CommandBuffer &commandBuffer,
                      SwapchainContext &swapchainContext,
                      const std::vector<RenderItem> &renderItems,
                      uint32_t frameIndex, uint32_t imageIndex) = 0;
  virtual vk::raii::DescriptorSetLayout &descriptorSetLayout() = 0;
};
