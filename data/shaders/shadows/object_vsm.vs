#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;

uniform mat4 u_projection_view;
uniform mat4 u_model;
uniform mat4 u_depth_bias_projection_view;

out DATA
{
	vec3 position;
	vec3 normal;
	vec4 shadow_coord;
} vs_out;

void main()
{
	vec4 position_world = u_model * vec4(a_position, 1.0);
	mat3 model = mat3(u_model);

	vs_out.position = vec3(position_world);
	vs_out.normal = model * a_normal;
	vs_out.shadow_coord = u_depth_bias_projection_view * position_world;

    gl_Position = u_projection_view * position_world;
}