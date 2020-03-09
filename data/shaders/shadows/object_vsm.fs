#version 330 core

struct Light {
	vec3 position; // in world space
	vec3 color;
};

out vec4 out_color;

uniform Light u_light;
uniform sampler2D u_shadow_sampler;

in DATA
{
	vec3 position;
	vec3 normal;
	vec4 shadow_coord;
} fs_in;

const float kScreenGamma = 2.2; // Assume that monitor is in sRGB color space
vec4 shadow_coord_post_w;

float ChebyshevUpperBound(float distance)
{
	// We retrive the two moments previously stored (depth and depth*depth)
	vec2 moments = texture(u_shadow_sampler, shadow_coord_post_w.xy).rg;
	
	// Surface is fully lit. as the current fragment is before the light occluder
	if (distance <= moments.x)
		return 1.0;

	// The fragment is either in shadow or penumbra. We now use chebyshev's upperBound to check
	// How likely this pixel is to be lit (p_max)
	float variance = moments.y - (moments.x * moments.x);
	variance = max(variance, 0.00002);

	float d = distance - moments.x;
	float p_max = variance / (variance + d*d);

	return p_max;
}

void main()
{
	// Compute ambient term
	vec3 ambient = vec3(0.2);

	// Compute diffuse term
	vec3 normal = normalize(fs_in.normal);
	vec3 diffuse = vec3(0.0);
	vec3 light = normalize(u_light.position - fs_in.position);
	float lambertian = clamp(dot(light, normal), 0.0, 1.0);
	diffuse += lambertian * u_light.color;

	shadow_coord_post_w = fs_in.shadow_coord / fs_in.shadow_coord.w;
	//float visibility = ChebyshevUpperBound(shadow_coord_post_w.z);
	float visibility = 1.0;
	if (texture(u_shadow_sampler, shadow_coord_post_w.xy).x < shadow_coord_post_w.z)
		visibility = 0.5;

	vec3 color_linear = ambient + (diffuse) * visibility;
	vec3 color_corrected = pow(color_linear, vec3(1.0/kScreenGamma));

	out_color = vec4(color_corrected, 1.0);
}