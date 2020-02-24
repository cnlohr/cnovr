#version 430
#include "cnovr.glsl"

in vec3 positionin;  //#MAPATTRIB positionin 0
in vec2 texcoordsin; //#MAPATTRIB texcoordsin 1
in vec3 normalin;    //#MAPATTRIB normalin 2
in vec4 extradata;   //#MAPATTRIB extradata 3


out vec2 texcoords;
out vec3 normal;
out vec3 position;
out vec4 extradataout;

void main()
{
	gl_Position = umPerspective * umView * umModel * vec4(positionin.xyz,1.0);
	texcoords = texcoordsin.xy;
	normal = normalin.xyz;
	position = positionin.xyz * vec3( 2.0, 0.7, 0.0 );
	extradataout = extradata;
}
