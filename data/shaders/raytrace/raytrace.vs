#version 330 core

layout(location = 0) in vec3 a_position;

out DATA
{
	vec2 uv;
} vs_out;

void main()
{
    vec4 clip_position = vec4(a_position, 1.0);

	vs_out.uv = (clip_position.xy + 1.0) * 0.5;

	gl_Position = clip_position;
}