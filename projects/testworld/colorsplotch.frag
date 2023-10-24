#version AUTOVER
#include "cnovr.glsl"

out vec4 colorOut;

in vec2 uv;

uniform vec4 colorbasis; //#MAPUNIFORM colorbasis 19

// All components are in the range [0…1], including hue.
vec3 hsv2rgb(vec3 c)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void main()
{
	int mode = int(floor( colorbasis.x ));
	
	vec2 uvi = uv * colorbasis.zw;
	
	if( mode == 0 )
	{
		colorOut = vec4( colorbasis.y, uvi, 1.0);
	}
	else if( mode == 1 )
	{
		colorOut = vec4( uvi.x, colorbasis.y, uvi.y, 1.0);
	}
	else if( mode == 2 )
	{
		colorOut = vec4( uvi, colorbasis.y, 1.0);
	}
	else if( mode == 3 )
	{
		colorOut = vec4( hsv2rgb( vec3( colorbasis.y, 1.-uvi.x, uvi.y ) ), 1.0);
	}
	else if( mode == 4 )
	{
		colorOut = vec4( hsv2rgb( vec3( uvi.x, 1.-colorbasis.y, uvi.y ) ), 1.0);
	}
	else if( mode == 5 )
	{
		colorOut = vec4( hsv2rgb( vec3( uvi.x, 1.-uvi.y, colorbasis.y ) ), 1.0);
	}
	else if( mode == 6 )
	{
		colorOut = vec4( colorbasis.yyy, 1.0);
	}
}
