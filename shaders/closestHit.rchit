#version 460
#extension GL_EXT_ray_tracing : enable
// For uint16_t type
#extension GL_EXT_shader_16bit_storage : enable
// For layout qualifier "scalar", which aligns vec3 as vec3, not as vec4
#extension GL_EXT_scalar_block_layout : enable

#include "common.glsl"

// Closest hit shader
// Implementation of physically based rendering (RT_MATERIAL_PBR) is based on the following papers:
//  B. Burley, "Physically Based Shading at Disney", in SIGGRAPH 2012 Course: Practical Physically Based Shading in Film and Game production, 2012, https://blog.selfshadow.com/publications/s2012-shading-course/burley/s2012_pbs_disney_brdf_notes_v3.pdf
//  B. Karis, "Real Shading in Unreal Engine 4", in SIGGRAPH 2013 Course: Physically Based Shading in Theory and Practice, 2013, https://cdn2.unrealengine.com/Resources/files/2013SiggraphPresentationsNotes-26915738.pdf
//  C. Schlick, "An Inexpensive BRDF Model for Physically-based Rendering", in Computer Graphics Forum, vol. 13, no. 3, 1994, pp. 233-246.
//  R. Guy and M. Agopian, "Physically Based Rendering in Filament", https://google.github.io/filament/Filament.md.html
//  shocker, "Disney Principled BRDF - Computer Graphics - memoRANDOM", https://rayspace.xyz/CG/contents/Disney_principled_BRDF/

layout(binding = 3, scalar) readonly buffer ObjectInfos {
  ObjectInfo objectInfos[];
};
layout(binding = 4, scalar) readonly buffer Indices {
  uint16_t indices[];
};
layout(binding = 5, scalar) readonly buffer Vertices {
  vec3 vertices[];
};
layout(binding = 6, scalar) readonly buffer Normals {
  vec3 normals[];
};
layout(binding = 7, scalar) readonly buffer TexCoords {
  vec2 texCoords[];
};
layout(binding = 8, scalar) readonly buffer Tangents {
  vec4 tangents[];
};

layout(binding = 10) uniform sampler2D textures[MAX_NUM_TEXTURES];

layout(location = 0) rayPayloadInEXT RayPayload payload;

hitAttributeEXT vec2 uv;  // Barycentric coordinate of the hit position inside a triangle

// Sample a random point in a sphere with radius of 1
vec3 randomPointInUnitSphere(inout RandomState state)
{
  // Rejection sampling
  while (true) {
    // Random point in a cube
    vec3 point = vec3(randomFloat(state, -1.0, 1.0), randomFloat(state, -1.0, 1.0), randomFloat(state, -1.0, 1.0));
    // Accept if the point is in a unit sphere
    if (dot(point, point) < 1.0) return point;
  }
}

// Calculate Fresnel term using Schlick's approximation
// cosTheta = dot(vectorToEye, normal)
vec3 fresnelSchlick(in float cosTheta, in vec3 f0)
{
  return f0 + (1 - f0) * pow(1 - cosTheta, 5);
}

// Sample a halfway vector from GGX/Trowbridge-Reitz normal distribution
// Based on the following articles:
//  B. Walter et al., "Microfacet Models for Refraction through Rough Surfaces," in Proceedings of the 18th Eurographics conference on Rendering Techniques (EGSR'07), 2007, pp. 195-206.
//  "Sampling microfacet BRDF," https://agraphicsguy.wordpress.com/2015/11/01/sampling-microfacet-brdf/
//  "Importance Sampling techniques for GGX with Smith Masking-Shadowing: Part 1," https://schuttejoe.github.io/post/ggximportancesamplingpart1/
vec3 sampleGGX(inout RandomState state, in vec3 viewVec, in vec3 normal, in float roughness)
{
  float alpha = roughness * roughness;

  float rand1 = randomFloat(state, 0.0, 1.0);
  float rand2 = randomFloat(state, 0.0, 1.0);

  float theta = atan(alpha * sqrt(rand1 / (1.0 - rand1)));
  float phi = 2.0 * PI * rand2;

  vec3 tangent = normalize(cross(normal, viewVec));
  vec3 binormal = cross(tangent, normal);
  
  return tangent * sin(theta) * cos(phi) + normal * cos(theta) + binormal * sin(theta) * sin(phi);
}

// Schlick GGX Geometric attenuation term G(l,v,h)
float geometricAttenuationSchlick(in vec3 lightVec, in vec3 viewVec, in vec3 normal, in float roughness)
{
  float dotNL = dot(normal, lightVec);
  float dotNV = dot(normal, viewVec);
  float alpha = roughness * roughness;
  float k = alpha / 2.0;
  float g1L = dotNL / (dotNL * (1.0 - k) + k);
  float g1V = dotNV / (dotNV * (1.0 - k) + k);
  return g1L * g1V;
}

