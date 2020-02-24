#version 330
#include "cnovr.glsl"

//This is a weird system for making things that look like GL_LINES with a hard black
//background, but this lets you avoid a second pass with lines after blackout.
//It also may run more performantly on non-Quadro cards.
//
//NOTE: your geometry *must* be loaded with the "barytc" modifier.

in vec4 position; //#MAPATTRIB position 0
in vec4 bary;     //#MAPATTRIB bary 1
in vec3 norm;     //#MAPATTRIB norm 2

out vec4 barytc;
out vec3 normo;


void main()
{
	barytc = bary;
	normo = norm;
	vec4 nppos  = umView * umModel * vec4(position.xyz,1.0);
	gl_Position = umPerspective * nppos;
}
