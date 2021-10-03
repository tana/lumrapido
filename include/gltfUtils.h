#pragma once

#include <type_traits>
#include <cstdint>
#include <cassert>
#include <vsg/maths/vec2.h>
#include <vsg/maths/vec3.h>
#include <vsg/maths/vec4.h>
#include <vsg/core/Array.h>

#undef TINYGLTF_IMPLEMENTATION
#undef STB_IMAGE_IMPLEMENTATION
#undef STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"

template<typename T>
struct GetGLTFComponentType;

template<>
struct GetGLTFComponentType<int8_t> { static const int TYPE = TINYGLTF_COMPONENT_TYPE_BYTE; };
template<>
struct GetGLTFComponentType<int16_t> { static const int TYPE = TINYGLTF_COMPONENT_TYPE_SHORT; };
template<>
struct GetGLTFComponentType<int32_t> { static const int TYPE = TINYGLTF_COMPONENT_TYPE_INT; };
template<>
struct GetGLTFComponentType<uint8_t> { static const int TYPE = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE; };
template<>
struct GetGLTFComponentType<uint16_t> { static const int TYPE = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT; };
template<>
struct GetGLTFComponentType<uint32_t> { static const int TYPE = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT; };
template<>
struct GetGLTFComponentType<float> { static const int TYPE = TINYGLTF_COMPONENT_TYPE_FLOAT; };
template<>
struct GetGLTFComponentType<double> { static const int TYPE = TINYGLTF_COMPONENT_TYPE_DOUBLE; };

template<typename T>
struct GetGLTFType;

template<typename T>
struct GetGLTFType
{
  static const int TYPE = TINYGLTF_TYPE_SCALAR;
  static const int COMP_TYPE = GetGLTFComponentType<T>::TYPE;
};

template<typename CompType>
struct GetGLTFType<vsg::t_vec2<CompType>> {
  static const int TYPE = TINYGLTF_TYPE_VEC2;
  static const int COMP_TYPE = GetGLTFComponentType<CompType>::TYPE;
};
template<typename CompType>
struct GetGLTFType<vsg::t_vec3<CompType>> {
  static const int TYPE = TINYGLTF_TYPE_VEC3;
  static const int COMP_TYPE = GetGLTFComponentType<CompType>::TYPE;
};
template<typename CompType>
struct GetGLTFType<vsg::t_vec4<CompType>> {
  static const int TYPE = TINYGLTF_TYPE_VEC4;
  static const int COMP_TYPE = GetGLTFComponentType<CompType>::TYPE;
};

size_t sizeOfGLTFComponentType(int compType);

size_t numComponentsOfGLTFType(int type);

template<typename T>
T readComponentAndConvert(std::vector<unsigned char> byteArray, size_t pos, int compType)
{
  switch (compType) {
  case TINYGLTF_COMPONENT_TYPE_BYTE:
    return T(int8_t(byteArray[pos]));
  case TINYGLTF_COMPONENT_TYPE_SHORT:
    return T(int16_t((byteArray[pos + 1] << 8) | byteArray[pos]));
  case TINYGLTF_COMPONENT_TYPE_INT:
    return T(int32_t((byteArray[pos + 3] << 24) | (byteArray[pos + 2] << 16) | (byteArray[pos + 1] << 8) | byteArray[pos]));
  case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
    return T(uint8_t(byteArray[pos]));
  case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
    return T(uint16_t((byteArray[pos + 1] << 8) | byteArray[pos]));
  case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
    return T(uint32_t((byteArray[pos + 3] << 24) | (byteArray[pos + 2] << 16) | (byteArray[pos + 1] << 8) | byteArray[pos]));
  case TINYGLTF_COMPONENT_TYPE_FLOAT:
    {
      uint32_t fourBytes = (byteArray[pos + 3] << 24) | (byteArray[pos + 2] << 16) | (byteArray[pos + 1] << 8) | byteArray[pos];
      return T(*reinterpret_cast<float*>(&fourBytes));
    }
  case TINYGLTF_COMPONENT_TYPE_DOUBLE:
    {
      uint64_t eightBytes = (uint64_t(byteArray[pos + 7]) << 56) | (uint64_t(byteArray[pos + 6]) << 48)
        | (uint64_t(byteArray[pos + 5]) << 40) | (uint64_t(byteArray[pos + 4]) << 32)
        | (byteArray[pos + 3] << 24) | (byteArray[pos + 2] << 16)
        | (byteArray[pos + 1] << 8) | byteArray[pos];
      return T(*reinterpret_cast<double*>(&eightBytes));
    }
  default:
    assert(false);
    return T();
  }
}

template<typename T>
struct GetComponentType;

template<typename T>
struct GetComponentType
{
  using TYPE = typename std::enable_if<std::disjunction<std::is_integral<T>, std::is_floating_point<T>>::value, T>::type;
};

template<typename CompType>
struct GetComponentType<vsg::t_vec2<CompType>> { using TYPE = CompType; };
template<typename CompType>
struct GetComponentType<vsg::t_vec3<CompType>> { using TYPE = CompType; };
template<typename CompType>
struct GetComponentType<vsg::t_vec4<CompType>> { using TYPE = CompType; };

// Enclose the function inside a class because function templates cannot be partially specialized
template<typename T>
struct ComponentsToVectorOrScalar
{
  static
    typename std::enable_if<std::disjunction<std::is_integral<T>, std::is_floating_point<T>>::value, T>::type
  call(std::vector<T> components)
  {
    return components[0];
  }
};

template<typename CompType>
struct ComponentsToVectorOrScalar<vsg::t_vec2<CompType>>
{
  static vsg::t_vec2<CompType> call(std::vector<CompType> components)
  {
    return vsg::t_vec2<CompType>(components[0], components[1]);
  }
};

template<typename CompType>
struct ComponentsToVectorOrScalar<vsg::t_vec3<CompType>>
{
  static vsg::t_vec3<CompType> call(std::vector<CompType> components)
  {
    return vsg::t_vec3<CompType>(components[0], components[1], components[2]);
  }
};

template<typename CompType>
struct ComponentsToVectorOrScalar<vsg::t_vec4<CompType>>
{
  static vsg::t_vec4<CompType> call(std::vector<CompType> components)
  {
    return vsg::t_vec4<CompType>(components[0], components[1], components[2], components[3]);
  }
};


template<typename T>
vsg::ref_ptr<vsg::Array<T>> readGLTFBuffer(int accessorIdx, tinygltf::Model& model)
{
  tinygltf::Accessor& accessor = model.accessors[accessorIdx];

  if (accessor.sparse.isSparse) { // Sparse accessor is not supported
    return {};
  }

  if (accessor.type != GetGLTFType<T>::TYPE) {
    // If the accessor has wrong type, return null pointer
    return {};
  }

  tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
  tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

  auto arr = vsg::Array<T>::create(uint32_t(accessor.count));

  size_t numComp = numComponentsOfGLTFType(accessor.type);
  size_t compSize = sizeOfGLTFComponentType(accessor.componentType);

  size_t stride = (bufferView.byteStride == 0) ? numComp * compSize : bufferView.byteStride;

  std::vector<GetComponentType<T>::TYPE> components(numComp);

  size_t pos = accessor.byteOffset + bufferView.byteOffset;
  for (size_t i = 0; i < accessor.count; ++i) {
    for (size_t j = 0; j < numComp; ++j) {
      components[j] = readComponentAndConvert<GetComponentType<T>::TYPE>(buffer.data, pos + compSize * j, accessor.componentType);
    }

    arr->at(i) = ComponentsToVectorOrScalar<T>::call(components);

    pos += stride;
  }

  return arr;
}
