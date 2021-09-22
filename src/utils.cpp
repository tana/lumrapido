#include "utils.h"

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
