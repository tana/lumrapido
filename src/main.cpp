#include <iostream>
#include <chrono>
#include <vsg/all.h>
#include "RayTracer.h"
#include "RayTracingMaterialGroup.h"
#include "SceneConversionTraversal.h"
#include "GLTFLoader.h"
#include "utils.h"

// Real-time ray tracing using Vulkan Ray Tracing extension
// Based on VSG's vsgraytracing example:
//  https://github.com/vsg-dev/vsgExamples/blob/master/examples/raytracing/vsgraytracing/vsgraytracing.cpp
// Ray tracing algorithm and scene are based on "Ray Tracing in One Weekend" (v3.2.3) written by Peter Shirley:
//  https://raytracing.github.io/books/RayTracingInOneWeekend.html
// Vulkan-specific designs are based on NVIDIA Vulkan Ray Tracing tutorial:
//  https://nvpro-samples.github.io/vk_raytracing_tutorial_KHR/

const int DEFAULT_SCREEN_WIDTH = 800;
const int DEFAULT_SCREEN_HEIGHT = 450;

const uint32_t DEFAULT_SAMPLES_PER_PIXEL = 100;

const int FPS_MEASURE_COUNT = 100;

vsg::ref_ptr<RayTracingScene> createDefaultScene(vsg::Device* device)
{
  // Define materials used in the scene
  RayTracingMaterial groundMaterial;
  groundMaterial.color = vsg::vec3(0.8f, 0.8f, 0.0f);
  groundMaterial.roughness = 1.0f;
  groundMaterial.metallic = 0.0f;
  RayTracingMaterial centerMaterial;
  centerMaterial.color = vsg::vec3(0.1f, 0.2f, 0.5f);
  centerMaterial.roughness = 0.1f;
  centerMaterial.metallic = 0.0f;
  RayTracingMaterial leftMaterial;
  leftMaterial.color = vsg::vec3(1.0f, 1.0f, 1.0f);
  leftMaterial.roughness = 0.5f;
  leftMaterial.metallic = 1.0f;
  RayTracingMaterial rightMaterial;
  rightMaterial.color = vsg::vec3(0.8f, 0.6f, 0.2f);
  rightMaterial.roughness = 0.0f;
  rightMaterial.metallic = 1.0f;

  // Scene to render
  auto scene = vsg::Group::create();
  auto groundGroup = RayTracingMaterialGroup::create(groundMaterial);
  groundGroup->addChild(createSphere(vsg::vec3(0.0f, -100.5f, -1.0f), 100.0f));
  //groundGroup->addChild(createQuad(vsg::vec3(0.0f, -0.5f, -1.0f), vsg::vec3(0.0f, 1.0f, 0.0f), vsg::vec3(0.0f, 0.0f, -1.0f), 100.0f, 100.0f));
  scene->addChild(groundGroup);
  auto centerGroup = RayTracingMaterialGroup::create(centerMaterial);
  centerGroup->addChild(createSphere(vsg::vec3(0.0f, 0.0f, -1.0f), 0.5f));
  scene->addChild(centerGroup);
  auto leftGroup = RayTracingMaterialGroup::create(leftMaterial);
  leftGroup->addChild(createSphere(vsg::vec3(-1.0f, 0.0f, -1.0f), 0.5f));
  scene->addChild(leftGroup);
  auto rightGroup = RayTracingMaterialGroup::create(rightMaterial);
  rightGroup->addChild(createSphere(vsg::vec3(1.0f, 0.0f, -1.0f), 0.5f));
  scene->addChild(rightGroup);

  // Convert scene into acceleration structure for ray tracing
  SceneConversionTraversal sceneConversionTraversal(device);
  scene->accept(sceneConversionTraversal);

  return sceneConversionTraversal.scene;
}

