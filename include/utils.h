#pragma once

#include <vsg/maths/vec3.h>
#include <vsg/nodes/Node.h>
#include <vsg/utils/Builder.h>

vsg::ref_ptr<vsg::Node> createSphere(vsg::vec3 center, float radius);