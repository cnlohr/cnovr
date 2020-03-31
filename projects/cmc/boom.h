
cnovr_pose    boomroot; //Must be origin

#define PARTICLEVERTS 1024

cnovr_model * explosion_model;
cnovr_shader * explosion_shader;
float * explosion_points;
float * explosion_data;
float * explosion_color;
float * explosion_extradata;

void EmitParticle( float * pos, float size, float * dir, float * color, float lifetime )
{
	static int exphead;
	float * extra;

	memcpy( explosion_points + exphead*3, pos, sizeof(float)*3 );
	cnovr_point3d halfdir;
	scale3d( halfdir, dir, ((rand()%100)/150.0)+0.2 );
	memcpy( explosion_data + (exphead*4+1), halfdir, sizeof(float)*3 );
	*(explosion_data + (exphead*4+0)) = size;
	memcpy( explosion_color + exphead*4, color, sizeof(float)*4 );
	extra = (explosion_extradata + exphead*4);
	extra[0] = lifetime/5.0; //Current lifetime
	extra[1] = size;
	extra[2] = lifetime;
	extra[3] = 0;

	exphead++;
	memcpy( explosion_points + exphead*3, pos, sizeof(float)*3 );
	memcpy( explosion_data + (exphead*4+1), dir, sizeof(float)*3 );
	*(explosion_data + (exphead*4+0)) = size;
	memcpy( explosion_color + exphead*4, color, sizeof(float)*4 );
	extra = (explosion_extradata + exphead*4);
	extra[0] = lifetime; //Current lifetime
	extra[1] = size;
	extra[2] = lifetime;
	extra[3] = 0;
	exphead++;
	if( exphead == PARTICLEVERTS ) exphead = 0;
}

void Boom( float * pos, int npart, float expand, float lifetime, float * customcolor )
{
	int i;
	for( i = 0; i < npart; i++ )
	{
		cnovr_point3d dir;
		dir[0] = (rand()%102)-50.5;
		dir[1] = (rand()%102)-50.5;
		dir[2] = (rand()%102)-50.5;
		//normalize3d( dir, dir );
		scale3d( dir, dir, 0.4 );
		//^^^^ Ugh buggy compiler
		scale3d( dir, dir, expand );
		float color[4];
		if( customcolor )
		{
			memcpy( color, customcolor, sizeof( color ) );

		}
		else
		{
			color[0] = (rand()%100)+1;
			color[1] = (rand()%100)+1;
			color[2] = (rand()%100)+1;
			color[3] = 1;
			normalize3d( color, color );
		}
		EmitParticle( pos, 20+(rand()%20), dir, color, lifetime );
	}
}

void UpdateExplosionData( float deltatime )
{
    int i;
	for( i = 0; i < PARTICLEVERTS; i++ )
	{
		float * pt = explosion_points + i*3;
		float * dat = explosion_data + i*4;
		float * col = explosion_color + i*4;
		float * ext = explosion_extradata + i*4;
 
		float life = ext[0];
		if( life > 0 )
		{
			life -= deltatime;
		}
		ext[0] = life;
		cnovr_point3d motion;
		scale3d( motion, dat+1, deltatime );
		add3d( pt, motion, pt );
		dat[0] = (life/ext[2])*ext[1];
		//printf( "%f/%f=%f   %f %f %f    %f    %f %f %f %f\n", life, ext[2], dat[0], PFTHREE( pt ), dat[0], PFFOUR(col) );
	}
	CNOVRVBOTaint( explosion_model->pGeos[0] );
	CNOVRVBOTaint( explosion_model->pGeos[1] );
	CNOVRVBOTaint( explosion_model->pGeos[2] );
	CNOVRVBOTaint( explosion_model->pGeos[3] );
}

void RenderExplosions()
{
	CNOVRRender( explosion_shader );
	CNOVRRender( explosion_model );
}

void ExplosionInit()
{
    int i;
	explosion_shader = CNOVRShaderCreate( "ovrball/explosion" );
	explosion_model = CNOVRModelCreate( 0, GL_LINES );
	CNOVRModelSetNumVBOsWithStrides( explosion_model, 4, 3, 4, 4, 4 );
	for( i = 0; i < PARTICLEVERTS; i++ )
	{
		float nil[4] = { 0 };
		CNOVRVBOTackv( explosion_model->pGeos[0], 3, nil );
		CNOVRVBOTackv( explosion_model->pGeos[1], 4, nil );
		CNOVRVBOTackv( explosion_model->pGeos[2], 4, nil );
		CNOVRVBOTackv( explosion_model->pGeos[3], 4, nil );
		CNOVRModelTackIndex( explosion_model, 1, i );
	}
	explosion_points = explosion_model->pGeos[0]->pVertices;
	explosion_data = explosion_model->pGeos[1]->pVertices;
	explosion_color = explosion_model->pGeos[2]->pVertices;
	explosion_extradata = explosion_model->pGeos[3]->pVertices;
	CNOVRVBOTaint( explosion_model->pGeos[0] );
	CNOVRVBOTaint( explosion_model->pGeos[1] );
	CNOVRVBOTaint( explosion_model->pGeos[2] );
	CNOVRVBOTaint( explosion_model->pGeos[3] );
	CNOVRModelTaintIndices( explosion_model );
	pose_make_identity( &boomroot );
	explosion_model->pose = &boomroot;
}
