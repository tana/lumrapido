#pragma once

#include <cstdint>
#include <vsg/maths/mat4.h>
#include <vsg/core/Inherit.h>
#include <vsg/core/Value.h>

struct RayTracingUniform
{
  vsg::mat4 invViewMat; // Inverse of view matrix (i.e. transform camera coordinate to world coordinate)
  vsg::mat4 invProjectionMat; // Inverse of projection matrix (i.e. transform normalized device coordinate into camera coordinate)
  uint32_t samplesPerPixel;
  int32_t envMapTextureIdx = -1;
};

// This inherits vsg::Data and it can be passed to vsg::DescriptorBuffer::create
class RayTracingUniformValue : public vsg::Inherit<vsg::Value<RayTracingUniform>, RayTracingUniformValue>
{
};