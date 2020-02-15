cnovr_model * projectiles_model;
cnovr_shader * projectiles_shader;

cnovr_pose    projectiles_root; //Must be origin


struct projectile
{
	cnovr_point3d start;
	cnovr_point3d current;
	cnovr_point3d direction;
	float length;
};

void UpdateProjectiles( float dtime )
{
}

void RenderProjectiles()
{
}

void InitProjectiles()
{
}



