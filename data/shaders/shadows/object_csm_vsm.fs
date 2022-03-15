#version 330 core

struct Light {
	vec3 color;
	vec3 direction; // in world space
};

out vec4 out_color;

#ifdef USE_CSM
  uniform float u_clip_space_split_distances[NUM_SPLITS];
#endif
uniform Light u_light;
#ifdef USE_CSM
  uniform float u_color_factor;
  uniform sampler2D u_shadow_samplers[NUM_SPLITS];
#else
  uniform sampler2D u_shadow_sampler;
#endif

in DATA
{
	vec3 position;
	vec3 normal;
#ifdef USE_CSM
 	vec4 shadow_coords[NUM_SPLITS];
 	float clip_space_z;
#else
	vec4 shadow_coord;
#endif
} fs_in;

const float kScreenGamma = 2.2; // Assume that monitor is in sRGB color space

#ifdef USE_CSM
int GetCascadeIndex()
{
	for (int i = 0; i < NUM_SPLITS; i++)
	{
		if (fs_in.clip_space_z <= u_clip_space_split_distances[i])
			return i;
	}
	// This shouldn't happen
	return 0;
}
#endif

#ifdef USE_CSM
float ChebyshevUpperBound(vec3 shadow_coord_post_w, int index)
#else
float ChebyshevUpperBound(vec3 shadow_coord_post_w)
#endif
{
	// We retrive the two moments previously stored (depth and depth*depth)
#ifdef USE_CSM
	vec2 moments = texture(u_shadow_samplers[index], shadow_coord_post_w.xy).rg;
#else
	vec2 moments = texture(u_shadow_sampler, shadow_coord_post_w.xy).rg;
#endif
	
	// Surface is fully lit. as the current fragment is before the light occluder
	float distance = shadow_coord_post_w.z;
	// if (distance <= moments.x)
	// 	return 1.0;
	float p = step(distance, moments.x); // x < edge => 0

	// The fragment is either in shadow or penumbra. We now use chebyshev's upperBound to check
	// How likely this pixel is to be lit (p_max)
	float variance = moments.y - (moments.x * moments.x);
	variance = max(variance, 0.00002);

	float d = distance - moments.x;
	float p_max = variance / (variance + d*d);

	return max(p, p_max);
}

#ifdef USE_CSM
float GetVisibility(int index)
#else
float GetVisibility()
#endif
{
#ifdef USE_CSM
	vec4 shadow_coord = fs_in.shadow_coords[index];
#else
	vec4 shadow_coord = fs_in.shadow_coord;
#endif
	vec3 shadow_coord_post_w = shadow_coord.xyz / shadow_coord.w;

	// Check if we outside the shadow map
	if (shadow_coord.w <= 0.0 ||
		(shadow_coord_post_w.x <  0.0 || shadow_coord_post_w.y <  0.0) ||
		(shadow_coord_post_w.x >= 1.0 || shadow_coord_post_w.y >= 1.0))
		return 1.0;

#ifdef USE_CSM
	return ChebyshevUpperBound(shadow_coord_post_w, index);
#else
	return ChebyshevUpperBound(shadow_coord_post_w);
#endif
}

#ifdef USE_CSM
const vec3 s_colors[] = vec3[](
	vec3(1.0, 0.0, 0.0),
	vec3(0.0, 1.0, 0.0),
	vec3(0.0, 0.0, 1.0)
);
#endif

void main()
{
	// Compute ambient term
	vec3 ambient = vec3(0.2);

	// Compute diffuse term
	vec3 normal = normalize(fs_in.normal);
	vec3 diffuse = vec3(0.0);
	vec3 light = normalize(u_light.direction);
	float lambertian = clamp(dot(light, normal), 0.0, 1.0);
	diffuse += lambertian * u_light.color;

	// Shadows factor
	float visibility = 1.0;
#ifdef USE_CSM
	int index = GetCascadeIndex();
	visibility -= 0.8 * (1.0 - GetVisibility(index));
#else
	visibility -= 0.8 * (1.0 - GetVisibility());
#endif

	vec3 color_linear = ambient + (diffuse) * visibility;
	vec3 color_corrected = pow(color_linear, vec3(1.0/kScreenGamma));

#ifdef USE_CSM
	color_corrected = mix(color_corrected, color_corrected * s_colors[index], u_color_factor);
#endif

	out_color = vec4(color_corrected, 1.0);
}