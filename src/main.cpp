#include <iostream>
#include <chrono>
#include <vsg/all.h>
#include <vsgXchange/all.h>
#include "utils.h"
#include "RayTracingUniform.h"
#include "RayTracingMaterialGroup.h"
#include "SceneConversionTraversal.h"

// Real-time ray tracing using Vulkan Ray Tracing extension
// Based on VSG's vsgraytracing example:
//  https://github.com/vsg-dev/vsgExamples/blob/master/examples/raytracing/vsgraytracing/vsgraytracing.cpp
// Ray tracing algorithm and scene are based on "Ray Tracing in One Weekend" (v3.2.3) written by Peter Shirley:
//  https://raytracing.github.io/books/RayTracingInOneWeekend.html
// Vulkan-specific designs are based on NVIDIA Vulkan Ray Tracing tutorial:
//  https://nvpro-samples.github.io/vk_raytracing_tutorial_KHR/

const int SCREEN_WIDTH = 400;
const int SCREEN_HEIGHT = 225;

const uint32_t SAMPLES_PER_PIXEL = 100;

const int FPS_MEASURE_COUNT = 100;

int main(int argc, char* argv[])
{
  auto options = vsg::Options::create(vsgXchange::all::create());

  // Define materials used in the scene
  RayTracingMaterial groundMaterial;
  groundMaterial.type = RT_MATERIAL_LAMBERT;
  groundMaterial.color = vsg::vec3(0.8f, 0.8f, 0.0f);
  RayTracingMaterial centerMaterial;
  centerMaterial.type = RT_MATERIAL_LAMBERT;
  centerMaterial.color = vsg::vec3(0.1f, 0.2f, 0.5f);
  //centerMaterial.roughness = 1.0f;
  //centerMaterial.metallic = 0.0f;
  RayTracingMaterial leftMaterial;
  leftMaterial.type = RT_MATERIAL_PBR;
  leftMaterial.color = vsg::vec3(1.0f, 1.0f, 1.0f);
  leftMaterial.roughness = 0.0f;
  leftMaterial.metallic = 1.0f;
  RayTracingMaterial rightMaterial;
  rightMaterial.type = RT_MATERIAL_PBR;
  rightMaterial.color = vsg::vec3(0.8f, 0.6f, 0.2f);
  rightMaterial.roughness = 0.2f;
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
  leftGroup->addChild(createSphere(vsg::vec3(-1.0f, 0.0f, -1.0f), -0.4f));
  scene->addChild(leftGroup);
  auto rightGroup = RayTracingMaterialGroup::create(rightMaterial);
  rightGroup->addChild(createSphere(vsg::vec3(1.0f, 0.0f, -1.0f), 0.5f));
  scene->addChild(rightGroup);

  auto windowTraits = vsg::WindowTraits::create(SCREEN_WIDTH, SCREEN_HEIGHT, "VSGRayTracer");
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
    VK_KHR_SPIRV_1_4_EXTENSION_NAME
  };
  // Enable features related to the above extensions
  windowTraits->deviceFeatures->get<VkPhysicalDeviceAccelerationStructureFeaturesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR>().accelerationStructure = true;
  windowTraits->deviceFeatures->get<VkPhysicalDeviceRayTracingPipelineFeaturesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR>().rayTracingPipeline = true;
  windowTraits->deviceFeatures->get<VkPhysicalDeviceBufferDeviceAddressFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES>().bufferDeviceAddress = true;
  windowTraits->debugLayer = true;  // Enable Vulkan Validation Layer

  auto window = vsg::Window::create(windowTraits);

  vsg::Device* device = window->getOrCreateDevice();  // Handle of a Vulkan device (GPU?)

  auto viewer = vsg::Viewer::create();
  viewer->addWindow(window);

  auto perspective = vsg::Perspective::create(90.0, double(SCREEN_WIDTH) / double(SCREEN_HEIGHT), 0.1, 1000.0);
  auto lookAt = vsg::LookAt::create(vsg::dvec3(0, 0, 0), vsg::dvec3(0, 0, -1), vsg::dvec3(0, 1, 0));
  auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

  viewer->addEventHandler(vsg::CloseHandler::create(viewer));
  viewer->addEventHandler(vsg::Trackball::create(camera));

  // Parameters for ray tracing
  auto uniformValue = RayTracingUniformValue::create();
  uniformValue->value().samplesPerPixel = SAMPLES_PER_PIXEL;
  // Ray generation shader uses inverse of projection and view matrices
  lookAt->get_inverse(uniformValue->value().invViewMat);
  perspective->get_inverse(uniformValue->value().invProjectionMat);

  // Load shaders
  auto rayGenerationShader = vsg::ShaderStage::read(VK_SHADER_STAGE_RAYGEN_BIT_KHR, "main", "shaders/rayGeneration.spv");
  auto missShader = vsg::ShaderStage::read(VK_SHADER_STAGE_MISS_BIT_KHR, "main", "shaders/miss.spv");
  auto closestHitShader = vsg::ShaderStage::read(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, "main", "shaders/closestHit.spv");
  if (!rayGenerationShader || !missShader || !closestHitShader) {
    std::cout << "Cannot load shaders" << std::endl;
    return -1;
  }

  auto shaderStages = vsg::ShaderStages{ rayGenerationShader, missShader, closestHitShader };

  auto rayGenerationShaderGroup = vsg::RayTracingShaderGroup::create();
  rayGenerationShaderGroup->type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
  rayGenerationShaderGroup->generalShader = 0;  // Index in shaderStages

  auto missShaderGroup = vsg::RayTracingShaderGroup::create();
  missShaderGroup->type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
  missShaderGroup->generalShader = 1; // Index in shaderStages

  auto closestHitShaderGroup = vsg::RayTracingShaderGroup::create();
  closestHitShaderGroup->type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
  closestHitShaderGroup->closestHitShader = 2;  // Index in shaderStages

  auto shaderGroups = vsg::RayTracingShaderGroups{ rayGenerationShaderGroup, missShaderGroup, closestHitShaderGroup };

