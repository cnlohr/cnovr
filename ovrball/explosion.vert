#version 430
#include "cnovr.glsl"

layout(location = 0) in vec4 position;
layout(location = 1) in vec4 data;
layout(location = 2) in vec4 color;

out vec4 localcolor;

void main()
{
	gl_Position = umPerspective * umView * umModel * vec4(position.xyz,1.0);
	localcolor = color;
	localcolor.a = data.x/4.0;
/*
	if( data.x <= 0 || data.x > 30 || data.x != data.x )
	{
		gl_PointSize = 0.0;
		gl_Position.z = 10;
	}
	else
	{
		gl_PointSize = data.x;
	}
*/
}
