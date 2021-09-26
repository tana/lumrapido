// Constants, struct definitions, and utility functions for shaders

const float EPSILON = 0.0001;

const int RT_MATERIAL_LAMBERT = 0;
const int RT_MATERIAL_METAL = 1;
const int RT_MATERIAL_DIELECTRIC = 2;

struct Material
{
  int type;
  vec3 color;
  float fuzz;
  float ior;
};

struct ObjectInfo
{
  uint indexOffset;
  uint vertexOffset;
  Material material;
};

// State for Xorshift random number generator
struct RandomState
{
  uint x, y, z, w;
};

struct RayPayload
{
  vec3 color;
  bool traceNextRay;
  vec3 nextOrigin;
  vec3 nextDirection;
  RandomState randomState;
};

struct RayTracingUniform
{
  mat4 invViewMat; // Inverse of view matrix (i.e. transform camera coordinate to world coordinate)
  mat4 invProjectionMat; // Inverse of projection matrix (i.e. transform normalized device coordinate into camera coordinate)
  uint samplesPerPixel; // How many rays are sampled to render one pixel
};

// Pseudo-random number using Xorshift (xor128)
// G. Marsaglia, "Xorshift RNGs", Journal of Statistical Software, vol. 8, no. 14, pp. 1-6, 2003, doi: 10.18637/jss.v008.i14
uint random(inout RandomState state)
{
  uint t = (state.x ^ (state.x << 11));
  state.x = state.y;
  state.y = state.z;
  state.z = state.w;
  return (state.w = (state.w ^ (state.w >> 19)) ^ (t ^ (t >> 8)));
}

// Initialize the RNG.
void initRandom(out RandomState state, uint x)
{
  state.x = x;
  // Values of y, z, w are same as Marsaglia's paper.
  state.y = 362436069;
  state.z = 521288629;
  state.w = 88675123;
}

float randomFloat(inout RandomState state, float minimum, float maximum)
{
  return minimum + (float(random(state)) / 4294967296.0) * (maximum - minimum);
}

bool nearZero(in vec3 v)
{
  return (abs(v.x) < EPSILON) && (abs(v.y) < EPSILON) && (abs(v.z) < EPSILON);
}
