#include "backend/AppWindow.h"
#include "backend/VulkanBackend.h"
#include "renderable/Mesh.h"
#include "renderer/ForwardRenderer.h"
#include "renderer/PipelineSpec.h"
#include "renderer/TestPass.h"
#include <memory>

#include <cstdlib>
#include <iostream>
#include <vector>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;
constexpr int MAX_FRAMES_IN_FLIGHT = 2;
const std::string ASSET_PATH = "assets";
const std::string SHADER_PATH = ASSET_PATH + "/shaders/test_pass.spv";

class RenderCubedApplication {
public:
  void run() {
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
  }

private:
  AppWindow appWindow;
  VulkanBackend backend;
  BackendConfig backendConfig{.appName = "RenderCubed",
                              .maxFramesInFlight = MAX_FRAMES_IN_FLIGHT};

  DeviceContext &deviceContext() { return backend.device(); }
  SwapchainContext &swapchainContext() { return backend.swapchain(); }
  CommandContext &commandContext() { return backend.commands(); }

  Mesh mesh;
  ForwardRenderer forwardRenderer;
  TestPass *testPass = nullptr;

  void initWindow() { appWindow.create(WIDTH, HEIGHT, "RenderCubed"); }

  void initVulkan() {
    backend.initialize(appWindow, backendConfig);

    mesh.setGeometry(
        {
            Vertex{.pos = {0.0f, -0.5f, 0.0f},
                   .color = {1.0f, 1.0f, 1.0f},
                   .texCoord = {0.5f, 1.0f}},
            Vertex{.pos = {0.5f, 0.5f, 0.0f},
                   .color = {1.0f, 1.0f, 1.0f},
                   .texCoord = {1.0f, 0.0f}},
            Vertex{.pos = {-0.5f, 0.5f, 0.0f},
                   .color = {1.0f, 1.0f, 1.0f},
                   .texCoord = {0.0f, 0.0f}},
        },
        {0, 1, 2});
    mesh.createVertexBuffer(commandContext(), deviceContext());
    mesh.createIndexBuffer(commandContext(), deviceContext());

    auto pass = std::make_unique<TestPass>(
        PipelineSpec{.shaderPath = SHADER_PATH,
                     .cullMode = vk::CullModeFlagBits::eNone},
        MAX_FRAMES_IN_FLIGHT);

    pass->setColor1({1.0f, 0.0f, 0.0f, 1.0f});
    pass->setColor2({0.0f, 1.0f, 0.0f, 1.0f});
    pass->setColor3({0.0f, 0.0f, 1.0f, 1.0f});

    testPass = pass.get();
    forwardRenderer.addPass(std::move(pass));
    forwardRenderer.initialize(deviceContext(), swapchainContext());
  }

  void mainLoop() {
    while (!appWindow.shouldClose()) {
      appWindow.pollEvents();
      drawFrame();
    }

    backend.waitIdle();
  }

  void cleanup() const { appWindow.destroy(); }

  void recreateSwapChain() {
    backend.recreateSwapchain(appWindow);
    forwardRenderer.recreate(deviceContext(), swapchainContext());
  }

  void drawFrame() {
    auto frameState = backend.beginFrame(appWindow);
    if (!frameState) {
      recreateSwapChain();
      return;
    }

    static auto startTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    float elapsed =
        std::chrono::duration<float>(currentTime - startTime).count();

    uint32_t flag = static_cast<uint32_t>(elapsed / 1.0f) % 3;
    testPass->setFlag(flag);

    auto &commandBuffer =
        commandContext().commandBuffer(frameState->frameIndex);
    std::vector<RenderItem> renderItems = {
        RenderItem{.mesh = &mesh, .descriptorBindings = nullptr}};

    forwardRenderer.record(commandBuffer, swapchainContext(), renderItems,
                           frameState->frameIndex, frameState->imageIndex);

    if (backend.endFrame(*frameState, appWindow)) {
      recreateSwapChain();
    }
  }
};

int main() {
  try {
    RenderCubedApplication app;
    app.run();
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
