#version AUTOVER
#include "cnovr.glsl"

in vec4 position; //#MAPATTRIB position 0
in vec2 uvAttrib;   	  //#MAPATTRIB uv 1

varying vec2 uv;

void main()
{
	gl_Position = umPerspective * umView * umModel * vec4(position.xyz,1.0);
	uv = uvAttrib;
}
