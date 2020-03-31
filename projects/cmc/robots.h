
#include "cnovrmath.h"

#define MAX_ROBOTS 256

struct robot
{
	int enabled;
	float time_offset;
	float time_since_spawn;
	cnovr_pose pose;
	float time_to_shoot;
};

cnovr_model * robotmodels[1];

struct robot robots[MAX_ROBOTS];

void enemy_pose(cnovr_pose *pose, FLT time, int enemyId, int type)
{
    if (type == 1)
    {
        FLT MAX_RADIUS = 1;
        FLT START_DEPTH = 7;
        FLT END_DEPTH = 40;
        int NUM_SPINS = 5;      // how many orbits to make before done
        int SPIN_DURATION = 10; // seconds per orbit
        int MAX_ENEMIES = 256.;

        // rotation is position in overall enemy cycle,
        FLT rotation = FLT_MOD(time, SPIN_DURATION * NUM_SPINS);
        FLT spin = FLT_MOD(rotation, SPIN_DURATION);
        // cnovr_pose:
        // cnovr_quat Rot;
        // cnovr_point3d Pos;
        // FLT Scale;
        FLT x = MAX_RADIUS * spin * FLT_COS((CNOVRPI * 2 * enemyId / MAX_ENEMIES) + rotation);
        FLT y = MAX_RADIUS * spin * FLT_SIN((CNOVRPI * 2 * enemyId / MAX_ENEMIES) + rotation);
        FLT z = -cnovr_lerp(START_DEPTH, END_DEPTH, spin / (SPIN_DURATION * NUM_SPINS));

        cnovr_point3d up = {0, 0, 1};
        cnovr_quat orientation;
        cnovr_point3d pos = {x, y, z};
        quatfromaxisangle(orientation, pos, spin);
        copy3d( pose->Pos, pos);
        quatcopy( pose->Rot, orientation);
        pose->Scale = 1;
    }
}

void UpdateRobots( float dtime )
{
	int i;
	for( i = 0; i < MAX_ROBOTS; i++ )
	{
		struct robot * r = robots + i;
		if( !r->enabled ) continue;
		r->time_to_shoot -= dtime;
		r->time_since_spawn += dtime;
		enemy_pose( &r->pose, r->time_since_spawn + r->time_offset, i, 1 );
		if( r->time_to_shoot < 0 )
		{
			EmitProjectile( &r->pose, 40, .1, i );
			r->time_to_shoot = (rand()%10)*.02 ;
		}
	}
}

void RenderRobots()
{
	int i;
	for( i = 0; i < MAX_ROBOTS; i++ )
	{
		struct robot * r = robots + i;
		if( !r->enabled ) continue;
	//	printf( "%f %f %f\n", PFTHREE( r->pose.Pos ) );
		CNOVRModelRenderWithPose( robotmodels[0], &r->pose );
	}
}

void InitRobots()
{
	int i;
	for( i = 0; i < MAX_ROBOTS; i++ )
	{
		struct robot * r = robots + i;
		r->time_since_spawn = 0;
		r->time_offset = i*0.5;
		r->enabled = 1;
		r->time_to_shoot = (rand()%10)*.2 + 1;
	}

	robotmodels[0] = CNOVRModelCreate( 0, GL_TRIANGLES );
	robotmodels[0]->pose = 0;
	CNOVRModelLoadFromFileAsync( robotmodels[0], "doomba.obj:barytc" );
}

