// Constants, struct definitions, and utility functions for shaders

const float EPSILON = 0.0001;
const float PI = 3.14159265359;

const int MAX_NUM_TEXTURES = 32;

struct Material
{
  vec3 color;
  float metallic;
  float roughness;
  int colorTextureIdx;
  int metallicRoughnessTextureIdx;
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
  float random[3];  // [0,1) random numbers used in closest hit shader
};

struct RayTracingUniform
{
  mat4 invViewMat; // Inverse of view matrix (i.e. transform camera coordinate to world coordinate)
  mat4 invProjectionMat; // Inverse of projection matrix (i.e. transform normalized device coordinate into camera coordinate)
  uint samplesPerPixel; // How many rays are sampled to render one pixel
  int envMapTextureIdx; // Index of environment map texture (negative when there is no environment map)
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

// Interpolation using barycentric coordinates
vec2 interpolate(in vec2 v0, in vec2 v1, in vec2 v2, in vec2 uv)
{
  return (1.0 - uv.x - uv.y) * v0 + uv.x * v1 + uv.y * v2;
}

vec3 interpolate(in vec3 v0, in vec3 v1, in vec3 v2, in vec2 uv)
{
  return (1.0 - uv.x - uv.y) * v0 + uv.x * v1 + uv.y * v2;
}

vec4 interpolate(in vec4 v0, in vec4 v1, in vec4 v2, in vec2 uv)
{
  return (1.0 - uv.x - uv.y) * v0 + uv.x * v1 + uv.y * v2;
}

// atan(y, x) with handling of x==0
// Because GLSL atan(y, x) is undefined when x==0.
// See:
//  https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/atan.xhtml
//  https://qiita.com/7CIT/items/ad76cfa6771641951d31
float safeAtan(in float y, in float x)
{
  return (abs(x) > EPSILON) ? atan(y, x) : (sign(y) * PI / 2.0);
}
