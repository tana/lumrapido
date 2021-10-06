#pragma once

#include <string>
#include <optional>
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

  std::optional<RayTracingMaterial> loadMaterial(const tinygltf::Material& gltfMaterial, const tinygltf::Model& model);
  std::optional<uint32_t> loadTexture(const tinygltf::Texture& gltfTexture, const tinygltf::Model& model);

  // Read image data and convert into a RGB float array, regardless of original format
  vsg::ref_ptr<vsg::Data> readImageData(const std::vector<unsigned char>& data, int width, int height, int numComp, int compType);

  vsg::ref_ptr<RayTracingScene> scene;
};
