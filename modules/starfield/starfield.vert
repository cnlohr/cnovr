#version 430
#include "cnovr.glsl"

layout(location = 0) in vec4 position;
layout(location = 1) in vec4 extra;

layout(location = 19) uniform vec4 props;

out vec3 localpos;
out vec4 extradata;
out vec3 comprgb;

void main()
{
	float r = 1. / position.z; 

	r *= props.y;

	if( r < 0 ) r *= -1;
	if( r > 90. ) r = 90.;
	vec3 posout = vec3( 
		cos(position.x) * cos(position.y), 
		sin( position.y ),
		sin(position.x)* cos(position.y)
	 ) * r;
	gl_Position = umPerspective * umView * umModel * vec4(posout.xyz,1.0);
	localpos = posout.xyz;
	extradata = extra;

	float mag = 2.5*(15.-extradata.x)/14.;
	gl_PointSize = mag;

	float bv = extradata.y;
	float vi = extradata.x;

	//Color temperature in kelvin
	float t = 4600 * ((1 / ((0.92 * bv) + 1.7)) +(1 / ((0.92 * bv) + 0.62)) );
	// t to xyY
	float x = 0, y = 0;

	if (t>=1667 && t<=4000) {
	  x = ((-0.2661239 * pow(10,9)) / pow(t,3)) + ((-0.2343580 * pow(10,6)) / pow(t,2)) + ((0.8776956 * pow(10,3)) / t) + 0.179910;
	} else if (t > 4000 && t <= 25000) {
	  x = ((-3.0258469 * pow(10,9)) / pow(t,3)) + ((2.1070379 * pow(10,6)) / pow(t,2)) + ((0.2226347 * pow(10,3)) / t) + 0.240390;
	}

	if (t >= 1667 && t <= 2222) {
	  y = -1.1063814 * pow(x,3) - 1.34811020 * pow(x,2) + 2.18555832 * x - 0.20219683;
	} else if (t > 2222 && t <= 4000) {
	  y = -0.9549476 * pow(x,3) - 1.37418593 * pow(x,2) + 2.09137015 * x - 0.16748867;
	} else if (t > 4000 && t <= 25000) {
	  y = 3.0817580 * pow(x,3) - 5.87338670 * pow(x,2) + 3.75112997 * x - 0.37001483;
	}


	float Y = (y == 0)? 0 : 1;
	float X = (y == 0)? 0 : (x * Y) / y;
	float Z = (y == 0)? 0 : ((1 - x - y) * Y) / y;

	comprgb =  vec3(
		 0.41847 * X - 0.15866 * Y - 0.082835 * Z,
		 -0.091169 * X + 0.25243 * Y + 0.015708 * Z,
		0.00092090 * X - 0.0025498 * Y + 0.17860 * Z );
}
