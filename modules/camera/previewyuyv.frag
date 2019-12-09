#version 430
#include "cnovr.glsl"

out vec4 colorOut;

layout(location = 8) uniform sampler2D textures[];

in vec2 texcoords;

//From Wikipedia
//const vec3 cvr = { 1.402, -.714, 0.0 };
//const vec3 cvb = { 0.0, -.344, 1.772 };
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

void main()
{
	ivec2 ts = textureSize( textures[0], 0 );
	vec2 uv = texcoords;
	//uv = vec2( 1. ) - uv;
	vec4 yuyv = texture( textures[0], uv );
	vec2 tp = floor( mod( texcoords.xy*ts*2.0, 2.0 ) );
	colorOut = vec4( tp.xx, 0.0, 1.0 );
	vec3 yuv = ( tp.x < 1.0 )?yuyv.rga:yuyv.bga;
	colorOut = vec4( yuvtorgb( yuv ), 1. );
}
