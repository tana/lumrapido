#include "gltfUtils.h"

size_t sizeOfGLTFComponentType(int compType)
{
  switch (compType) {
  case TINYGLTF_COMPONENT_TYPE_BYTE:
  case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
    return 1;
  case TINYGLTF_COMPONENT_TYPE_SHORT:
  case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
    return 2;
  case TINYGLTF_COMPONENT_TYPE_INT:
  case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
  case TINYGLTF_COMPONENT_TYPE_FLOAT:
    return 4;
  case TINYGLTF_COMPONENT_TYPE_DOUBLE:
    return 8;
  default:
    assert(false);
    return 0;
  }
}

size_t numComponentsOfGLTFType(int type)
{
  switch (type) {
  case TINYGLTF_TYPE_SCALAR:
    return 1;
  case TINYGLTF_TYPE_VEC2:
    return 2;
  case TINYGLTF_TYPE_VEC3:
    return 3;
  case TINYGLTF_TYPE_VEC4:
    return 4;
  case TINYGLTF_TYPE_MAT2:
    return 4;
  case TINYGLTF_TYPE_MAT3:
    return 9;
  case TINYGLTF_TYPE_MAT4:
    return 16;
  default:
    assert(false);
    return 0;
  }
}
