#version 430
#include "cnovr.glsl"

out vec4 colorOut;


in vec2 texcoords;
in vec3 position;
in vec3 normal;

uniform vec4 colorprops1; //#MAPUNIFORM colorprops1 22
uniform vec4 colorprops2; //#MAPUNIFORM colorprops2 23

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
	uv.y = 1.-uv.y;
	vec4 yuyv = texture( texi, uv );
	vec2 tp = floor( mod( texcoords.xy*ts*2.0, 2.0 ) );
	return yuvtorgb(( tp.x < 1.0 )?yuyv.rga:yuyv.bga);
}

float greennessfn( vec2 uv )
{
	vec3 video = getyuyvpixel( textures[2], uv );
	vec3 normvideo = mix( normalize(video), video, colorprops1.x );
	vec3 target = colorprops1.yzw;
	float dist = length( target - normvideo )*colorprops2.x*4.0;
	float greenness = (1.0-dist)*colorprops2.y*4.0;
	greenness = clamp( greenness, 0., 1. );
	return greenness;
}

void main()
{
	vec4 texbg = texture( textures[0], texcoords.xy ); 
	vec4 texfg = texture( textures[1], texcoords.xy ); 
	vec3 video = getyuyvpixel( textures[2], texcoords.xy );

	//Yucky - TODO: Perform a single pyramid decimation on the video stream.
	vec3 ivsize = vec3( vec2(1.)/textureSize( textures[2], 0 ), 0. );
	float greenness = max(
		max(
			max( greennessfn( texcoords.xy + ivsize.xz ), greennessfn( texcoords.xy + ivsize.zy ) ),
			max( greennessfn( texcoords.xy - ivsize.xz ), greennessfn( texcoords.xy - ivsize.zy ) )
		),
			greennessfn( texcoords.xy )
		);
	vec3 backandvideo = mix( video.rgb, texbg.rgb, greenness );
	colorOut = vec4( mix( backandvideo, texfg.rgb, clamp( texfg.a*2.0, 0., 1. )), 1.0);
}
