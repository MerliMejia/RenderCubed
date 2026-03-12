#pragma once

#include "DescriptorBindings.h"
#include "FrameUniforms.h"
#include "Mesh.h"
#include "Sampler.h"
#include "Texture.h"
#include <array>
#include <filesystem>
#include <vector>

class ModelMaterialSet {
public:
  void create(DeviceContext &deviceContext, CommandContext &commandContext,
              const vk::raii::DescriptorSetLayout &descriptorSetLayout,
              FrameUniforms &frameUniforms, Sampler &sampler,
              const std::vector<ObjMaterialData> &materials,
              uint32_t framesInFlight) {
    defaultMaterialResource = MaterialResource{};
    materialResources.clear();
    materialResources.reserve(materials.size());

    initializeResource(defaultMaterialResource, nullptr, deviceContext,
                       commandContext, descriptorSetLayout, frameUniforms,
                       sampler, framesInFlight);

    for (const auto &material : materials) {
      materialResources.emplace_back();
      initializeResource(materialResources.back(), &material, deviceContext,
                         commandContext, descriptorSetLayout, frameUniforms,
                         sampler, framesInFlight);
    }
  }

  DescriptorBindings &bindingsForMaterialIndex(int materialIndex) {
    if (materialIndex < 0 ||
        static_cast<size_t>(materialIndex) >= materialResources.size()) {
      return defaultMaterialResource.bindings;
    }

    return materialResources[materialIndex].bindings;
  }

private:
  struct MaterialResource {
    Texture texture;
    DescriptorBindings bindings;
  };

  static std::array<uint8_t, 4> toRgba8(const glm::vec4 &color) {
    auto toByte = [](float channel) {
      return static_cast<uint8_t>(glm::clamp(channel, 0.0f, 1.0f) * 255.0f +
                                  0.5f);
    };

    return {toByte(color.r), toByte(color.g), toByte(color.b), toByte(color.a)};
  }

  static bool hasAvailableDiffuseTexture(const ObjMaterialData &material) {
    return material.hasDiffuseTexture() &&
           std::filesystem::exists(material.resolvedDiffuseTexturePath);
  }

  void initializeResource(
      MaterialResource &resource, const ObjMaterialData *material,
      DeviceContext &deviceContext, CommandContext &commandContext,
      const vk::raii::DescriptorSetLayout &descriptorSetLayout,
      FrameUniforms &frameUniforms, Sampler &sampler, uint32_t framesInFlight) {
    const glm::vec4 fallbackColor =
        material == nullptr ? glm::vec4(1.0f) : material->diffuseRgba();

    if (material != nullptr && hasAvailableDiffuseTexture(*material)) {
      resource.texture.create(material->resolvedDiffuseTexturePath,
                              commandContext, deviceContext);
    } else {
      resource.texture.createSolidColor(toRgba8(fallbackColor), commandContext,
                                        deviceContext);
    }

    resource.bindings.create(deviceContext, descriptorSetLayout, frameUniforms,
                             resource.texture, sampler, framesInFlight);
  }

  MaterialResource defaultMaterialResource;
  std::vector<MaterialResource> materialResources;
};
