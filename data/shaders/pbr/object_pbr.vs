#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texcoord;
layout(location = 3) in vec3 a_tangent;
layout(location = 4) in vec3 a_binormal;

uniform mat4 u_projection_view;
uniform mat4 u_model;
uniform mat4 u_depth_bias_projection_view;

out DATA
{
	vec3 position;
	mat3 tbn;
	vec2 uv;
	vec4 shadow_coord;
} vs_out;

void main()
{
	vec4 position_world = u_model * vec4(a_position, 1.0);

	mat3 model = mat3(u_model);
	vec3 normal = model * a_normal;
	vec3 binormal = model * a_binormal;
	vec3 tangent = model * a_tangent;

	vs_out.position = vec3(position_world);
	vs_out.tbn = mat3(tangent, binormal, normal);
	vs_out.uv = a_texcoord;
	vs_out.shadow_coord = u_depth_bias_projection_view * position_world;

	gl_Position = u_projection_view * position_world;
}