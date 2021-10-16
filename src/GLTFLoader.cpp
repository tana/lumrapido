#include "GLTFLoader.h"

#include <filesystem>
#include <iostream>
#include <cstring>
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

bool GLTFLoader::loadFile(const std::string& path)
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

bool GLTFLoader::loadModel(const tinygltf::Model& model)
{
  bool ret = true;
  for (auto& gltfScene : model.scenes) {
    ret &= loadScene(gltfScene, model);
  }
  return ret;
}

bool GLTFLoader::loadScene(const tinygltf::Scene& gltfScene, const tinygltf::Model& model)
{
  bool ret = true;
  for (auto& nodeIdx : gltfScene.nodes) {
    ret &= loadNode(model.nodes[nodeIdx], model, vsg::mat4());
  }
  return ret;
}

bool GLTFLoader::loadNode(const tinygltf::Node& node, const tinygltf::Model& model, const vsg::mat4& parentTransform)
{
  bool ret = true;

  // Transform matrix of a node
  // No special handling is needed because both VSG and GLTF use column-major format for matrices.
  vsg::mat4 transform;  // Default is identity matrix

  if (!node.matrix.empty()) {
    transform = vsg::dmat4(
      node.matrix[0], node.matrix[1],node.matrix[2], node.matrix[3],
      node.matrix[4], node.matrix[5],node.matrix[6], node.matrix[7],
      node.matrix[8], node.matrix[9],node.matrix[10], node.matrix[11],
      node.matrix[12], node.matrix[13],node.matrix[14], node.matrix[15]);
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

bool GLTFLoader::loadMesh(const tinygltf::Mesh& mesh, const tinygltf::Model& model, const vsg::mat4& transform)
{
  bool ret = true;
  for (auto& primitive : mesh.primitives) {
    ret &= loadPrimitive(primitive, model, transform);
  }
  return ret;
}

bool GLTFLoader::loadPrimitive(const tinygltf::Primitive& primitive, const tinygltf::Model& model, const vsg::mat4& transform)
{
  if (primitive.mode != TINYGLTF_MODE_TRIANGLES) {
    std::cerr << "Only triangle meshes are supported" << std::endl;
    return false;
  }

  auto indices = readGLTFBuffer<uint16_t>(primitive.indices, model);
  auto vertices = readGLTFBuffer<vsg::vec3>(primitive.attributes.at("POSITION"), model);
  auto normals = readGLTFBuffer<vsg::vec3>(primitive.attributes.at("NORMAL"), model);
  auto texCoords = readGLTFBuffer<vsg::vec2>(primitive.attributes.at("TEXCOORD_0"), model);
  vsg::ref_ptr<vsg::vec4Array> tangents;
  if (primitive.attributes.find("TANGENT") != primitive.attributes.end()) { // Object contains tangent data
    tangents = readGLTFBuffer<vsg::vec4>(primitive.attributes.at("TANGENT"), model);
  } else {
    tangents = vsg::vec4Array::create(vertices->valueCount());
  }

  std::optional<RayTracingMaterial> material = loadMaterial(model.materials[primitive.material], model);
  if (!material) {
    return false;
  }

  scene->addMesh(transform, indices, vertices, normals, texCoords, tangents, material.value());

  return true;
}

std::optional<RayTracingMaterial> GLTFLoader::loadMaterial(const tinygltf::Material& gltfMaterial, const tinygltf::Model& model)
{
  const tinygltf::PbrMetallicRoughness& pbr = gltfMaterial.pbrMetallicRoughness;
  
  RayTracingMaterial material;

  material.color.r = float(pbr.baseColorFactor[0]);
  material.color.g = float(pbr.baseColorFactor[1]);
  material.color.b = float(pbr.baseColorFactor[2]);
  material.metallic = float(pbr.metallicFactor);
  material.roughness = float(pbr.roughnessFactor);

  if (pbr.baseColorTexture.index >= 0) {  // Has a base color texture
    if (pbr.baseColorTexture.texCoord != 0) {
      return std::nullopt;  // Only TEXCOORD_0 is supported
    }

    auto textureIdx = loadTexture(model.textures[pbr.baseColorTexture.index], model);
    if (!textureIdx) {
      return std::nullopt;
    }
    material.colorTextureIdx = textureIdx.value();
  } else {
    material.colorTextureIdx = -1;
  }

  if (pbr.metallicRoughnessTexture.index >= 0) {  // Has a metallic/roughness texture
    if (pbr.metallicRoughnessTexture.texCoord != 0) {
      return std::nullopt;  // Only TEXCOORD_0 is supported
    }

    auto textureIdx = loadTexture(model.textures[pbr.metallicRoughnessTexture.index], model);
    if (!textureIdx) {
      return std::nullopt;
    }
    material.metallicRoughnessTextureIdx = textureIdx.value();
  } else {
    material.metallicRoughnessTextureIdx = -1;
  }

  if (gltfMaterial.normalTexture.index >= 0) {  // Has a normal texture
    if (gltfMaterial.normalTexture.texCoord != 0) {
      return std::nullopt;  // Only TEXCOORD_0 is supported
    }

    auto textureIdx = loadTexture(model.textures[gltfMaterial.normalTexture.index], model);
    if (!textureIdx) {
      return std::nullopt;
    }
    material.normalTextureIdx = textureIdx.value();

    material.normalTextureScale = float(gltfMaterial.normalTexture.scale);
  } else {
    material.normalTextureIdx = -1;
  }

  return material;
}

std::optional<uint32_t> GLTFLoader::loadTexture(const tinygltf::Texture& gltfTexture, const tinygltf::Model& model)
{
  const tinygltf::Image& gltfImage = model.images[gltfTexture.source];
  
  vsg::ref_ptr<vsg::Data> imageData = readImageData(gltfImage.image, gltfImage.width, gltfImage.height, gltfImage.component, gltfImage.pixel_type);

  auto sampler = vsg::Sampler::create();
  // TODO: sampler setting

  return scene->addTexture(imageData, sampler);
}

vsg::ref_ptr<vsg::Data> GLTFLoader::readImageData(const std::vector<unsigned char>& data, int width, int height, int numComp, int compType)
{
  vsg::ref_ptr<vsg::Data> imageData;

  // Image format have to be specified in the layout of a vsg::Data.
  // It did not work when tried with format property of vsg::Image only.
  //  See: https://github.com/vsg-dev/vsgXchange/blob/fb0f0754b72112edb821814f28d25a070790a89a/src/stbi/stbi.cpp#L134

  if (numComp == 1) {
    switch (compType) {
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
      imageData = vsg::ubyteArray2D::create(width, height, vsg::Data::Layout{ VK_FORMAT_R8_UNORM });
      break;
    case TINYGLTF_COMPONENT_TYPE_FLOAT:
      imageData = vsg::floatArray2D::create(width, height, vsg::Data::Layout{ VK_FORMAT_R32_SFLOAT });
      break;
    default:
      return {};  // Unsupported component type
    }
  } else if (numComp == 2) {
    switch (compType) {
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
      imageData = vsg::ubvec2Array2D::create(width, height, vsg::Data::Layout{ VK_FORMAT_R8G8_UNORM });
      break;
    case TINYGLTF_COMPONENT_TYPE_FLOAT:
      imageData = vsg::vec2Array2D::create(width, height, vsg::Data::Layout{ VK_FORMAT_R32G32_SFLOAT });
      break;
    default:
      return {};  // Unsupported component type
    }
  } else if (numComp == 3) {
    switch (compType) {
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
      imageData = vsg::ubvec3Array2D::create(width, height, vsg::Data::Layout{ VK_FORMAT_R8G8B8_UNORM });
      break;
    case TINYGLTF_COMPONENT_TYPE_FLOAT:
      imageData = vsg::vec3Array2D::create(width, height, vsg::Data::Layout{ VK_FORMAT_R32G32B32_SFLOAT });
      break;
    default:
      return {};  // Unsupported component type
    }
  } else if (numComp == 4) {
    switch (compType) {
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
      imageData = vsg::ubvec4Array2D::create(width, height, vsg::Data::Layout{ VK_FORMAT_R8G8B8A8_UNORM });
      break;
    case TINYGLTF_COMPONENT_TYPE_FLOAT:
      imageData = vsg::vec4Array2D::create(width, height, vsg::Data::Layout{ VK_FORMAT_R32G32B32A32_SFLOAT });
      break;
    default:
      return {};  // Unsupported component type
    }
  } else {
    return {};  // Unsupported number of components
  }

  std::memcpy(imageData->dataPointer(), data.data(), imageData->dataSize());

  return imageData;
}