int main(int argc, char* argv[])
{
  // Use VSG's option parser to handle command line arguments
  vsg::CommandLine arguments(&argc, argv);
  bool useDebugLayer = arguments.read({ "--debug" });
  auto cameraPos = arguments.value(vsg::dvec3(0.0, 0.0, 1.0), { "--camera", "-c" });
  auto lookAtPos = arguments.value(vsg::dvec3(0.0, 0.0, 0.0), { "--lookat", "-l" });
  auto cameraUpVec = arguments.value(vsg::dvec3(0.0, 1.0, 0.0), { "--camera-up", "-u" });
  double fov = arguments.value(90.0, { "--camera-fov", "-f" });
  uint32_t samplesPerPixel = arguments.value(DEFAULT_SAMPLES_PER_PIXEL, { "--samples", "-s" });
  std::string envMapFile = arguments.value<std::string>("", { "--envmap", "-e" });
  int screenWidth = arguments.value<int>(DEFAULT_SCREEN_WIDTH, { "--screen-width", "-W" });
  int screenHeight = arguments.value<int>(DEFAULT_SCREEN_HEIGHT, { "--screen-height", "-H" });
  std::string algorithmName = arguments.value<std::string>("pt", { "--algorithm", "-a" });

  SamplingAlgorithm algorithm;
  if (algorithmName == "pt") {
    algorithm = SamplingAlgorithm::PATH_TRACING;
  } else if (algorithmName == "qmc") {
    algorithm = SamplingAlgorithm::QUASI_MONTE_CARLO;
  }

  std::string gltfFile;
  // Flags such as "--debug" are removed by arguments.read calls above
  if (arguments.argc() >= 2) {
    gltfFile = arguments[1];
  }

  auto windowTraits = vsg::WindowTraits::create(screenWidth, screenHeight, "VSGRayTracer");
  windowTraits->queueFlags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;  // Because ray tracing needs compute queue. See: https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdTraceRaysKHR.html#VkQueueFlagBits
  windowTraits->swapchainPreferences.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;  // The screen can be target of image-to-image copy
  // Ray tracing requires Vulkan 1.1
  windowTraits->vulkanVersion = VK_API_VERSION_1_1;
  // Vulkan extensions required for ray tracing
  windowTraits->deviceExtensionNames = {
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
    VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
    // Below are extensions required by the above two
    VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
    VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,
    VK_KHR_SPIRV_1_4_EXTENSION_NAME,
    // Below are for shaders
    VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME
  };
  // Enable features related to the above extensions and GLSL extensions used in shaders
  windowTraits->deviceFeatures->get<VkPhysicalDeviceAccelerationStructureFeaturesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR>().accelerationStructure = true;
  windowTraits->deviceFeatures->get<VkPhysicalDeviceRayTracingPipelineFeaturesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR>().rayTracingPipeline = true;
  windowTraits->deviceFeatures->get<VkPhysicalDeviceBufferDeviceAddressFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES>().bufferDeviceAddress = true;
  windowTraits->deviceFeatures->get().shaderInt16 = true;
  windowTraits->deviceFeatures->get<VkPhysicalDevice16BitStorageFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES>().storageBuffer16BitAccess = true;
  windowTraits->deviceFeatures->get<VkPhysicalDeviceScalarBlockLayoutFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES>().scalarBlockLayout = true;
  // Enable Vulkan validation layer if specified by command line argument
  windowTraits->debugLayer = useDebugLayer;

  auto window = vsg::Window::create(windowTraits);

  vsg::Device* device = window->getOrCreateDevice();  // Handle of a Vulkan device (GPU?)

  vsg::ref_ptr<RayTracingScene> scene;
  if (!gltfFile.empty()) {
    // Load scene from a GLTF file
    scene = RayTracingScene::create(device);
    GLTFLoader loader(scene);
    if (!loader.loadFile(gltfFile)) {
      std::cerr << "GLTF load error" << std::endl;
      return -1;
    }
  } else {
    // Use default scene
    scene = createDefaultScene(device);
  }

  if (!envMapFile.empty()) {
    auto envMap = loadEXRTexture(envMapFile);
    if (!envMap) {
      std::cerr << "Environment map load error" << std::endl;
      return -1;
    }
    
    scene->envMap = envMap;
  } else {
    // Use default environment map (1x1 px and value is 1.0)
    scene->envMap = vsg::vec3Array2D::create(1, 1, vsg::vec3(1.0f, 1.0f, 1.0f), vsg::Data::Layout{ VK_FORMAT_R32G32B32_SFLOAT });
  }

  auto rayTracer = RayTracer::create(device, screenWidth, screenHeight, scene, algorithm);

  auto viewer = vsg::Viewer::create();
  viewer->addWindow(window);

  auto perspective = vsg::Perspective::create(fov, double(screenWidth) / double(screenHeight), 0.1, 1000.0);
  auto lookAt = vsg::LookAt::create(cameraPos, lookAtPos, cameraUpVec);
  auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

  viewer->addEventHandler(vsg::CloseHandler::create(viewer));
  viewer->addEventHandler(vsg::Trackball::create(camera));

  rayTracer->setSamplesPerPixel(samplesPerPixel);

  // Ray generation shader uses inverse of projection and view matrices
  vsg::dmat4 viewMat, projectionMat;
  lookAt->get(viewMat);
  perspective->get(projectionMat);
  rayTracer->setCameraParams(viewMat, projectionMat);

  viewer->assignRecordAndSubmitTaskAndPresentation({ rayTracer->createCommandGraph(window) });
  viewer->compile();

  // For FPS measurement
  int counter = 0;
  auto lastTime = std::chrono::high_resolution_clock::now();

  while (viewer->advanceToNextFrame()) {
    viewer->handleEvents();

    lookAt->get(viewMat);
    rayTracer->setCameraParams(viewMat, projectionMat);

    viewer->update();
    viewer->recordAndSubmit();
    viewer->present();

    // FPS measurement
    ++counter;
    if (counter >= FPS_MEASURE_COUNT) {
      counter = 0;
      auto elapsed = std::chrono::high_resolution_clock::now() - lastTime;
      long long fps = std::chrono::seconds(FPS_MEASURE_COUNT) / elapsed;
      std::cout << fps << " fps" << std::endl;
      lastTime = std::chrono::high_resolution_clock::now();
    }
  }

  return 0;
}