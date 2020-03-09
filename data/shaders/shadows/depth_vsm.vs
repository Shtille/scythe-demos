#version 330 core

layout(location = 0) in vec3 a_position;

uniform mat4 u_projection_view;
uniform mat4 u_model;

out vec4 v_position;

void main()
{
	vec4 position_clip = u_projection_view * u_model * vec4(a_position, 1.0);
	v_position = position_clip;
    gl_Position = position_clip;
}