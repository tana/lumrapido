#pragma once

#include <cassert>
#include <vector>
#include <vsg/core/Object.h>
#include <vsg/core/Inherit.h>
#include <vsg/core/Array.h>
#include <vsg/core/Array2D.h>
#include "utils.h"

// Precomputed data for importance sampling of environment map
// Reference:
//  Project 3-2, Part 3: Environment Map Lights https://cs184.eecs.berkeley.edu/sp18/article/25
//  M. Phare and G. Humphreys, "Monte Carlo Rendering with Natural Illumination," in University of Virginia Dept. of Computer Science Tech Report, 2004. https://doi.org/10.18130/V3C484
class EnvMapSamplingData : public vsg::Inherit<vsg::Object, EnvMapSamplingData>
{
public:
  EnvMapSamplingData(vsg::ref_ptr<vsg::vec4Array2D> envMap);

  // Probability density function p(u,v) where u is vertical and v is horizontal position
  // This PDF is piecewise constant.
  // pdf->at(j, i) stores value of p(u, v) where i=floor(u), j=floor(v) .
  // (Note: vsg::Array2D#at function uses at(column, row) order)
  vsg::ref_ptr<vsg::floatArray2D> pdf;

  // Cumulative distribution function P(u) for marginal distribution of vertical position p(u)
  // This CDF is piecewise linear because the PDF is piecewise constant.
  // marginalCDF->at(i) stores value of integral çp(u)du from u=0 to u=i+1 .
  // i.e. marginalCDF->at(0) = p(i) and marginalCDF->at(i) = marginalCDF->at(i-1) + p(i) .
  vsg::ref_ptr<vsg::floatArray> marginalCDF;

  // CDF P(v|u) for conditional distribution of horizontal position given vertical position p(v|u)
  // Each row of conditionalCDF stores CDF of conditional distribution for the same row of the PDF.
  vsg::ref_ptr<vsg::floatArray2D> conditionalCDF;

private:
  static vsg::ref_ptr<vsg::floatArray2D> generatePDF(vsg::ref_ptr<vsg::vec4Array2D> envMap);
  static std::vector<float> generateCDF1D(const std::vector<float>& pdf);
};
