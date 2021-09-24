#include "utils.h"

#include <cmath>
#include <vsg/maths/transform.h>

vsg::ref_ptr<vsg::Node> createSphere(vsg::vec3 center, float radius)
{
  int numTheta = 32;
  int numPhi = 64;

  int numVertices = (numTheta + 1) * numPhi;
  int numIndices = 6 * numTheta * numPhi;

  auto vertices = vsg::vec3Array::create(numVertices);
  auto normals = vsg::vec3Array::create(numVertices);
  auto texCoords = vsg::vec2Array::create(numVertices);
  auto indices = vsg::ushortArray::create(numIndices);

  for (int i = 0; i <= numTheta; ++i) {
    float v = float(i) / numTheta;
    float theta = vsg::PIf * v;

    for (int j = 0; j < numPhi; ++j) {
      float u = float(j) / numPhi;
      float phi = 2 * vsg::PIf * u;

      auto normal = vsg::vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
      
      int vertId = numPhi * i + j;
      vertices->set(vertId, normal * radius + center);
      normals->set(vertId, normal);
      texCoords->set(vertId, vsg::vec2(u, v));

      if (i < numTheta) {
        int nextI = i + 1;
        int nextJ = (j + 1) % numPhi;
        indices->set(6 * vertId, numPhi * i + j);
        indices->set(6 * vertId + 1, numPhi * nextI + j);
        indices->set(6 * vertId + 2, numPhi * nextI + nextJ);
        indices->set(6 * vertId + 3, numPhi * i + j);
        indices->set(6 * vertId + 4, numPhi * nextI + nextJ);
        indices->set(6 * vertId + 5, numPhi * i + nextJ);
      }
    }
  }

  auto geom = vsg::Geometry::create();
  geom->arrays = vsg::DataList{ vertices, normals, texCoords };
  geom->indices = indices;

  return geom;
}

vsg::ref_ptr<vsg::Node> createQuad(vsg::vec3 center, vsg::vec3 normal, vsg::vec3 up, float width, float height)
{
  auto builder = vsg::Builder::create();

  vsg::GeometryInfo geomInfo;
  geomInfo.dx = vsg::vec3(width, 0.0f, 0.0f);
  geomInfo.dy = vsg::vec3(0.0f, height, 0.0f);
  geomInfo.dz = vsg::vec3(0.0f, 0.0f, 1.0f);
  geomInfo.transform = vsg::lookAt(center, center + normal, up);

  return builder->createQuad(geomInfo);
}