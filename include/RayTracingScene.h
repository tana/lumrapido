#pragma once

#include <vsg/core/Object.h>
#include <vsg/core/Inherit.h>
#include <vsg/raytracing/TopLevelAccelerationStructure.h>
#include <vsg/maths/mat4.h>
#include <vsg/core/Data.h>
#include <vsg/state/ImageInfo.h>
#include "RayTracingMaterial.h"

struct ObjectInfo
{
  // Offset (first index) of index and vertex attributes of a particular object in respective array
  uint32_t indexOffset;
  uint32_t vertexOffset;
  RayTracingMaterial material;
};

class ObjectInfoValue : public vsg::Inherit<vsg::Value<ObjectInfo>, ObjectInfoValue>
{
};

class RayTracingScene : public vsg::Inherit<vsg::Object, RayTracingScene>
{
public:
  RayTracingScene(vsg::Device* device);

  uint32_t addMesh(const vsg::mat4& transform, vsg::ref_ptr<vsg::ushortArray> indices, vsg::ref_ptr<vsg::vec3Array> vertices, vsg::ref_ptr<vsg::vec3Array> normals, vsg::ref_ptr<vsg::vec2Array> texCoords, vsg::ref_ptr<vsg::vec4Array> tangents, const RayTracingMaterial& material);
  // For meshes without tangent vectors
  uint32_t addMesh(const vsg::mat4& transform, vsg::ref_ptr<vsg::ushortArray> indices, vsg::ref_ptr<vsg::vec3Array> vertices, vsg::ref_ptr<vsg::vec3Array> normals, vsg::ref_ptr<vsg::vec2Array> texCoords, const RayTracingMaterial& material);

  uint32_t addTexture(vsg::ImageInfo imageInfo);

  vsg::ref_ptr<vsg::Array<ObjectInfo>> getObjectInfo() const;
  vsg::ref_ptr<vsg::ushortArray> getIndices() const;
  vsg::ref_ptr<vsg::vec3Array> getVertices() const;
  vsg::ref_ptr<vsg::vec3Array> getNormals() const;
  vsg::ref_ptr<vsg::vec2Array> getTexCoords() const;
  vsg::ref_ptr<vsg::vec4Array> getTangents() const;

  vsg::ref_ptr<vsg::TopLevelAccelerationStructure> tlas;

  vsg::ImageInfoList textures;

private:
  vsg::Device* device;

  std::vector<ObjectInfo> objectInfoList;
  std::vector<vsg::ref_ptr<vsg::ushortArray>> indicesList;
  std::vector<vsg::ref_ptr<vsg::vec3Array>> verticesList;
  std::vector<vsg::ref_ptr<vsg::vec3Array>> normalsList;
  std::vector<vsg::ref_ptr<vsg::vec2Array>> texCoordsList;
  std::vector<vsg::ref_ptr<vsg::vec4Array>> tangentsList;

  uint32_t numIndices;
  uint32_t numVertices;
};
