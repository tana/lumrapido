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

class RayTracer : public vsg::Inherit<vsg::Object, RayTracer>
{
public:
  RayTracer(vsg::Device* device, int width, int height);

  // Update setting of samples per pixel in uniform buffer
  void setSamplesPerPixel(int samplesPerPixel);
  // Update camera parameters in uniform buffer
  void setCameraParams(const vsg::mat4& viewMat, const vsg::mat4& projectionMat);

  vsg::ref_ptr<vsg::CommandGraph> createCommandGraph(vsg::ref_ptr<vsg::Window> window);

private:
  vsg::Device* device;
  
  VkExtent2D screenSize;

  vsg::ref_ptr<RayTracingUniformValue> uniformValue;  // Parameters for ray tracing

  vsg::ref_ptr<vsg::ShaderStage> rayGenerationShader, missShader, closestHitShader;
  vsg::ref_ptr<vsg::RayTracingShaderGroup> rayGenerationShaderGroup, missShaderGroup, closestHitShaderGroup;

  vsg::ref_ptr<vsg::Image> targetImage; // Image to render result of ray tracing
  vsg::ref_ptr<vsg::ImageView> targetImageView;

  vsg::ref_ptr<vsg::DescriptorAccelerationStructure> tlasDescriptor;
  vsg::ref_ptr<vsg::DescriptorImage> targetImageDescriptor;
  vsg::ref_ptr<vsg::DescriptorBuffer> uniformDescriptor, objectInfoDescriptor, indicesDescriptor, verticesDescriptor, normalsDescriptor, texCoordsDescriptor;
  vsg::ref_ptr<vsg::DescriptorSet> descriptorSet;
  vsg::ref_ptr<vsg::PipelineLayout> pipelineLayout;
  vsg::ref_ptr<vsg::RayTracingPipeline> rayTracingPipeline;
};
