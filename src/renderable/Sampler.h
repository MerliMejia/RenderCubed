#pragma once

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#include "../backend/DeviceContext.h"

class Sampler {
public:
  void create(DeviceContext &deviceContext) {
    vk::PhysicalDeviceProperties properties =
        deviceContext.physicalDeviceHandle().getProperties();
    vk::SamplerCreateInfo samplerInfo{
        .magFilter = vk::Filter::eLinear,
        .minFilter = vk::Filter::eLinear,
        .mipmapMode = vk::SamplerMipmapMode::eLinear,
        .addressModeU = vk::SamplerAddressMode::eRepeat,
        .addressModeV = vk::SamplerAddressMode::eRepeat,
        .addressModeW = vk::SamplerAddressMode::eRepeat,
        .mipLodBias = 0.0f,
        .anisotropyEnable = vk::True,
        .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
        .compareEnable = vk::False,
        .compareOp = vk::CompareOp::eAlways};
    sampler = vk::raii::Sampler(deviceContext.deviceHandle(), samplerInfo);
  }

  vk::raii::Sampler &handle() { return sampler; }
  const vk::raii::Sampler &handle() const { return sampler; }

private:
  vk::raii::Sampler sampler = nullptr;
};
