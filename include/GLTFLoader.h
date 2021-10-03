#pragma once

#include <string>
#include <vsg/maths/mat4.h>
#include "tiny_gltf.h"
#include "RayTracingScene.h"

class GLTFLoader
{
public:
  GLTFLoader(vsg::ref_ptr<RayTracingScene> scene);

  bool loadFile(std::string path);

protected:
  bool loadModel(tinygltf::Model& model);
  bool loadScene(tinygltf::Scene& gltfScene, tinygltf::Model& model);
  bool loadNode(tinygltf::Node& node, tinygltf::Model& model, vsg::mat4& parentTransform);
  bool loadMesh(tinygltf::Mesh& mesh, tinygltf::Model& model, vsg::mat4& transform);
  bool loadPrimitive(tinygltf::Primitive& primitive, tinygltf::Model& model, vsg::mat4& transform);

  vsg::ref_ptr<RayTracingScene> scene;
};
