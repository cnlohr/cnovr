#define NUM_PROJECTILE_VERTS 1024
#define NUM_PROJECTILES (NUM_PROJECTILE_VERTS/2)

cnovr_model * projectiles_model;
cnovr_shader * projectiles_shader;
float * projectiles_points;
float * projectiles_data;
float * projectiles_color;
float * projectiles_extradata;

cnovr_pose    projectiles_root; //Must be origin

int collision_head;

struct projectile
{
	int enabled;
	int originid;
	cnovr_point3d color;
	cnovr_point3d start;
	cnovr_point3d position;
	cnovr_point3d velocity;
	float length;
	float pewlen;
};

struct projectile projectiles[NUM_PROJECTILES];

void UpdateProjectiles( float dtime )
{
	struct projectile * p;
	int i;
	for( i = 0; i < NUM_PROJECTILES; i++ )
	{
		p = &projectiles[i];
		if( !p->enabled ) continue;

		int r;
		for( r = 0; r < MAX_ROBOTS; r++ )
		{
			struct robot * rbt = &robots[r];
			if( r == p->originid ) continue; //Robots can't hit themselves.
			if( !rbt->enabled ) continue;
			cnovr_point3d distance;
			sub3d( distance, p->position, rbt->pose.Pos );
			if( magnitude3d( distance ) < .3 )
			{
				//Explode robot (we have contact)
				Boom( rbt->pose.Pos, 100, .2, 2, 0);
				rbt->enabled = false;
				p->enabled = false;
				store->points++;
				break;
			}			
		}
		cnovr_point3d posdiv;
		scale3d( posdiv, p->velocity, dtime );
		add3d( p->position, p->position, posdiv );

		int mk;
		for( mk = 0; mk < 2; mk++ )
		{
			int j = mk + i*2;
			//printf( "%d %f %f %f\n", j, PFTHREE( p->position ) );
			cnovr_point3d prf;
			normalize3d( prf, p->velocity );
			if( mk == 0 )
			{
				projectiles_points[0+j*3] = p->position[0];
				projectiles_points[1+j*3] = p->position[1];
				projectiles_points[2+j*3] = p->position[2];
			}
			else
			{
				projectiles_points[0+j*3] = p->position[0]-prf[0];
				projectiles_points[1+j*3] = p->position[1]-prf[1];
				projectiles_points[2+j*3] = p->position[2]-prf[2];
			}
			projectiles_data[0+j*4] = p->enabled?10.:0.0;
			projectiles_data[1+j*4] = 10.;
			projectiles_data[2+j*4] = 10.;
			projectiles_data[3+j*4] = 10.;
			projectiles_color[0+j*4] = p->color[0];
			projectiles_color[1+j*4] = p->color[1];
			projectiles_color[2+j*4] = p->color[2];
			projectiles_color[3+j*4] = 1;
			projectiles_extradata[0+j*4] = 0;
			projectiles_extradata[1+j*4] = 0;
			projectiles_extradata[2+j*4] = 0;
			projectiles_extradata[3+j*4] = 0;
		}
	}

	CNOVRVBOTaint( projectiles_model->pGeos[0] );
	CNOVRVBOTaint( projectiles_model->pGeos[1] );
	CNOVRVBOTaint( projectiles_model->pGeos[2] );
	CNOVRVBOTaint( projectiles_model->pGeos[3] );
}

void EmitProjectile( cnovr_pose * pemit, float velocity, float pewlen, int origin )
{
	struct projectile * p = &projectiles[collision_head];
	copy3d( p->start, pemit->Pos );
	cnovr_point3d unacc_pos = { 0, 0, -velocity };
	quatrotatevector( p->velocity, pemit->Rot, unacc_pos);
	copy3d( p->position, p->start );
	p->enabled = 1;
	p->color[0] = (rand()%100)+1;
	p->color[1] = (rand()%100)+1;
	p->color[2] = (rand()%100)+1;
	p->pewlen = pewlen;
	p->originid = origin;
	normalize3d( p->color, p->color );
	//printf ("%f %f %f -> %f %f %f %d\n", PFTHREE( p->position ), PFTHREE( p->velocity ), collision_head );
	collision_head = (collision_head+1)%NUM_PROJECTILES;
}

void RenderProjectiles()
{
	CNOVRRender( projectiles_shader );
	CNOVRRender( projectiles_model );
}

void InitProjectiles()
{
	store->points = 0;
    int i;
	projectiles_shader = CNOVRShaderCreate( "ovrball/explosion" );
	projectiles_model = CNOVRModelCreate( 0, GL_LINES );
	CNOVRModelSetNumVBOsWithStrides( projectiles_model, 4, 3, 4, 4, 4 );
	for( i = 0; i < NUM_PROJECTILE_VERTS; i++ )
	{
		float nil[4] = { 0 };
		CNOVRVBOTackv( projectiles_model->pGeos[0], 3, nil );
		CNOVRVBOTackv( projectiles_model->pGeos[1], 4, nil );
		CNOVRVBOTackv( projectiles_model->pGeos[2], 4, nil );
		CNOVRVBOTackv( projectiles_model->pGeos[3], 4, nil );
		CNOVRModelTackIndex( projectiles_model, 1, i );
	}
	projectiles_points = projectiles_model->pGeos[0]->pVertices;
	projectiles_data = projectiles_model->pGeos[1]->pVertices;
	projectiles_color = projectiles_model->pGeos[2]->pVertices;
	projectiles_extradata = projectiles_model->pGeos[3]->pVertices;
	CNOVRVBOTaint( projectiles_model->pGeos[0] );
	CNOVRVBOTaint( projectiles_model->pGeos[1] );
	CNOVRVBOTaint( projectiles_model->pGeos[2] );
	CNOVRVBOTaint( projectiles_model->pGeos[3] );
	CNOVRModelTaintIndices( projectiles_model );
	pose_make_identity( &projectiles_root );
	projectiles_model->pose = &projectiles_root;
	printf( "PROJECTILES WERE INITTED\n" );
}



