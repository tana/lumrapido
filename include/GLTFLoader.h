#pragma once

#include <string>
#include <vsg/maths/mat4.h>
#include "tiny_gltf.h"
#include "RayTracingScene.h"

class GLTFLoader
{
public:
  GLTFLoader(vsg::ref_ptr<RayTracingScene> scene);

  bool loadFile(const std::string& path);

protected:
  bool loadModel(const tinygltf::Model& model);
  bool loadScene(const tinygltf::Scene& gltfScene, const tinygltf::Model& model);
  bool loadNode(const tinygltf::Node& node, const tinygltf::Model& model, const vsg::mat4& parentTransform);
  bool loadMesh(const tinygltf::Mesh& mesh, const tinygltf::Model& model, const vsg::mat4& transform);
  bool loadPrimitive(const tinygltf::Primitive& primitive, const tinygltf::Model& model, const vsg::mat4& transform);

  vsg::ref_ptr<RayTracingScene> scene;
};
