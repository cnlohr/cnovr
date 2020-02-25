#version AUTOVER
#include "cnovr.glsl"

in vec4 position;    //#MAPATTRIB position 0
uniform vec4 props;

out vec4 localpos;

void main()
{
	vec3 ppos = position.xyz;
	if( props.y > 0.5 )
	{
		//Hit point
		ppos = vec3( ppos.xyz );
	}
	else
	{
		ppos = vec3( ppos.xy, (ppos.z>0.0)?props.x:0.0 );
	}
	gl_Position = umPerspective * umView * umModel * vec4(ppos,1.0);
	localpos = vec4( ppos, props.y );
}
