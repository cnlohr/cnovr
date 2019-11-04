#version 430
#include "cnovr.glsl"

out vec4 colorOut;

in vec4 localcolor;


void main()
{
	colorOut = localcolor;
}