void main()
{
  uint indexOffset = objectInfos[gl_InstanceID].indexOffset;
  uint vertexOffset = objectInfos[gl_InstanceID].vertexOffset;

  uint idx0 = uint(indices[indexOffset + 3 * gl_PrimitiveID]);
  uint idx1 = uint(indices[indexOffset + 3 * gl_PrimitiveID + 1]);
  uint idx2 = uint(indices[indexOffset + 3 * gl_PrimitiveID + 2]);

  // Normal vectors of each vertices
  vec3 normal0 = normals[vertexOffset + idx0];
  vec3 normal1 = normals[vertexOffset + idx1];
  vec3 normal2 = normals[vertexOffset + idx2];
  // Texture coordinates of each vertices
  vec2 texCoord0 = texCoords[vertexOffset + idx0];
  vec2 texCoord1 = texCoords[vertexOffset + idx1];
  vec2 texCoord2 = texCoords[vertexOffset + idx2];
  // Tangent vectors of each vertices
  vec4 tangent0 = tangents[vertexOffset + idx0];
  vec4 tangent1 = tangents[vertexOffset + idx1];
  vec4 tangent2 = tangents[vertexOffset + idx2];

  Material material = objectInfos[gl_InstanceID].material;

  bool isFront = gl_HitKindEXT == gl_HitKindFrontFacingTriangleEXT;
  
  // Normal vector in object coordinate (interpolated from barycentric coords)
  vec3 normalObj = interpolate(normal0, normal1, normal2, uv);
  // Normal vector in world coordinate
  vec3 normal = normalize(gl_ObjectToWorldEXT * vec4(normalObj, 0.0));
  // Reverse normal vector if the surface is facing back
  if (!isFront) {
    normal = -normal;
  }

  // Interpolate texture coord
  vec2 texCoord = interpolate(texCoord0, texCoord1, texCoord2, uv);

  // Interpolate tangent (assuming all tangent vectors of a triangle have same w component)
  vec4 tangent = interpolate(tangent0, tangent1, tangent2, uv);

  // Calculate base color
  vec3 color = material.color;
  if (material.colorTextureIdx >= 0) {  // If the object has a color texture
    color *= texture(textures[material.colorTextureIdx], texCoord).xyz;
  }

  // Point of intersection
  vec3 hitPoint = gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT;
  // Normalized direction of incoming ray
  vec3 unitRayDir = normalize(gl_WorldRayDirectionEXT);

  if (material.type == RT_MATERIAL_LAMBERT) {
    // Randomly generate a new ray
    payload.nextDirection = normal + normalize(randomPointInUnitSphere(payload.randomState));
    if (nearZero(payload.nextDirection)) {
      payload.nextDirection = normal;
    }
    payload.nextOrigin = hitPoint;
    // Signal the caller (ray generation shader) to trace next ray, instead of recursively tracing
    payload.color *= color;
    payload.traceNextRay = true;
  } else if (material.type == RT_MATERIAL_METAL) {
    vec3 reflectedDir = reflect(unitRayDir, normal) + material.fuzz * randomPointInUnitSphere(payload.randomState);
    if (dot(reflectedDir, normal) > 0) {
      payload.nextDirection = reflectedDir;
      payload.nextOrigin = hitPoint;
      payload.color *= color;
      payload.traceNextRay = true;
    } else {  // When the reflected ray goes inside the object
      payload.color = vec3(0.0);
      payload.traceNextRay = false;
    }
  } else if (material.type == RT_MATERIAL_DIELECTRIC) {
    float eta = isFront ? (1 / material.ior) : material.ior;
    // sqrt(f0) = (n1-n2)/(n1+n2) = (eta-1)/(eta+1)
    float sqrtF0 = (eta - 1) / (eta + 1);
    float f0 = sqrtF0 * sqrtF0;
    vec3 refractedDir = refract(unitRayDir, normal, eta);
    float fresnel = fresnelSchlick(dot(-unitRayDir, normal), vec3(f0)).x;
    if (nearZero(refractedDir) || fresnel > randomFloat(payload.randomState, 0.0, 1.0)) {
      // Total internal reflection or Fresnel
      refractedDir = reflect(unitRayDir, normal);
    }
    payload.nextDirection = refractedDir;
    payload.nextOrigin = hitPoint;
    payload.traceNextRay = true;
  } else if (material.type == RT_MATERIAL_PBR) {
    vec3 lightVec;  // Vector of incoming light (from point on the point of intersection to light source or another object)

    // Vector towards the camera
    vec3 viewVec = -unitRayDir;
    float dotNV = dot(normal, viewVec);

    if (randomFloat(payload.randomState, 0.0, 1.0) < 0.5) { // Specular
      // Fresnel factor F(v,h)
      vec3 f0 = mix(vec3(0.04), color, material.metallic);

      // Sample a halfway vector from GGX NDF
      vec3 halfwayVec = sampleGGX(payload.randomState, viewVec, normal, material.roughness);
      // Calculate light vector from halfway vector
      lightVec = reflect(-viewVec, halfwayVec);

      // Geometric attenuation term G(l,v,h)
      float geometricAttenuation = geometricAttenuationSchlick(lightVec, viewVec, normal, material.roughness);

      if (dot(lightVec, normal) <= 0.0 || dot(lightVec, halfwayVec) <= 0.0) { // Not reflecting
        payload.color = vec3(0.0);
        payload.traceNextRay = false;
        return;
      }

      vec3 fresnel = fresnelSchlick(dot(viewVec, halfwayVec), f0);
      float dotVH = dot(viewVec, halfwayVec);
      float dotNH = dot(normal, halfwayVec);

      payload.color *= 2.0 * fresnel * geometricAttenuation * dotVH / (dotNV * dotNH);
    } else {  // Diffuse
      lightVec = normal + randomPointInUnitSphere(payload.randomState);
      if (nearZero(lightVec)) {
        lightVec = normal;
      }

      payload.color *= 2.0 * (1.0 - material.metallic) * color / PI;
    }

    // Trace next ray
    payload.nextOrigin = hitPoint;
    payload.nextDirection = lightVec;
    payload.traceNextRay = true;
  } else {  // Unknown material
    payload.color *= vec3(0.0);
    payload.traceNextRay = false;
  }
}