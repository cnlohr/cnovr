#version AUTOVER
#include "cnovr.glsl"

out vec4 colorOut;

in vec4 localpos;

uniform vec4 props;  //#MAPUNIFORM props 19

void main()
{
	if( localpos.w > 0.5 ) 
	{
		colorOut = vec4( vec3 (
			(mod( length( floor( mod( localpos.xyz * 200.0 + vec3( .5 ), vec3(2.0) ) ) ), 1.3 ) > 0.9)?1.0:0.0
		), 1.0);
		return;
	}
	else
	{
		//Debug colors
		//colorOut = vec4( localpos.xyz * 50.0, 1.0 );
		float z = localpos.z;
		//Straight Z
		//colorOut = vec4( abs(localpos.xyz)*1.0, 1.0);

		//Ruler
		if( props.z >= 0.0 )
		{
			colorOut = vec4(
				.4,
				(mod( z, 1.0 ) < 0.01)?1.0:0.0,
				(mod( z, 0.1 ) < 0.01)?1.0:0.0,
				1.0);
		}
		else
		{
			colorOut = vec4(
				(mod( z, 1.0 ) < 0.01)?1.0:0.0,
				.4,
				.4,
				1.0);
		}

	}
}
