#version 330 core

layout(location = 0) out vec4 out_color;

in DATA
{
	vec3 position;
	mat3 tbn;
	vec2 uv;
	vec4 shadow_coord;
} fs_in;

struct Camera
{
	vec3 position; //!< world space camera position
};

struct Light
{
	vec3 color;
	vec3 direction;
};

uniform Camera u_camera;
uniform Light u_light;
uniform float u_shadow_scale; // determines how much shadow we want [0; 1]

// PBR Inputs
uniform samplerCube u_diffuse_env_sampler;
uniform samplerCube u_specular_env_sampler;
uniform sampler2D u_preintegrated_fg_sampler;

// PBR Map Inputs
uniform sampler2D u_albedo_sampler;
uniform sampler2D u_normal_sampler;
uniform sampler2D u_roughness_sampler;
uniform sampler2D u_metal_sampler;

uniform sampler2D u_shadow_sampler;

// Encapsulate the various inputs used by the various functions in the shading equation
// We store values in this struct to simplify the integration of alternative implementations
// of the shading terms, outlined in the Readme.MD Appendix.
struct PBRInfo
{
	float NdotL;                  // cos angle between normal and light direction
	float NdotV;                  // cos angle between normal and view direction
	float NdotH;                  // cos angle between normal and half vector
	float LdotH;                  // cos angle between light direction and half vector
	float VdotH;                  // cos angle between view direction and half vector
	float perceptual_roughness;    // roughness value, as authored by the model creator (input to shader)
	float metalness;              // metallic value at the surface
	vec3 reflectance0;            // full reflectance color (normal incidence angle)
	vec3 reflectance90;           // reflectance color at grazing angle
	float alpha_roughness;         // roughness mapped to a more linear change in the roughness (proposed by [2])
	vec3 diffuse_color;            // color contribution from diffuse lighting
	vec3 specular_color;           // color contribution from specular lighting
};

const float PI = 3.141592653589793;
#define GAMMA 2.2

// Used for shadows
vec2 poissonDisk[16] = vec2[](
	vec2(-0.94201624, -0.39906216),
	vec2(0.94558609, -0.76890725),
	vec2(-0.094184101, -0.92938870),
	vec2(0.34495938, 0.29387760),
	vec2(-0.91588581, 0.45771432),
	vec2(-0.81544232, -0.87912464),
	vec2(-0.38277543, 0.27676845),
	vec2(0.97484398, 0.75648379),
	vec2(0.44323325, -0.97511554),
	vec2(0.53742981, -0.47373420),
	vec2(-0.26496911, -0.41893023),
	vec2(0.79197514, 0.19090188),
	vec2(-0.24188840, 0.99706507),
	vec2(-0.81409955, 0.91437590),
	vec2(0.19984126, 0.78641367),
	vec2(0.14383161, -0.14100790)
	);
float random(vec3 seed, int i)
{
 vec4 seed4 = vec4(seed, i);
 float dot_product = dot(seed4, vec4(12.9898, 78.233, 45.164, 94.673));
 return fract(sin(dot_product) * 43758.5453);
}

vec4 SrgbToLinear(vec4 srgb_in)
{
	vec3 linear_out = pow(srgb_in.xyz, vec3(GAMMA));
	return vec4(linear_out, srgb_in.w);
}

// Find the normal for this fragment, pulling either from a predefined normal map
// or from the interpolated mesh normal and tangent attributes.
vec3 GetNormal()
{
	// Retrieve the tangent space matrix
	mat3 tbn = fs_in.tbn;
	vec3 n = texture(u_normal_sampler, fs_in.uv).rgb;
	n = normalize(tbn * (2.0 * n - 1.0));
	return n;
}

