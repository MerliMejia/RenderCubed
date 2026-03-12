#pragma once

#include "../renderer/RenderPass.h"
#include "ModelMaterialSet.h"
#include "ObjModelAsset.h"
#include <vector>

class RenderableModel {
public:
  void loadFromObj(const std::string &path, CommandContext &commandContext,
                   DeviceContext &deviceContext,
                   const vk::raii::DescriptorSetLayout &descriptorSetLayout,
                   FrameUniforms &frameUniforms, Sampler &sampler,
                   uint32_t framesInFlight) {
    asset.load(path);
    asset.createGpuBuffers(commandContext, deviceContext);
    materials.create(deviceContext, commandContext, descriptorSetLayout,
                     frameUniforms, sampler, asset.materials(), framesInFlight);
  }

  std::vector<RenderItem> buildRenderItems(const RenderPass *targetPass) {
    std::vector<RenderItem> items;
    const auto &submeshes = asset.submeshes();
    items.reserve(submeshes.empty() ? 1 : submeshes.size());

    if (submeshes.empty()) {
      items.push_back(RenderItem{
          .mesh = &asset.mesh(),
          .descriptorBindings = &materials.bindingsForMaterialIndex(-1),
          .targetPass = targetPass,
      });
      return items;
    }

    for (const auto &submesh : submeshes) {
      items.push_back(RenderItem{
          .mesh = &asset.mesh(),
          .descriptorBindings =
              &materials.bindingsForMaterialIndex(submesh.materialIndex),
          .targetPass = targetPass,
          .indexOffset = submesh.indexOffset,
          .indexCount = submesh.indexCount,
      });
    }

    return items;
  }

  ObjModelAsset &modelAsset() { return asset; }
  const ObjModelAsset &modelAsset() const { return asset; }

private:
  ObjModelAsset asset;
  ModelMaterialSet materials;
};
