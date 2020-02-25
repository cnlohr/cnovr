#version AUTOVER
#include "cnovr.glsl"

//This is a weird system for making things that look like GL_LINES with a hard black
//background, but this lets you avoid a second pass with lines after blackout.
//It also may run more performantly on non-Quadro cards.
//
//NOTE: your geometry *must* be loaded with the "barytc" modifier.

in vec4 position;  //#MAPATTRIB position 0
in vec4 bary;      //#MAPATTRIB bary 1
in vec3 norm;      //#MAPATTRIB norm 2

out vec4 barytc;
out vec3 normo;

uniform vec4 ringanimation;  //#MAPUNIFORM ringanimation 21
uniform vec4 ballspeedathit; //#MAPUNIFORM ballspeedathit 22

void main()
{
	barytc = bary;
	normo = norm;
	float difft = ringanimation.x;
	vec3 epicenter = ringanimation.yzw;
	epicenter += ballspeedathit.xyz * difft; 
	float dist_2d_from_ep = length( epicenter.xz - position.xz );
	float normextrude = -30.*pow( 2.78, -pow(-.1+difft*4.-dist_2d_from_ep*.5, 2.) );
	normextrude = normextrude/(difft*20.+1.);
	vec4 nppos  = umView * umModel * vec4(position.xyz + norm.xyz * normextrude,1.0);
	gl_Position = umPerspective * nppos;
}
