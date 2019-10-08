#version 430
#include "cnovr.glsl"

layout(location = 0) in vec4 position;
layout(location = 1) in vec2 texcoordsin;
layout(location = 2) in vec3 normalin;
layout(location = 3) in vec4 extra;

out vec3 localpos;
out vec2 texcoords;
out vec3 localnorm;
out vec4 extradata;

void main()
{
	gl_Position = umPerspective * umView * umModel * vec4(position.xyz,1.0);
	localpos = position.xyz;
	texcoords = texcoordsin.xy;
	localnorm = normalin.xyz;
	extradata = extra;
}
