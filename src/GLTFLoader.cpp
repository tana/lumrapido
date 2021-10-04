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

  std::optional<RayTracingMaterial> material = loadMaterial(model.materials[primitive.material], model);
  if (!material) {
    return false;
  }

  scene->addMesh(transform, indices, vertices, normals, texCoords, material.value());

  return true;
}

std::optional<RayTracingMaterial> GLTFLoader::loadMaterial(const tinygltf::Material& gltfMaterial, const tinygltf::Model& model)
{
  const tinygltf::PbrMetallicRoughness& pbr = gltfMaterial.pbrMetallicRoughness;
  
  RayTracingMaterial material;
  material.type = RT_MATERIAL_PBR;

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

  return material;
}

std::optional<uint32_t> GLTFLoader::loadTexture(const tinygltf::Texture& gltfTexture, const tinygltf::Model& model)
{
  const tinygltf::Image& gltfImage = model.images[gltfTexture.source];
  
  vsg::ref_ptr<vsg::Data> imageData = readImageData(gltfImage.image, gltfImage.width, gltfImage.height, gltfImage.component, gltfImage.pixel_type);

  auto image = vsg::Image::create(imageData);
  image->usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  image->format = VK_FORMAT_R32G32B32_SFLOAT;
  image->tiling = VK_IMAGE_TILING_LINEAR;
  auto imageView = vsg::ImageView::create(image);
  auto sampler = vsg::Sampler::create();
  // TODO: sampler setting

  return scene->addTexture(vsg::ImageInfo(sampler, imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
}

vsg::ref_ptr<vsg::vec3Array2D> GLTFLoader::readImageData(const std::vector<unsigned char>& data, int width, int height, int numComp, int compType)
{
  // Image format have to be specified in vsg::Array2D creation.
  // It did not work when tried with format property of vsg::Image only.
  //  See: https://github.com/vsg-dev/vsgXchange/blob/fb0f0754b72112edb821814f28d25a070790a89a/src/stbi/stbi.cpp#L134
  auto imageData = vsg::vec3Array2D::create(width, height, vsg::Data::Layout{ VK_FORMAT_R32G32B32_SFLOAT });

  size_t compSize = sizeOfGLTFComponentType(compType);

  vsg::vec3 pixelValue;

  for (size_t i = 0; i < height; ++i) {
    for (size_t j = 0; j < width; ++j) {
      for (int k = 0; k < std::min(numComp, 3); ++k) {
        size_t pos = ((i * width + j) * numComp + k) * compSize;
        // TODO: Support component types other than 8-bit (scaling other than 256)
        pixelValue[k] = readComponentAndConvert<float>(data, pos, compType) / 256.0f;
      }
      imageData->at(i * width + j) = pixelValue;
    }
  }

  return imageData;
}