#if 1
  // Convert scene into acceleration structure for ray tracing
  SceneConversionTraversal sceneConversionTraversal(window->getOrCreateDevice());
  scene->accept(sceneConversionTraversal);
  vsg::ref_ptr<vsg::TopLevelAccelerationStructure> tlas = sceneConversionTraversal.tlas;
  auto objectInfo = sceneConversionTraversal.getObjectInfo();
  auto indices = sceneConversionTraversal.getIndices();
  auto vertices = sceneConversionTraversal.getVertices();
  auto normals = sceneConversionTraversal.getNormals();
  auto texCoords = sceneConversionTraversal.getTexCoords();
#else
  auto geom = vsg::AccelerationGeometry::create();
  geom->verts = vsg::vec3Array::create({
    { -1, -1, -2 },
    { 1, -1, -2 },
    { 1, 1, -2 },
    { -1, 1, -2 }
    });
  geom->indices = vsg::ushortArray::create({ 0, 1, 2, 0, 2, 3 });
  auto blas = vsg::BottomLevelAccelerationStructure::create(device);
  blas->geometries.push_back(geom);
  auto instance = vsg::GeometryInstance::create();
  instance->accelerationStructure = blas;
  instance->transform = vsg::mat4();
  auto tlas = vsg::TopLevelAccelerationStructure::create(device);
  tlas->geometryInstances.push_back(instance);

  auto indices = geom->indices;
  auto vertices = geom->verts;
  auto normals = vsg::vec3Array::create({
    { 0, 0, 1 },
    { 0, 0, 1 },
    { 0, 0, 1 },
    { 0, 0, 1 }
  });
  auto texCoords = vsg::vec2Array::create({
    { 0, 1 },
    { 1, 1 },
    { 1, 0 },
    { 0, 0 }
  });

  auto objectInfo = ObjectInfoValue::create();
  objectInfo->value().indexOffset = 0;
  objectInfo->value().vertexOffset = 0;
  objectInfo->value().material.type = RT_MATERIAL_LAMBERT;
  objectInfo->value().material.color = vsg::vec3(1.0f, 0.4f, 0.4f);
