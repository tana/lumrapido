#include <iostream>
#include <filesystem>
#include <vsg/all.h>
#include <vsgXchange/all.h>
#include "utils.h"
#include "RayTracingUniform.h"

// Real-time ray tracing using Vulkan Ray Tracing extension
// Based on VSG's vsgraytracing example:
//  https://github.com/vsg-dev/vsgExamples/blob/master/examples/raytracing/vsgraytracing/vsgraytracing.cpp
// Ray tracing algorithm and scene are based on "Ray Tracing in One Weekend" (v3.2.3) written by Peter Shirley:
//  https://raytracing.github.io/books/RayTracingInOneWeekend.html

const int SCREEN_WIDTH = 400;
const int SCREEN_HEIGHT = 225;

int main(int argc, char* argv[])
{
  auto options = vsg::Options::create(vsgXchange::all::create());

  // Scene to render
  auto scene = vsg::Group::create();
  scene->addChild(createSphere(vsg::vec3(0.0f, 0.0f, -1.0f), 0.5f));
  scene->addChild(createSphere(vsg::vec3(0.0f, -100.5f, -1.0f), 100.0f));
  
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

  // Ray generation shader uses inverse of projection and view matrices
  auto uniformValue = RayTracingUniformValue::create();
  lookAt->get_inverse(uniformValue->value().invViewMat);
  perspective->get_inverse(uniformValue->value().invProjectionMat);

  // Load shaders
  auto rayGenerationShader = vsg::read_cast<vsg::ShaderStage>("shaders/rayGeneration.rgen", options);
  auto missShader = vsg::read_cast<vsg::ShaderStage>("shaders/background.rmiss", options);
  auto closestHitShader = vsg::read_cast<vsg::ShaderStage>("shaders/closestHit.rchit", options);
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

  // Convert scene into acceleration structure for ray tracing
  vsg::BuildAccelerationStructureTraversal buildAccelStructTraversal(window->getOrCreateDevice());
  scene->accept(buildAccelStructTraversal);
  vsg::ref_ptr<vsg::TopLevelAccelerationStructure> tlas = buildAccelStructTraversal.tlas;

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
    { 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr }
  };
  auto descriptorLayout = vsg::DescriptorSetLayout::create(descriptorBindings);

  // Create descriptors and descriptor set
  auto tlasDescriptor = vsg::DescriptorAccelerationStructure::create(vsg::AccelerationStructures{ tlas }, 0, 0); // Binding 0, first element of the array
  auto targetImageDescriptor = vsg::DescriptorImage::create(targetImageInfo, 1, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // Binding 1
  auto uniformDescriptor = vsg::DescriptorBuffer::create(uniformValue, 2, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);  // Binding 2
  auto descriptorSet = vsg::DescriptorSet::create(descriptorLayout, vsg::Descriptors{ tlasDescriptor, targetImageDescriptor, uniformDescriptor });

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

  while (viewer->advanceToNextFrame()) {
    viewer->handleEvents();

    // Update inverse view matrix in uniform buffer
    // (projection matrix is constant and does not need updating)
    lookAt->get_inverse(uniformValue->value().invViewMat);
    uniformDescriptor->copyDataListToBuffers();

    viewer->update();
    viewer->recordAndSubmit();
    viewer->present();
  }

  return 0;
}