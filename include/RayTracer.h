#pragma once

#include <vsg/core/Inherit.h>
#include <vsg/core/Object.h>
#include <vsg/core/ref_ptr.h>
#include <vsg/vk/Device.h>
#include <vsg/viewer/Window.h>
#include <vsg/maths/mat4.h>
#include <vsg/raytracing/RayTracingShaderGroup.h>
#include <vsg/raytracing/DescriptorAccelerationStructure.h>
#include <vsg/state/DescriptorImage.h>
#include <vsg/state/DescriptorBuffer.h>
#include <vsg/state/DescriptorSet.h>
#include "RayTracingUniform.h"
#include "RayTracingScene.h"

enum class SamplingAlgorithm
{
  PATH_TRACING, QUASI_MONTE_CARLO
};

enum class Bindings : uint32_t
{
  TLAS = 0,
  TARGET_IMAGE = 1,
  UNIFORMS = 2,
  OBJECT_INFOS = 3,
  INDICES = 4,
  VERTICES = 5,
  NORMALS = 6,
  TEX_COORDS = 7,
  TANGENTS = 8,
  TEXTURES = 10,
  HAMMERSLEY = 11,
  ENV_MAP = 12,
  ENV_MAP_PDF = 13,
  ENV_MAP_MARGINAL_CDF = 14,
  ENV_MAP_CONDITIONAL_CDF = 15
};

class RayTracer : public vsg::Inherit<vsg::Object, RayTracer>
{
public:
  RayTracer(vsg::Device* device, int width, int height, vsg::ref_ptr<RayTracingScene> scene, SamplingAlgorithm algorithm = SamplingAlgorithm::PATH_TRACING);

  // Update setting of samples per pixel in uniform buffer
  void setSamplesPerPixel(int samplesPerPixel);
  // Update camera parameters in uniform buffer
  void setCameraParams(const vsg::mat4& viewMat, const vsg::mat4& projectionMat);

  vsg::ref_ptr<vsg::CommandGraph> createCommandGraph(vsg::ref_ptr<vsg::Window> window);

  vsg::ref_ptr<RayTracingScene> scene;

  const size_t MAX_NUM_TEXTURES = 32;  // FIXME: Larger value (limit is unclear) breaks QMC (entire screen becomes blue). Probably GPU memory corruption

  const int MAX_DEPTH = 10;
  const int SAMPLING_DIMENSIONS = 2 + 3 * MAX_DEPTH;  // 2 for antialiasing, 3 per each depth of ray tracing
  const int HAMMERSLEY_REPLICATIONS = 71; // This must agree with the definition in shader rayGeneration.rgen

protected:
  vsg::Device* device;
  
  VkExtent2D screenSize;

  SamplingAlgorithm algorithm;

  vsg::ref_ptr<RayTracingUniformValue> uniformValue;  // Parameters for ray tracing

  vsg::ref_ptr<vsg::ShaderStage> rayGenerationShader, missShader, closestHitShader;
  vsg::ref_ptr<vsg::RayTracingShaderGroup> rayGenerationShaderGroup, missShaderGroup, closestHitShaderGroup;

  vsg::ref_ptr<vsg::Image> targetImage; // Image to render result of ray tracing
  vsg::ref_ptr<vsg::ImageView> targetImageView;

  vsg::ref_ptr<vsg::floatArray> hammersley; // Hammersley sequence for QMC

  vsg::ref_ptr<vsg::DescriptorAccelerationStructure> tlasDescriptor;
  vsg::ref_ptr<vsg::DescriptorImage> targetImageDescriptor;
  vsg::ref_ptr<vsg::DescriptorBuffer> uniformDescriptor, objectInfoDescriptor, indicesDescriptor, verticesDescriptor, normalsDescriptor, texCoordsDescriptor, tangentsDescriptor, hammersleyDescriptor;
  vsg::ref_ptr<vsg::DescriptorImage> textureDescriptor, envMapDescriptor;
  vsg::ref_ptr<vsg::DescriptorImage> envMapPDFDescriptor, envMapMarginalCDFDescriptor, envMapConditionalCDFDescriptor;
  vsg::ref_ptr<vsg::DescriptorSet> descriptorSet;
  vsg::ref_ptr<vsg::PipelineLayout> pipelineLayout;
  vsg::ref_ptr<vsg::RayTracingPipeline> rayTracingPipeline;
};
