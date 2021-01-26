#version AUTOVER
#include "cnovr.glsl"

out vec4 colorOut;

varying vec2 uv;

uniform vec4 colorbasis; //#MAPUNIFORM colorbasis 19

void main()
{
	colorOut = vec4( abs(uv)*1.0, colorbasis.x, 1.0);
}
