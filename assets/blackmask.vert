#version 430
#include "cnovr.glsl"

//Create black geometry, but pushed ever so slightly further away from the view than normal geometry.
//this can be used for washing a black color over the scene where there's geometry.

in vec4 position; //#MAPATTRIB position 0

varying vec3 localpos;

void main()
{
	gl_Position = umPerspective * umView * umModel * vec4(position.xyz,1.0);
	gl_Position.z *= 1.000001;
}
