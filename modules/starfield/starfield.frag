#version 430
#include "cnovr.glsl"

layout(location = 19) uniform vec4 props;

out vec4 colorOut;

in vec3 localpos;
in vec4 extradata;
in vec3 comprgb;

//uint16_t magnitude_mag1000;
//int16_t bvcolor_mag1000;
//int16_t vicolor_mag1000;

layout(location = 8) uniform sampler2D textures[];

void main()
{
	//float mag = (14.-extradata.x)/14.;

	float mag = 5./extradata.x; //pow( extradata.x, 10. );

	if( mag < .3 ) mag = .3;

	vec3 rgb;
	if( extradata.w < -2. )
	{
		rgb = extradata.rgb;
	}
	else
	{
		if( props.x < 0.5 )
		{
			//Lines
			mag *= 0.5;
			rgb = mix( normalize( comprgb ), vec3(1.), .7 );
			if( mag > 0.6 ) mag = 0.6;
		}
		else
		{
			//stars
			rgb = mix( normalize( comprgb ), vec3(1.), .4 );
		}
	}
	colorOut = vec4( rgb, 1.0 ) * mag;
	//if( gl_FragDepth > 0.999 ) gl_FragDepth = 0.999;
}
