#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texcoord;
#ifdef USE_TANGENT
 layout(location = 3) in vec3 a_tangent;
 layout(location = 4) in vec3 a_binormal;
#endif

uniform mat4 u_projection_view;
uniform mat4 u_model;
#ifdef USE_SHADOW
 #ifdef USE_CSM
  uniform mat4 u_depth_bias_projection_view[NUM_SPLITS];
 #else
  uniform mat4 u_depth_bias_projection_view;
 #endif
#endif

out DATA
{
	vec3 position;
#ifdef USE_TANGENT
	mat3 tbn;
#else
	vec3 normal;
#endif
	vec2 uv;
#ifdef USE_SHADOW
 #ifdef USE_CSM
 	vec4 shadow_coords[NUM_SPLITS];
 	float clip_space_z;
 #else
	vec4 shadow_coord;
 #endif
#endif
} vs_out;

void main()
{
	vec4 position_world = u_model * vec4(a_position, 1.0);

	mat3 model = mat3(u_model);
	vec3 normal = model * a_normal;
#ifdef USE_TANGENT
	vec3 binormal = model * a_binormal;
	vec3 tangent = model * a_tangent;
#endif

	vs_out.position = vec3(position_world);
#ifdef USE_TANGENT
	vs_out.tbn = mat3(tangent, binormal, normal);
#else
	vs_out.normal = normal;
#endif
	vs_out.uv = a_texcoord;
#ifdef USE_SHADOW
 #ifdef USE_CSM
 	for (int i = 0; i < NUM_SPLITS; i++)
 		vs_out.shadow_coords[i] = u_depth_bias_projection_view[i] * position_world;
 #else
	vs_out.shadow_coord = u_depth_bias_projection_view * position_world;
 #endif
#endif

	gl_Position = u_projection_view * position_world;

#ifdef USE_CSM
	vs_out.clip_space_z = gl_Position.z;
#endif
}