#endif

  // Create a target image for rendering
  auto targetImage = vsg::Image::create();
  targetImage->imageType = VK_IMAGE_TYPE_2D;
  targetImage->format = VK_FORMAT_B8G8R8A8_UNORM; // 4-channel, normalized (float in shader but actually integer) See: https://www.khronos.org/opengl/wiki/Image_Format
  targetImage->extent.width = SCREEN_WIDTH;
  targetImage->extent.height = SCREEN_HEIGHT;
  targetImage->extent.depth = 1; // Because this is a 2D image, it has only one depth
  targetImage->mipLevels = 1; // No mipmap
  targetImage->arrayLayers = 1; // Only one layer
  targetImage->samples = VK_SAMPLE_COUNT_1_BIT; // No multisampling
  targetImage->tiling = VK_IMAGE_TILING_OPTIMAL;  // Placed in optimal memory layout
  targetImage->usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  targetImage->sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  targetImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  targetImage->flags = 0;
  // Create an image view as a color image
  auto targetImageView = vsg::createImageView(device, targetImage, VK_IMAGE_ASPECT_COLOR_BIT);
  // Image information for creating a descriptor
  vsg::ImageInfo targetImageInfo(nullptr, targetImageView, VK_IMAGE_LAYOUT_GENERAL);;

  // Descriptor layout which specifies types of descriptors passed to shaders
  vsg::DescriptorSetLayoutBindings descriptorBindings{
    // Binding 0 is an acceleration structure which contains the scene
    { 0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr },
    // Binding 1 is the target image
    { 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr },
    // Binding 2 is the uniform buffer
    { 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr },
    // Binding 3 is array of ObjectInfo, which contains offsets of indices and vertex attributes
    { 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr },
    // Binding 4 is array of indices of all objects combined
    { 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr },
    // Binding 5 is array of vertices of all objects combined
    { 5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr },
    // Binding 6 is array of normals of all objects combined
    { 6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr },
    // Binding 7 is array of texture coords of all objects combined
    { 7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr },
  };
  auto descriptorLayout = vsg::DescriptorSetLayout::create(descriptorBindings);

  // Create descriptors and descriptor set
  auto tlasDescriptor = vsg::DescriptorAccelerationStructure::create(vsg::AccelerationStructures{ tlas }, 0, 0); // Binding 0, first element of the array
  auto targetImageDescriptor = vsg::DescriptorImage::create(targetImageInfo, 1, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // Binding 1
  auto uniformDescriptor = vsg::DescriptorBuffer::create(uniformValue, 2, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);  // Binding 2
  auto objectInfoDescriptor = vsg::DescriptorBuffer::create(objectInfo, 3, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER); // Binding 3
  auto indicesDescriptor = vsg::DescriptorBuffer::create(indices, 4, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER); // Binding 4
  auto verticesDescriptor = vsg::DescriptorBuffer::create(vertices, 5, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER); // Binding 5
  auto normalsDescriptor = vsg::DescriptorBuffer::create(normals, 6, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER); // Binding 6
  auto texCoordsDescriptor = vsg::DescriptorBuffer::create(texCoords, 7, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER); // Binding 7
  auto descriptorSet = vsg::DescriptorSet::create(descriptorLayout, vsg::Descriptors{ tlasDescriptor, targetImageDescriptor, uniformDescriptor, objectInfoDescriptor, indicesDescriptor, verticesDescriptor, normalsDescriptor, texCoordsDescriptor });

  // Create ray tracing pipeline
  auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{ descriptorLayout }, vsg::PushConstantRanges{});
  auto rayTracingPipeline = vsg::RayTracingPipeline::create(pipelineLayout, shaderStages, shaderGroups);

  // Prepare commands for ray tracing
  auto commands = vsg::Commands::create();
  commands->addChild(vsg::BindRayTracingPipeline::create(rayTracingPipeline));
  commands->addChild(vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout, 0, descriptorSet));
  auto traceRaysCommand = vsg::TraceRays::create();
  traceRaysCommand->raygen = rayGenerationShaderGroup;
  traceRaysCommand->missShader = missShaderGroup;
  traceRaysCommand->hitShader = closestHitShaderGroup;
  traceRaysCommand->width = SCREEN_WIDTH;
  traceRaysCommand->height = SCREEN_HEIGHT;
  traceRaysCommand->depth = 1;
  commands->addChild(traceRaysCommand);

  // Command graph to render the result into the window
  auto commandGraph = vsg::CommandGraph::create(window);
  commandGraph->addChild(commands);
  commandGraph->addChild(vsg::CopyImageViewToWindow::create(targetImageView, window));  // Target image is copied into window
  
  viewer->assignRecordAndSubmitTaskAndPresentation({ commandGraph });
  viewer->compile();

  // For FPS measurement
  int counter = 0;
  auto lastTime = std::chrono::high_resolution_clock::now();

  while (viewer->advanceToNextFrame()) {
    viewer->handleEvents();

    // Update inverse view matrix in uniform buffer
    // (projection matrix is constant and does not need updating)
    lookAt->get_inverse(uniformValue->value().invViewMat);
    uniformDescriptor->copyDataListToBuffers();

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