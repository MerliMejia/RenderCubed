#include "backend/AppWindow.h"
#include "backend/BackendConfig.h"
#include "backend/VulkanBackend.h"

#include <cstdlib>
#include <exception>
#include <iostream>

constexpr uint32_t WIDTH = 1280;
constexpr uint32_t HEIGHT = 720;
constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

class MinimalRendererApp {
public:
  void run() {
    initWindow();
    initRenderer();
    mainLoop();
    shutdown();
  }

private:
  AppWindow window;
  VulkanBackend backend;
  BackendConfig config{.appName = "Abstracto",
                       .maxFramesInFlight = MAX_FRAMES_IN_FLIGHT};

  void initWindow() { window.create(WIDTH, HEIGHT, "Abstracto"); }

  void initRenderer() {
    backend.initialize(window, config);

    // TODO: Load meshes, textures, and any scene data.
    // TODO: Create render passes and graphics pipelines.
    // TODO: Allocate frame resources, descriptor sets, and uniform buffers.
  }

  void drawFrame() {
    // TODO: Start the frame once the render path is implemented.
    // auto frameState = backend.beginFrame(window);
    // if (!frameState.has_value()) {
    //   backend.recreateSwapchain(window);
    //   return;
    // }

    // TODO: Record commands into:
    // backend.commands().commandBuffer(frameState->frameIndex)

    // TODO: Submit and present once command recording is ready.
    // bool shouldRecreate = backend.endFrame(*frameState, window);
    // if (shouldRecreate) {
    //   backend.recreateSwapchain(window);
    // }
  }

  void mainLoop() {
    while (!window.shouldClose()) {
      window.pollEvents();

      // For now, this keeps the window open.
      // Replace this with drawFrame() when the renderer is ready.
      // drawFrame();
    }

    backend.waitIdle();
  }

  void shutdown() { window.destroy(); }
};

int main() {
  try {
    MinimalRendererApp app;
    app.run();
  } catch (const std::exception &error) {
    std::cerr << error.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
