cnovr_canvas * titlecanvas;
cnovr_canvas * scorecanvas;
//float fpstime = 1.;

#define UI_WINDOW_COLOR 0x00004DFF
#define UI_TEXT_COLOR 0x00000000

cnovr_pose * titlecanvas_pose;
cnovr_pose * scorecanvas_pose;
cnovr_pose tcoffset;
cnovr_pose scoffset;

//overballstore_t
void InitUI()
{
	//CANVAS_PROP_NO_INTERACT
	titlecanvas = CNOVRCanvasCreate( "titlecanvas", 260, 152, CANVAS_PROP_NO_INTERACT );
	scorecanvas = CNOVRCanvasCreate( "scorecanvas", 200, 40, CANVAS_PROP_NO_INTERACT );

	titlecanvas_pose = titlecanvas->pose;
	pose_make_identity( &tcoffset );
	tcoffset.Pos[2] = 3.;
	tcoffset.Pos[1] = -1.5;
	cnovr_euler_angle eulera;
	eulera[0] = 0;
	eulera[1] = 0;
	eulera[2] = 3.14159;
	quatfromeuler( tcoffset.Rot, eulera );
	tcoffset.Scale = 1.0;

	scorecanvas_pose = scorecanvas->pose;
	pose_make_identity( &scoffset );
	scoffset.Pos[2] = 3.;
	scoffset.Pos[1] = -1.0;
	eulera[0] = 0;
	eulera[1] = 0;
	eulera[2] = 3.14159;
	quatfromeuler( scoffset.Rot, eulera );
	scoffset.Scale = 1.0;

}

void UpdateUI( float deltatime )
{
	// time calculations
	static double start;
	static double last;
	double now = OGGetAbsoluteTime();
	if( start < 1 ) { start = now; last = now; return; }
	double runtime = now - start;
	last = now;

	//store->points // current points

	//0x0000GGRR
	titlecanvas->bgcolor = UI_WINDOW_COLOR;
	titlecanvas->color = UI_TEXT_COLOR;
	
	scorecanvas->bgcolor = UI_WINDOW_COLOR;
	scorecanvas->color = UI_TEXT_COLOR;
	
	//cnovrfocus_properties * headset_focus = CNOVRFocusGetPropsForDev( int ctrl );
	//titlecanvas->pose = cnovrfocus_properties * CNOVRFocusGetPropsForDev( int ctrl )
	//scorecanvas->pose = 
	
	// Get headset properties 
//	cnovrfocus_properties * p = //CNOVRFocusGetPropsForDev( 0 );
	cnovr_pose * pf = CNOVRFocusGetTipPose( 0 );
	cnovr_pose tcpose_target;
	cnovr_pose scpose_target;

	float * tcpa = (float*)&tcpose_target;
	float * tcpo = (float*)titlecanvas_pose;
	float * scpa = (float*)&scpose_target;
	float * scpo = (float*)scorecanvas_pose;

	apply_pose_to_pose( &tcpose_target, pf, &tcoffset );
	apply_pose_to_pose( &scpose_target, pf, &scoffset );

	scpose_target.Pos[1] = pf->Pos[1] + .5;
	tcpose_target.Pos[1] = pf->Pos[1] + 1;
	int i = 0;
	for( i = 0; i < 8; i++ )
	{
		float slack = .01;
		tcpo[i] = tcpo[i] * (1.-slack) + tcpa[i] * slack;
		scpo[i] = scpo[i] * (1.-slack) + scpa[i] * slack;
	}

//	memcpy( tcpo, tcpa, sizeof( tcpose_target ) );
//	memcpy( scpo, scpa, sizeof( tcpose_target ) );

//	quatcopy( titlecanvas_pose->Rot, pf->Rot );
//	copy3d( titlecanvas_pose->Pos, pf->Pos );
//	titlecanvas_pose->Scale = titlecanvas->pose->Scale;
	

//	titlecanvas->pose = &titlecanvas_pose;
	// TODO: from p->tip, project outwards and at a rotation to the side.
	///printf("%f %f %f\n",PFTHREE(titlecanvas->pose->Pos));
	
	/*
	typedef struct cnovr_pose {
		cnovr_quat Rot;
		cnovr_point3d Pos;
		FLT Scale;
	} cnovr_pose;
	*/

	// clear canvas
	CNOVRCanvasClearFrame( titlecanvas );
	CNOVRCanvasClearFrame( scorecanvas );
	
	CNOVRCanvasSetLineWidth( titlecanvas, 3 );
	CNOVRCanvasSetLineWidth( scorecanvas, 3 );
	
	CNOVRCanvasDrawText( titlecanvas, 2, 4, trprintf( "  COSMIC\n MISCREANT\nCONDITIONER" ), 8 );
	CNOVRCanvasDrawText( scorecanvas, 2, 4, trprintf( "SCORE: %04d", store->points ), 6 );
	
	CNOVRCanvasSwapBuffers( titlecanvas );
	CNOVRCanvasSwapBuffers( scorecanvas );
}

void RenderUI()
{
	CNOVRRender( titlecanvas );
	CNOVRRender( scorecanvas );
}
