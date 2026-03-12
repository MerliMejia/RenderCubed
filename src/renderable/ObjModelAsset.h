#pragma once

#include "Mesh.h"

class ObjModelAsset {
public:
  void load(const std::string &path) {
    sourcePath = path;
    geometryMesh.loadModel(path);
  }

  void createGpuBuffers(CommandContext &commandContext,
                        DeviceContext &deviceContext) {
    geometryMesh.createVertexBuffer(commandContext, deviceContext);
    geometryMesh.createIndexBuffer(commandContext, deviceContext);
  }

  ObjGeometryMesh &mesh() { return geometryMesh; }
  const ObjGeometryMesh &mesh() const { return geometryMesh; }

  const std::vector<ObjMaterialData> &materials() const {
    return geometryMesh.getMaterials();
  }

  const std::vector<ObjSubmesh> &submeshes() const {
    return geometryMesh.getSubmeshes();
  }

  const std::string &path() const { return sourcePath; }

private:
  std::string sourcePath;
  ObjGeometryMesh geometryMesh;
};
