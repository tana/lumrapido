#pragma once

#include <vector>
#include <vsg/core/Array.h>

float scrambledRadicalInverse(int num, int base, const std::vector<int>& perm);

// Generate scrambled Hammersley sequence of given number of dimensions and samples.
// Additionally, many replications using different permutations can be generated.
// See: T. Kollig and A. Keller, "Efficient Bidirectional Path Tracing by Randomized Quasi-Monte Carlo Integration," in Monte Carlo and Quasi-Monte Carlo Methods 2000, KT. Fang, H. Niederreiter, F.J. Hickernell, Eds. Springer, Berlin, Heidelberg. 2002. pp. 290-305.
void generateScrambledHammersley(int numDims, int numSamples, int numReps, vsg::ref_ptr<vsg::floatArray> arr);
