#include "utils.h"

#include <vsg/maths/transform.h>

vsg::ref_ptr<vsg::Node> createSphere(vsg::vec3 center, float radius)
{
  auto builder = vsg::Builder::create();

  vsg::GeometryInfo geomInfo;
  geomInfo.position = center;
  geomInfo.dx = vsg::vec3(2.0f * radius, 0.0f, 0.0f);
  geomInfo.dy = vsg::vec3(0.0f, 2.0f * radius, 0.0f);
  geomInfo.dz = vsg::vec3(0.0f, 0.0f, 2.0f * radius);
  geomInfo.color = vsg::vec4(1.0f, 1.0f, 1.0f, 1.0f);

  return builder->createSphere(geomInfo);
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