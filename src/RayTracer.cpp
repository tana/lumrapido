#include "RayTracer.h"

#include <cstdint>
#include <iostream>
#include "utils.h"
#include "RayTracingUniform.h"
#include "RayTracingMaterialGroup.h"
#include "SceneConversionTraversal.h"

RayTracer::RayTracer(vsg::Device* device, int width, int height)
  : device(device), screenSize({ uint32_t(width), uint32_t(height) })
{
  // Define materials used in the scene
  RayTracingMaterial groundMaterial;
  groundMaterial.type = RT_MATERIAL_PBR;
  groundMaterial.color = vsg::vec3(0.8f, 0.8f, 0.0f);
  groundMaterial.roughness = 1.0f;
  groundMaterial.metallic = 0.0f;
  RayTracingMaterial centerMaterial;
  centerMaterial.type = RT_MATERIAL_PBR;
  centerMaterial.color = vsg::vec3(0.1f, 0.2f, 0.5f);
  centerMaterial.roughness = 0.1f;
  centerMaterial.metallic = 0.0f;
  RayTracingMaterial leftMaterial;
  leftMaterial.type = RT_MATERIAL_PBR;
  leftMaterial.color = vsg::vec3(1.0f, 1.0f, 1.0f);
  leftMaterial.roughness = 0.5f;
  leftMaterial.metallic = 1.0f;
  RayTracingMaterial rightMaterial;
  rightMaterial.type = RT_MATERIAL_PBR;
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

  uniformValue = RayTracingUniformValue::create();

  // Load shaders
  rayGenerationShader = vsg::ShaderStage::read(VK_SHADER_STAGE_RAYGEN_BIT_KHR, "main", "shaders/rayGeneration.spv");
  missShader = vsg::ShaderStage::read(VK_SHADER_STAGE_MISS_BIT_KHR, "main", "shaders/miss.spv");
  closestHitShader = vsg::ShaderStage::read(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, "main", "shaders/closestHit.spv");
  if (!rayGenerationShader || !missShader || !closestHitShader) {
    std::cout << "Cannot load shaders" << std::endl;
  }

  auto shaderStages = vsg::ShaderStages{ rayGenerationShader, missShader, closestHitShader };

  rayGenerationShaderGroup = vsg::RayTracingShaderGroup::create();
  rayGenerationShaderGroup->type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
  rayGenerationShaderGroup->generalShader = 0;  // Index in shaderStages

  missShaderGroup = vsg::RayTracingShaderGroup::create();
  missShaderGroup->type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
  missShaderGroup->generalShader = 1; // Index in shaderStages

  closestHitShaderGroup = vsg::RayTracingShaderGroup::create();
  closestHitShaderGroup->type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
  closestHitShaderGroup->closestHitShader = 2;  // Index in shaderStages

  auto shaderGroups = vsg::RayTracingShaderGroups{ rayGenerationShaderGroup, missShaderGroup, closestHitShaderGroup };

  // Convert scene into acceleration structure for ray tracing
  SceneConversionTraversal sceneConversionTraversal(device);
  scene->accept(sceneConversionTraversal);
  vsg::ref_ptr<vsg::TopLevelAccelerationStructure> tlas = sceneConversionTraversal.tlas;
  auto objectInfo = sceneConversionTraversal.getObjectInfo();
  auto indices = sceneConversionTraversal.getIndices();
  auto vertices = sceneConversionTraversal.getVertices();
  auto normals = sceneConversionTraversal.getNormals();
  auto texCoords = sceneConversionTraversal.getTexCoords();

  // Create a target image for rendering
  targetImage = vsg::Image::create();
  targetImage->imageType = VK_IMAGE_TYPE_2D;
  targetImage->format = VK_FORMAT_B8G8R8A8_UNORM; // 4-channel, normalized (float in shader but actually integer) See: https://www.khronos.org/opengl/wiki/Image_Format
  targetImage->extent.width = screenSize.width;
  targetImage->extent.height = screenSize.height;
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
  targetImageView = vsg::createImageView(device, targetImage, VK_IMAGE_ASPECT_COLOR_BIT);
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
  tlasDescriptor = vsg::DescriptorAccelerationStructure::create(vsg::AccelerationStructures{ tlas }, 0, 0); // Binding 0, first element of the array
  targetImageDescriptor = vsg::DescriptorImage::create(targetImageInfo, 1, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // Binding 1
  uniformDescriptor = vsg::DescriptorBuffer::create(uniformValue, 2, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);  // Binding 2
  objectInfoDescriptor = vsg::DescriptorBuffer::create(objectInfo, 3, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER); // Binding 3
  indicesDescriptor = vsg::DescriptorBuffer::create(indices, 4, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER); // Binding 4
  verticesDescriptor = vsg::DescriptorBuffer::create(vertices, 5, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER); // Binding 5
  normalsDescriptor = vsg::DescriptorBuffer::create(normals, 6, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER); // Binding 6
  texCoordsDescriptor = vsg::DescriptorBuffer::create(texCoords, 7, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER); // Binding 7
  descriptorSet = vsg::DescriptorSet::create(descriptorLayout, vsg::Descriptors{ tlasDescriptor, targetImageDescriptor, uniformDescriptor, objectInfoDescriptor, indicesDescriptor, verticesDescriptor, normalsDescriptor, texCoordsDescriptor });

  // Create ray tracing pipeline
  pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{ descriptorLayout }, vsg::PushConstantRanges{});
  rayTracingPipeline = vsg::RayTracingPipeline::create(pipelineLayout, shaderStages, shaderGroups);
}

void RayTracer::setSamplesPerPixel(int samplesPerPixel)
{
  uniformValue->value().samplesPerPixel = uint32_t(samplesPerPixel);
  uniformDescriptor->copyDataListToBuffers();
}

void RayTracer::setCameraParams(const vsg::mat4& viewMat, const vsg::mat4& projectionMat)
{
  uniformValue->value().invViewMat = vsg::inverse(viewMat);
  uniformValue->value().invProjectionMat = vsg::inverse(projectionMat);
  uniformDescriptor->copyDataListToBuffers();
}

vsg::ref_ptr<vsg::CommandGraph> RayTracer::createCommandGraph(vsg::ref_ptr<vsg::Window> window)
{
  // Prepare commands for ray tracing
  auto commands = vsg::Commands::create();
  commands->addChild(vsg::BindRayTracingPipeline::create(rayTracingPipeline));
  commands->addChild(vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout, 0, descriptorSet));
  auto traceRaysCommand = vsg::TraceRays::create();
  traceRaysCommand->raygen = rayGenerationShaderGroup;
  traceRaysCommand->missShader = missShaderGroup;
  traceRaysCommand->hitShader = closestHitShaderGroup;
  traceRaysCommand->width = screenSize.width;
  traceRaysCommand->height = screenSize.height;
  traceRaysCommand->depth = 1;
  commands->addChild(traceRaysCommand);

  // Command graph to render the result into the window
  auto commandGraph = vsg::CommandGraph::create(window);
  commandGraph->addChild(commands);
  commandGraph->addChild(vsg::CopyImageViewToWindow::create(targetImageView, window));  // Target image is copied into window

  return commandGraph;
}
