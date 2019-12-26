#version 330 core

struct Light {
	vec3 position; // in world space
	vec3 color;
};

out vec4 out_color;

uniform Light u_light;
uniform vec3 u_color; // object color
//uniform sampler2DShadow u_shadow_sampler;

in DATA
{
	vec3 position;
	vec3 normal;
	//vec4 shadow_coord;
} fs_in;

const float kScreenGamma = 2.2; // Assume that monitor is in sRGB color space

void main()
{
	// Compute ambient term
	vec3 ambient = vec3(0.2);

	// Compute diffuse term
	vec3 normal = normalize(fs_in.normal);
	vec3 diffuse = vec3(0.0);
	vec3 light = normalize(u_light.position - fs_in.position);
	float lambertian = clamp(dot(light, normal), 0.0, 1.0);
	diffuse += lambertian * u_light.color * u_color;

	// Compute total color
	vec3 color_linear = ambient + diffuse;
	vec3 color_corrected = pow(color_linear, vec3(1.0/kScreenGamma));

	out_color = vec4(color_corrected, 1.0);
}