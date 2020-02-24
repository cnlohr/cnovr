#version 430
#include "cnovr.glsl"

in vec4 position;    //#MAPATTRIB position 0
in vec2 texcoordsin; //#MAPATTRIB texcoordsin 1
in vec3 normalin;    //#MAPATTRIB normalin 2
in vec4 extra;       //#MAPATTRIB extra 3

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
