#include "RayTracingMaterialGroup.h"

#include "RayTracingVisitor.h"

RayTracingMaterialGroup::RayTracingMaterialGroup(RayTracingMaterial material)
  : material(material)
{
}

void RayTracingMaterialGroup::accept(vsg::Visitor& visitor)
{
  // Use dynamic cast to handle custom node type
  // See https://stackoverflow.com/questions/27837228/scene-graph-update-callback-design
  
  RayTracingVisitor* rtVisitor = dynamic_cast<RayTracingVisitor*>(&visitor);
  if (rtVisitor) {
    rtVisitor->apply(*this);
  } else {
    visitor.apply(*this);
  }
}