// Calculation of the lighting contribution from an optional Image Based Light source.
// Precomputed Environment Maps are required uniform inputs and are computed as outlined in [1].
// See our README.md on Environment Maps [3] for additional discussion.
vec3 GetIBLContribution(PBRInfo pbr_inputs, vec3 n, vec3 reflection)
{
	float mipCount = 4.0; // resolution of 512x512
	float lod = (pbr_inputs.perceptual_roughness * mipCount);
	// retrieve a scale and bias to F0. See [1], Figure 3
	vec3 brdf = SrgbToLinear(texture(u_preintegrated_fg_sampler, vec2(pbr_inputs.NdotV, 1.0 - pbr_inputs.perceptual_roughness))).rgb;
	vec3 diffuseLight = SrgbToLinear(texture(u_diffuse_env_sampler, n)).rgb;

	vec3 specularLight = SrgbToLinear(textureLod(u_specular_env_sampler, reflection, lod)).rgb;

	vec3 diffuse = diffuseLight * pbr_inputs.diffuse_color;
	vec3 specular = specularLight * (pbr_inputs.specular_color * brdf.x + brdf.y);

	return diffuse + specular;
}

// Basic Lambertian diffuse
// Implementation from Lambert's Photometria https://archive.org/details/lambertsphotome00lambgoog
// See also [1], Equation 1
vec3 Diffuse(PBRInfo pbr_inputs)
{
	return pbr_inputs.diffuse_color / PI;
}

// The following equation models the Fresnel reflectance term of the spec equation (aka F())
// Implementation of fresnel from [4], Equation 15
vec3 SpecularReflection(PBRInfo pbr_inputs)
{
	return pbr_inputs.reflectance0 + (pbr_inputs.reflectance90 - pbr_inputs.reflectance0) * pow(clamp(1.0 - pbr_inputs.VdotH, 0.0, 1.0), 5.0);
}

// This calculates the specular geometric attenuation (aka G()),
// where rougher material will reflect less light back to the viewer.
// This implementation is based on [1] Equation 4, and we adopt their modifications to
// alpha_roughness as input as originally proposed in [2].
float GeometricOcclusion(PBRInfo pbr_inputs)
{
	float NdotL = pbr_inputs.NdotL;
	float NdotV = pbr_inputs.NdotV;
	float r = pbr_inputs.alpha_roughness;

	float attenuationL = 2.0 * NdotL / (NdotL + sqrt(r * r + (1.0 - r * r) * (NdotL * NdotL)));
	float attenuationV = 2.0 * NdotV / (NdotV + sqrt(r * r + (1.0 - r * r) * (NdotV * NdotV)));
	return attenuationL * attenuationV;
}

// The following equation(s) model the distribution of microfacet normals across the area being drawn (aka D())
// Implementation from "Average Irregularity Representation of a Roughened Surface for Ray Reflection" by T. S. Trowbridge, and K. P. Reitz
// Follows the distribution function recommended in the SIGGRAPH 2013 course notes from EPIC Games [1], Equation 3.
float MicrofacetDistribution(PBRInfo pbr_inputs)
{
	float roughnessSq = pbr_inputs.alpha_roughness * pbr_inputs.alpha_roughness;
	float f = (pbr_inputs.NdotH * roughnessSq - pbr_inputs.NdotH) * pbr_inputs.NdotH + 1.0;
	return roughnessSq / (PI * f * f);
}

// Compute visibility for shadow factor using Chebyshev's equation
float GetVisibility()
{
	vec4 shadow_coord_post_w = fs_in.shadow_coord / fs_in.shadow_coord.w;
	// Check if we outside the shadow map
	if (fs_in.shadow_coord.w <= 0.0 ||
		(shadow_coord_post_w.x <  0.0 || shadow_coord_post_w.y <  0.0) ||
		(shadow_coord_post_w.x >= 1.0 || shadow_coord_post_w.y >= 1.0))
		return 1.0;
	// We retrive the two moments previously stored (depth and depth*depth)
	vec2 moments = texture(u_shadow_sampler, shadow_coord_post_w.xy).rg;
	
	// Surface is fully lit. as the current fragment is before the light occluder
	if (shadow_coord_post_w.z <= moments.x)
		return 1.0;

	// The fragment is either in shadow or penumbra. We now use Chebyshev's upper bound to check
	// How likely this pixel is to be lit (p_max)
	float variance = moments.y - (moments.x * moments.x);
	variance = max(variance, 0.00002);

	float d = shadow_coord_post_w.z - moments.x;
	float p_max = variance / (variance + d*d);

	return p_max;
}

