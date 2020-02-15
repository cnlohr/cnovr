
#include "cnovrmath.h"

#if 0
cnovr_pose enemy_pose(FLT time, int enemyId, int type)
{
    if (type == 1)
    {
        FLT MAX_RADIUS = 100;
        FLT START_DEPTH = 150;
        FLT END_DEPTH = 40;
        int NUM_SPINS = 5;      // how many orbits to make before done
        int SPIN_DURATION = 10; // seconds per orbit

        // rotation is position in overall enemy cycle,
        FLT rotation = FLT_MOD(time, SPIN_DURATION * NUM_SPINS);
        FLT spin = FLT_MOD(rotation, SPIN_DURATION);
        // cnovr_pose:
        // cnovr_quat Rot;
        // cnovr_point3d Pos;
        // FLT Scale;
        FLT x = MAX_RADIUS * spin * FLT_COS(rotation);
        FLT y = MAX_RADIUS * spin * FLT_SIN(rotation);
        FLT z = cnovr_lerp(START_DEPTH, END_DEPTH, spin / (SPIN_DURATION * MAX_SPINS));

        cnovr_point3d up = {0, 0, 1};
        cnovr_quat orientation;
        quatfromaxisangle(orientation, {x, y, z}, spin);

        return {.Pos = {x, y, z}, .Rot = orientation, .Scale = 1};
    }
}
#endif
