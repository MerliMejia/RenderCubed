#pragma once
#include "UniformMeshRenderPass.h"

struct TestData {
  glm::vec4 color1 = {1.0f, 0.0f, 0.0f, 1.0f};
  glm::vec4 color2 = {0.0f, 1.0f, 0.0f, 1.0f};
  glm::vec4 color3 = {0.0f, 0.0f, 1.0f, 1.0f};
};

struct TestPushConstant {
  uint32_t flag = 0;
};

class TestPass : public UniformMeshRenderPass<TestData, TestPushConstant> {
public:
  TestPass(PipelineSpec pipelineSpec, uint32_t framesInFlight)
      : UniformMeshRenderPass<TestData, TestPushConstant>(
            std::move(pipelineSpec), framesInFlight) {}

  void setColor1(glm::vec4 color) { data.color1 = color; }
  void setColor2(glm::vec4 color) { data.color2 = color; }
  void setColor3(glm::vec4 color) { data.color3 = color; }
  void setFlag(uint32_t flag) { pushData.flag = flag; }

protected:
  TestData buildUniformData(uint32_t frameIndex) const override { return data; }
  TestPushConstant buildPushConstantData(uint32_t frameIndex) const override {
    return pushData;
  }

  VertexInputLayoutSpec vertexInputLayout() const override {
    return VertexInputLayoutSpec{
        .bindings = {Vertex::getBindingDescription()},
        .attributes = {vk::VertexInputAttributeDescription(
            0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos))}};
  }

private:
  TestData data;
  TestPushConstant pushData;
};
