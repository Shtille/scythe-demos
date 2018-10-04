#version 330 core

layout(location = 0) in vec3 a_position;

uniform mat4 u_projection_view;
uniform mat4 u_model;

void main()
{
	vec4 position_world = u_model * vec4(a_position, 1.0);
    gl_Position = u_projection_view * position_world;
}