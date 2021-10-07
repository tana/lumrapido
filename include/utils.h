#pragma once

#include <vector>
#include <vsg/maths/vec3.h>
#include <vsg/nodes/Node.h>
#include <vsg/utils/Builder.h>
#include <vsg/core/Array.h>

vsg::ref_ptr<vsg::Node> createSphere(vsg::vec3 center, float radius);
vsg::ref_ptr<vsg::Node> createQuad(vsg::vec3 center, vsg::vec3 normal, vsg::vec3 up, float width, float height);

vsg::ref_ptr<vsg::Data> loadEXRTexture(const std::string& path);

template<typename T>
vsg::ref_ptr<vsg::Array<T>> concatArray(std::vector<vsg::ref_ptr<vsg::Array<T>>> arrays)
{
  size_t totalValueCount = 0;
  for (auto& arr : arrays) {
    totalValueCount += arr->valueCount();
  }
  
  auto newArray = vsg::Array<T>::create(totalValueCount);
  size_t pos = 0;
  for (auto& arr : arrays) {
    for (auto& elem : *arr) {
      newArray->at(pos) = elem;
      ++pos;
    }
  }

  return newArray;
}