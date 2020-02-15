#version 430
#include "cnovr.glsl"

layout(location = 0) in vec3 positionin;
layout(location = 1) in vec2 texcoordsin;
layout(location = 2) in vec3 normalin;

out vec2 texcoords;

void main()
{
	gl_Position = umPerspective * umView * umModel * vec4(positionin.xyz,1.0);
	texcoords = texcoordsin.xy;
}
