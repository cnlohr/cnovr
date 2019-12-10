#version 430
#include "cnovr.glsl"

out vec4 colorOut;

layout(location = 8) uniform sampler2D textures[];
layout(location = 9) uniform vec4 osdtarget;

in vec2 texcoords;

//From Wikipedia
const vec3 cvr = { 1.402, -.714, 0.0 };
const vec3 cvb = { 0.0, -.344, 1.772 };

vec3 yuvtorgb( vec3 yuv )
{
	vec3 ret;
	yuv.x = yuv.x - (1./16.);
	yuv.yz = yuv.yz-.5;//*2.0 - 1.0;
	ret = vec3( yuv.x ) + (vec3( yuv.z ) *cvr) + (vec3( yuv.y) * cvb);
	return clamp( ret, 0., 1. );
}

vec3 getyuyvpixel( sampler2D texi, vec2 uv )
{
	ivec2 ts = textureSize( texi, 0 );
	//uv = vec2( 1. ) - uv;
	vec4 yuyv = texture( texi, uv );
	vec2 tp = floor( mod( texcoords.xy*ts*2.0, 2.0 ) );
	return yuvtorgb(( tp.x < 1.0 )?yuyv.rga:yuyv.bga);
}

void main()
{
	vec2 uv = texcoords;

	if( osdtarget.x > 0.5 )
	{
		if( length( (uv - osdtarget.yz)*vec2(1.0,0.5) ) < 0.1 && ( length( uv.x - osdtarget.y ) < 0.0005 || length( uv.y - osdtarget.z ) < 0.001 )  )
		{
			colorOut = vec4( 1., 0., 1., 1. );
			return;
		}
	}
	vec3 rgbpix = getyuyvpixel( textures[0], uv );
	colorOut = vec4( rgbpix, 1. );
}
