#include "RayTracer.h"

#include <cstdint>
#include <iostream>
#include <vsg/all.h>
#include "RayTracingUniform.h"
#include "hammersley.h"

RayTracer::RayTracer(vsg::Device* device, int width, int height, vsg::ref_ptr<RayTracingScene> scene, SamplingAlgorithm algorithm)
  : device(device), screenSize({ uint32_t(width), uint32_t(height) }),
    scene(scene),
    algorithm(algorithm)
{
  uniformValue = RayTracingUniformValue::create();

  // Choose ray generation shader for specified sampling algorithm
  std::string rayGenerationShaderPath;
  switch (algorithm) {
  case SamplingAlgorithm::PATH_TRACING:
    rayGenerationShaderPath = "shaders/rayGeneration.spv";
    break;
  case SamplingAlgorithm::QUASI_MONTE_CARLO:
    rayGenerationShaderPath = "shaders/rayGenerationQMC.spv";
    break;
  default:
    break;
  }

  // Load shaders
  rayGenerationShader = vsg::ShaderStage::read(VK_SHADER_STAGE_RAYGEN_BIT_KHR, "main", rayGenerationShaderPath);
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

  vsg::ref_ptr<vsg::TopLevelAccelerationStructure> tlas = scene->tlas;
  auto objectInfo = scene->getObjectInfo();
  auto indices = scene->getIndices();
  auto vertices = scene->getVertices();
  auto normals = scene->getNormals();
  auto texCoords = scene->getTexCoords();
  auto tangents = scene->getTangents();

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
    // Binding 8 is array of tangents of all objects combined
    { 8, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr },
    // Binding 10 is textures
    { 10, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, uint32_t(MAX_NUM_TEXTURES), VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr },
    // Binding 12 is environment map
    { 12, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_MISS_BIT_KHR, nullptr }
  };
  // If algorithm is QMC, add binding for low-discrepancy sequence (Binding 11)
  if (algorithm == SamplingAlgorithm::QUASI_MONTE_CARLO) {
    descriptorBindings.push_back(VkDescriptorSetLayoutBinding{ 11, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr });
  }
  auto descriptorLayout = vsg::DescriptorSetLayout::create(descriptorBindings);

  // Create descriptors
  tlasDescriptor = vsg::DescriptorAccelerationStructure::create(vsg::AccelerationStructures{ tlas }, 0, 0); // Binding 0, first element of the array
  targetImageDescriptor = vsg::DescriptorImage::create(targetImageInfo, 1, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); // Binding 1
  uniformDescriptor = vsg::DescriptorBuffer::create(uniformValue, 2, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);  // Binding 2
  objectInfoDescriptor = vsg::DescriptorBuffer::create(objectInfo, 3, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER); // Binding 3
  indicesDescriptor = vsg::DescriptorBuffer::create(indices, 4, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER); // Binding 4
  verticesDescriptor = vsg::DescriptorBuffer::create(vertices, 5, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER); // Binding 5
  normalsDescriptor = vsg::DescriptorBuffer::create(normals, 6, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER); // Binding 6
  texCoordsDescriptor = vsg::DescriptorBuffer::create(texCoords, 7, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER); // Binding 7
  tangentsDescriptor = vsg::DescriptorBuffer::create(tangents, 8, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER); // Binding 8

  // Prepare descriptor for texture
  auto emptyImageData = vsg::vec3Array2D::create(1, 1, vsg::Data::Layout{ VK_FORMAT_R32G32B32_SFLOAT });
  auto emptyImage = vsg::Image::create(emptyImageData);
  emptyImage->usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  emptyImage->format = VK_FORMAT_R32G32B32_SFLOAT;
  emptyImage->tiling = VK_IMAGE_TILING_LINEAR;
  vsg::ImageInfo emptyImageInfo(vsg::Sampler::create(), vsg::ImageView::create(emptyImage), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  vsg::ImageInfoList imageInfoList(MAX_NUM_TEXTURES, emptyImageInfo);
  std::copy(
    scene->textures.cbegin(),
    scene->textures.cbegin() + std::min(MAX_NUM_TEXTURES, scene->textures.size()),
    imageInfoList.begin());
  textureDescriptor = vsg::DescriptorImage::create(imageInfoList, 10, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // Binding 10

  // When algorithm is QMC, create a descriptor for low-discrepancy sequence
  if (algorithm == SamplingAlgorithm::QUASI_MONTE_CARLO) {
    hammersley = vsg::floatArray::create(); // Actual content is set later (in setSamplesPerPixel)
    lowDiscrepancySeqDescriptor = vsg::DescriptorBuffer::create(hammersley, 11, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);  // Binding 11
  }

  // Create descriptor for environment map
  envMapDescriptor = vsg::DescriptorImage::create(
    vsg::Sampler::create(),
    scene->envMap,
    12, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // Binding 12

  // Combine descriptor into a descriptor set
  vsg::Descriptors descriptors = { tlasDescriptor, targetImageDescriptor, uniformDescriptor, objectInfoDescriptor, indicesDescriptor, verticesDescriptor, normalsDescriptor, texCoordsDescriptor, tangentsDescriptor, textureDescriptor, envMapDescriptor };
  if (algorithm == SamplingAlgorithm::QUASI_MONTE_CARLO) {
    descriptors.push_back(lowDiscrepancySeqDescriptor);
  }
  descriptorSet = vsg::DescriptorSet::create(descriptorLayout, descriptors);

  // Create ray tracing pipeline
  pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{ descriptorLayout }, vsg::PushConstantRanges{});
  rayTracingPipeline = vsg::RayTracingPipeline::create(pipelineLayout, shaderStages, shaderGroups);
}

void RayTracer::setSamplesPerPixel(int samplesPerPixel)
{
  uniformValue->value().samplesPerPixel = uint32_t(samplesPerPixel);
  uniformDescriptor->copyDataListToBuffers();

  if (algorithm == SamplingAlgorithm::QUASI_MONTE_CARLO) {
    // Generate low-discrepancy sequence for specified number of samples
    int numElems = SAMPLING_DIMENSIONS * samplesPerPixel;
    hammersley->assign(numElems, (float*)malloc(numElems * sizeof(float)));
    generateScrambledHammersley(SAMPLING_DIMENSIONS, samplesPerPixel, hammersley);  // Fill data
  }
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
