#include "hammersley.h"

#include <cstdlib>
#include <array>
#include <algorithm>
#include <numeric>
#include <random>

// Generation of Hammersley sequence for Quasi-Monte Carlo sampling
// Reference:
//  M. Pharr et al., "The Halton Sampler", in "Physically Based Rendering: From Theory To Implementation"
//      https://www.pbr-book.org/3ed-2018/Sampling_and_Reconstruction/The_Halton_Sampler
// (However we use Hammersley sequence instead of Halton sequence)

const std::array<int, 100> PRIMES = {
  2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71,
  73, 79, 83, 89, 97, 101, 103, 107, 109, 113, 127, 131, 137, 139, 149, 151,
  157, 163, 167, 173, 179, 181, 191, 193, 197, 199, 211, 223, 227, 229, 233,
  239, 241, 251, 257, 263, 269, 271, 277, 281, 283, 293, 307, 311, 313, 317,
  331, 337, 347, 349, 353, 359, 367, 373, 379, 383, 389, 397, 401, 409, 419,
  421, 431, 433, 439, 443, 449, 457, 461, 463, 467, 479, 487, 491, 499, 503,
  509, 521, 523, 541
};

float scrambledRadicalInverse(int num, int base, const std::vector<int>& perm)
{
  float result = 0.0f;
  float invBasePow = 1.0f / float(base);

  while (num > 0) {
    div_t quotRem = div(num, base);
    int digit = quotRem.rem;
    result += perm[digit] * invBasePow;
    num = quotRem.quot;
    invBasePow /= base;
  }

  return result;
}

void generateScrambledHammersley(int numDims, int numSamples, int numReps, vsg::ref_ptr<vsg::floatArray> arr)
{
  // Initialize RNG for permutation generation
  std::mt19937 rng(12345);  // Seed is fixed

  for (size_t rep = 0; rep < size_t(numReps); ++rep) {
    std::vector<std::vector<int>> permutations; // Permutations for each base
    // Generate permutations for scrambling
    for (size_t i = 0; i < size_t(numDims) - 1; ++i) {
      int base = PRIMES[i];
      std::vector<int> perm(base);
      std::iota(perm.begin(), perm.end(), 0); // Generate numbers from 0 to base-1

      std::shuffle(perm.begin(), perm.end(), rng);  // Shuffle

      permutations.emplace_back(perm);
    }

    for (size_t i = 0; i < numSamples; ++i) {
      // First dimension of a Hammersley point is calculated by special formula
      arr->at((numSamples * numDims) * rep + numDims * i) = float(i) / numSamples;
      // Other dimensions are calculated by (scrambled) radical inverse
      for (size_t j = 1; j < numDims; ++j) {
        arr->at((numSamples * numDims) * rep + numDims * i + j) = scrambledRadicalInverse(int(i), PRIMES[j - 1], permutations[j - 1]);
      }
    }
  }
}
