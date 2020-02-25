#version AUTOVER
#include "cnovr.glsl"

in vec4 barytc;
in vec3 normo;
out vec4 colorOut;

const float extrathickness = 0.5;
const float sharpness = 1.0;

void main()
{
	vec3 barytcdiff = abs( dFdx(barytc.xyz) ) + abs( dFdy(barytc.xyz) );
	vec3 barycomp = barytc.xyz / barytcdiff;
	float baryo = min( min( barycomp.x, barycomp.y ), barycomp.z );
	baryo = 1. - baryo;
	baryo = ( baryo + extrathickness ) * sharpness;
	baryo = clamp( baryo, 0.0, 1.0 );
	colorOut = vec4( mix( vec3( 0. ), abs(normo), baryo ), 1.0);
}
