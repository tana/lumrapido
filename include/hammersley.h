#pragma once

#include <vector>
#include <vsg/core/Array.h>

float scrambledRadicalInverse(int num, int base, const std::vector<int>& perm);

void generateScrambledHammersley(int numDims, int numSamples, vsg::ref_ptr<vsg::floatArray> arr);
