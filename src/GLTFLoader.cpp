#include "GLTFLoader.h"

#include <filesystem>
#include <iostream>
#include <vsg/maths/transform.h>
#include <vsg/maths/quat.h>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"

#include "gltfUtils.h"

GLTFLoader::GLTFLoader(vsg::ref_ptr<RayTracingScene> scene)
  : scene(scene)
{
}

bool GLTFLoader::loadFile(std::string path)
{
  tinygltf::TinyGLTF tinyGLTF;
  tinygltf::Model model;

  bool ret;
  std::string error;
  std::string warning;

  auto ext = std::filesystem::path(path).extension();
  if (ext == ".gltf") {
    ret = tinyGLTF.LoadASCIIFromFile(&model, &error, &warning, path);
  } else {  // Files with other extensions are treated as a GLB file
    ret = tinyGLTF.LoadBinaryFromFile(&model, &error, &warning, path);
  }

  if (!ret) {
    std::cerr << error << std::endl;
    return false;
  }

  if (!warning.empty()) {
    std::cerr << warning << std::endl;
  }

  return loadModel(model);
}

bool GLTFLoader::loadModel(tinygltf::Model& model)
{
  bool ret = true;
  for (auto& gltfScene : model.scenes) {
    ret &= loadScene(gltfScene, model);
  }
  return ret;
}

bool GLTFLoader::loadScene(tinygltf::Scene& gltfScene, tinygltf::Model& model)
{
  bool ret = true;
  for (auto& nodeIdx : gltfScene.nodes) {
    ret &= loadNode(model.nodes[nodeIdx], model, vsg::mat4());
  }
  return ret;
}

bool GLTFLoader::loadNode(tinygltf::Node& node, tinygltf::Model& model, vsg::mat4& parentTransform)
{
  bool ret = true;

  // Transform matrix of a node
  // No special handling is needed because both VSG and GLTF use column-major format for matrices.
  vsg::mat4 transform;  // Default is identity matrix

  if (!node.matrix.empty()) {
    transform = vsg::dmat4(node.matrix.data());
  }

  if (!node.translation.empty()) {
    transform = transform * vsg::translate(float(node.translation[0]), float(node.translation[1]), float(node.translation[2]));
  }
  if (!node.rotation.empty()) {
    vsg::quat quat(float(node.rotation[0]), float(node.rotation[1]), float(node.rotation[2]), float(node.rotation[3]));
    transform = transform * vsg::rotate(quat);
  }
  if (!node.scale.empty()) {
    transform = transform * vsg::scale(float(node.scale[0]), float(node.scale[1]), float(node.scale[2]));
  }

  if (node.mesh >= 0) {
    ret &= loadMesh(model.meshes[node.mesh], model, parentTransform * transform);
  }

  for (auto& childIdx : node.children) {
    ret &= loadNode(model.nodes[childIdx], model, parentTransform * transform);
  }

  return ret;
}

bool GLTFLoader::loadMesh(tinygltf::Mesh& mesh, tinygltf::Model& model, vsg::mat4& transform)
{
  bool ret = true;
  for (auto& primitive : mesh.primitives) {
    ret &= loadPrimitive(primitive, model, transform);
  }
  return ret;
}

bool GLTFLoader::loadPrimitive(tinygltf::Primitive& primitive, tinygltf::Model& model, vsg::mat4& transform)
{
  if (primitive.mode != TINYGLTF_MODE_TRIANGLES) {
    std::cerr << "Only triangle meshes are supported" << std::endl;
    return false;
  }

  auto indices = readGLTFBuffer<uint16_t>(primitive.indices, model);
  auto vertices = readGLTFBuffer<vsg::vec3>(primitive.attributes["POSITION"], model);
  auto normals = readGLTFBuffer<vsg::vec3>(primitive.attributes["NORMAL"], model);
  auto texCoords = readGLTFBuffer<vsg::vec2>(primitive.attributes["TEXCOORD_0"], model);

  RayTracingMaterial material;
  material.type = RT_MATERIAL_LAMBERT;
  material.color = vsg::vec3(1.0, 1.0, 1.0);

  scene->addMesh(transform, indices, vertices, normals, texCoords, material);

  return true;
}