void main()
{
	// Roughness
	float perceptual_roughness = texture(u_roughness_sampler, fs_in.uv).r;
	perceptual_roughness = clamp(perceptual_roughness, 0.04, 1.0);

	// Metallic
	float metallic = texture(u_metal_sampler, fs_in.uv).r;
	metallic = clamp(metallic, 0.0, 1.0);

	// Roughness is authored as perceptual roughness; as is convention,
	// convert to material roughness by squaring the perceptual roughness [2].
	float alpha_roughness = perceptual_roughness * perceptual_roughness;

	// The albedo may be defined from a base texture or a flat color
	vec4 base_color = SrgbToLinear(texture(u_albedo_sampler, fs_in.uv));

	vec3 f0 = vec3(0.04);
	vec3 diffuse_color = base_color.rgb * (vec3(1.0) - f0);
	diffuse_color *= 1.0 - metallic;
	vec3 specular_color = mix(f0, base_color.rgb, metallic);

	// Compute reflectance.
	float reflectance = max(max(specular_color.r, specular_color.g), specular_color.b);

	// For typical incident reflectance range (between 4% to 100%) set the grazing reflectance to 100% for typical fresnel effect.
	// For very low reflectance range on highly diffuse objects (below 4%), incrementally reduce grazing reflecance to 0%.
	float reflectance90 = clamp(reflectance * 25.0, 0.0, 1.0);
	vec3 specularEnvironmentR0 = specular_color.rgb;
	vec3 specularEnvironmentR90 = vec3(1.0, 1.0, 1.0) * reflectance90;

	vec3 n = GetNormal();									// normal at surface point
	vec3 v = normalize(u_camera.position - fs_in.position); // vector from surface point to camera
	vec3 l = normalize(u_light.direction);					// vector from surface point to light
	vec3 h = normalize(l + v);								// half vector between both l and v
	vec3 reflection = normalize(reflect(-v, n));

	float NdotL = clamp(dot(n, l), 0.001, 1.0);
	float NdotV = abs(dot(n, v)) + 0.001;
	float NdotH = clamp(dot(n, h), 0.0, 1.0);
	float LdotH = clamp(dot(l, h), 0.0, 1.0);
	float VdotH = clamp(dot(v, h), 0.0, 1.0);

	PBRInfo pbr_inputs = PBRInfo(
		NdotL,
		NdotV,
		NdotH,
		LdotH,
		VdotH,
		perceptual_roughness,
		metallic,
		specularEnvironmentR0,
		specularEnvironmentR90,
		alpha_roughness,
		diffuse_color,
		specular_color
	);

	// Calculate the shading terms for the microfacet specular shading model
	vec3 F = SpecularReflection(pbr_inputs);
	float G = GeometricOcclusion(pbr_inputs);
	float D = MicrofacetDistribution(pbr_inputs);

	// Calculation of analytical lighting contribution
	vec3 diffuse_contrib = (1.0 - F) * Diffuse(pbr_inputs);
	vec3 spec_contrib = F * G * D / (4.0 * NdotL * NdotV);
	vec3 color = NdotL * u_light.color * (diffuse_contrib + spec_contrib);

	// Calculate lighting contribution from image based lighting source (IBL)
	color += GetIBLContribution(pbr_inputs, n, reflection);

	// Calculate shadow factor
	float visibility = 1.0 - u_shadow_scale * (1.0 - GetVisibility());
	color *= visibility;

	out_color = vec4(pow(color, vec3(1.0/GAMMA)), base_color.a);
}