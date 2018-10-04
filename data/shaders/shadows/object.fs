#version 330 core

struct Light {
	vec3 position; // in world space
	vec3 color;
};

out vec4 out_color;

uniform Light u_light;
uniform sampler2DShadow u_shadow_sampler;

in DATA
{
	vec3 position;
	vec3 normal;
	vec4 shadow_coord;
} fs_in;

const float kScreenGamma = 2.2; // Assume that monitor is in sRGB color space

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

	float bias = 0.005 * (sqrt(1.0 - lambertian*lambertian)/lambertian);
	bias = clamp(bias, 0.0, 0.01);
	float visibility = 1.0;
	vec4 shadow = fs_in.shadow_coord;
	for (int i = 0; i < 4; ++i)
	{
		int index = int(16.0 * random(gl_FragCoord.xyy, i)) % 16;
		visibility -= 0.2 * (1.0 - texture(u_shadow_sampler, vec3(shadow.xy + poissonDisk[index] / 700.0, shadow.z - bias)/shadow.w));
	}

	vec3 color_linear = ambient + (diffuse) * visibility;
	vec3 color_corrected = pow(color_linear, vec3(1.0/kScreenGamma));

	out_color = vec4(color_corrected, 1.0);
}