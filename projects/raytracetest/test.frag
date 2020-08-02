#version AUTOVER
#include "cnovr.glsl"

out vec4 colorOut;

in vec3 localpos;
in vec3 start;
in vec3 rayin;
vec3 ray;

float hit_sphere( vec3 center, float radius ){
    vec3 oc = start - center;
    float a = dot(ray, ray);
    float b = 2.0 * dot(oc, ray);
    float c = dot(oc,oc) - radius*radius;
    float discriminant = b*b - 4*a*c;
    //return (discriminant>0);
    if(discriminant < 0){
        return -1.0;
    }
    else{
        return (-b - sqrt(discriminant)) / (2.0*a);
    }
}

void main()
{
	vec3 drawnorm = vec3( 0., 0., 1. );
	ray = normalize(rayin);
	float minhit = 1000.;
	float x, y, z;
	for( x = 0; x < 4; x++ )
	for( y = 0; y < 4; y++ )
	for( z = 0; z < 4; z++ )
	{
		vec3 spherepos = vec3( x, y, z );

		float hit = hit_sphere( spherepos, .1 );
		if( hit >= 0 && hit < minhit )
		{
			vec3 hitpos = hit * ray + start;
			minhit = hit;
			drawnorm = (hitpos-spherepos)*10;
		}
	}
	if( minhit > 999. ) minhit = -1.;
	colorOut = vec4( (minhit>0)?vec3(drawnorm/2. + .5):vec3(0.), 1.0);
}
