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
vec3 sampleGGX(in float rand1, in float rand2, in vec3 viewVec, in vec3 normal, in float roughness)
{
  float alpha = roughness * roughness;

  float theta = atan(alpha * sqrt(rand1 / (1.0 - rand1)));
  float phi = 2.0 * PI * rand2;

  vec3 tangent = normalize(cross(normal, viewVec));
  vec3 binormal = cross(tangent, normal);
  
  float sinTheta = sin(theta);  // Avoid calculating sin(theta) twice
  return tangent * (sinTheta * cos(phi)) + normal * cos(theta) + binormal * (sinTheta * sin(phi));
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

// Sample a random point on unit hemisphere from p(x,y,z)=cosƒÆ
// See: https://shikihuiku.wordpress.com/2016/06/14/%E3%83%AC%E3%83%B3%E3%83%80%E3%83%AA%E3%83%B3%E3%82%B0%E3%81%AB%E3%81%8A%E3%81%91%E3%82%8Bimportancesampling%E3%81%AE%E5%9F%BA%E7%A4%8E/ (in Japanese)
// (Note: In the above article, it seems there is a mistake in equation of P(ƒÓ|ƒÆ) and ƒÓ. However sample code is correct)
vec3 sampleHemisphereCosine(in float rand1, in float rand2, in vec3 viewVec, in vec3 normal)
{
  // Sample in spherical coordinate
  float theta = asin(sqrt(rand1));
  float phi = 2 * PI * rand2;
  // Convert into Cartesian coordinate
  vec3 tangent = normalize(cross(normal, viewVec));
  vec3 binormal = cross(tangent, normal);
  float sinTheta = sin(theta);  // Avoid calculating sin(theta) twice
  return tangent * (sinTheta * cos(phi)) + normal * cos(theta) + binormal * (sinTheta * sin(phi));
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

  // Calculate metallic and roughness
  float metallic = material.metallic;
  float roughness = material.roughness;
  if (material.metallicRoughnessTextureIdx >= 0) {  // If the object has a metallic/roughness texture
    vec4 metallicRoughness = texture(textures[material.metallicRoughnessTextureIdx], texCoord);
    metallic *= metallicRoughness.b;
    roughness *= metallicRoughness.g;
  }

  // Point of intersection
  vec3 hitPoint = gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT;
  // Normalized direction of incoming ray
  vec3 unitRayDir = normalize(gl_WorldRayDirectionEXT);

  vec3 lightVec;  // Vector of incoming light (from point on the point of intersection to light source or another object)

  // Vector towards the camera
  vec3 viewVec = -unitRayDir;
  float dotNV = dot(normal, viewVec);

  // Probability of sampling specular reflection
  float specularProb = mix(0.3, 1.0, metallic);

  if (payload.random[0] < specularProb) { // Specular
    // Fresnel factor F(v,h)
    vec3 f0 = mix(vec3(0.04), color, metallic);

    // Sample a halfway vector from GGX NDF
    vec3 halfwayVec = sampleGGX(payload.random[1], payload.random[2], viewVec, normal, roughness);
    // Calculate light vector from halfway vector
    lightVec = reflect(-viewVec, halfwayVec);

    // Geometric attenuation term G(l,v,h)
    float geometricAttenuation = geometricAttenuationSchlick(lightVec, viewVec, normal, roughness);

    if (dot(lightVec, normal) <= 0.0 || dot(lightVec, halfwayVec) <= 0.0) { // Not reflecting
      payload.color = vec3(0.0);
      payload.traceNextRay = false;
      return;
    }

    vec3 fresnel = fresnelSchlick(dot(viewVec, halfwayVec), f0);
    float dotVH = dot(viewVec, halfwayVec);
    float dotNH = dot(normal, halfwayVec);

    payload.color *= fresnel * geometricAttenuation * dotVH / (dotNV * dotNH) / specularProb;
  } else {  // Diffuse
    lightVec = sampleHemisphereCosine(payload.random[1], payload.random[2], viewVec, normal);

    // ((1 - metallic) * (color / PI) * dotNV) / (dotNV / PI) / (1 - specularProb)
    payload.color *=  (1 - metallic) * color / (1 - specularProb);
  }

  // Trace next ray
  payload.nextOrigin = hitPoint;
  payload.nextDirection = lightVec;
  payload.traceNextRay = true;
}